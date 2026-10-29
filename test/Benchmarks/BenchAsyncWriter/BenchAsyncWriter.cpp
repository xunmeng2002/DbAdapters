#include "RecordOwnershipTestSupport.h"
#include "RecordingDb.h"

#include <DbAdapters/AsyncDbWriter/AsyncDbWriter.h>
#include <DbAdapters/DbInterface/RecordHandle.h>
#include <DbAdapters/DbInterface/RowChangeEvent.h>
#include <Spark/TemplateLib/Benchmark/BenchmarkRunner.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

using namespace DbAdapters;
using namespace DbAdapters::RecordOwnershipTest;
namespace Benchmark = Spark::Benchmark;

namespace
{
constexpr unsigned int BenchTableId = 0;
constexpr int DrainTimeoutMilliseconds = 5000;
constexpr int DrainTimeoutCheckInterval = 4096;

// 自旋等待替身后端计数前进：一轮写完才返回，队列不会随测量轮次增长。
// 超时直接失败退出——挂住的基准比一个好看的数字更有价值。
template <typename Predicate>
void SpinUntilDrained(Predicate predicate, const char* benchmarkName)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(DrainTimeoutMilliseconds);
    int spinCount = 0;
    while (!predicate())
    {
        if (++spinCount >= DrainTimeoutCheckInterval)
        {
            spinCount = 0;
            if (std::chrono::steady_clock::now() > deadline)
            {
                std::fprintf(stderr, "%s: pipeline did not drain within %d ms\n", benchmarkName, DrainTimeoutMilliseconds);
                std::exit(1);
            }
        }
        std::this_thread::yield();
    }
}

RowChangeEventPtr MakeInsertEvent(unsigned int indexId, std::uint64_t sequence)
{
    OwnershipProbeRecord* record = OwnershipProbeRecord::Allocate();
    record->PK = static_cast<int>(sequence);
    return std::make_shared<const RowChangeEvent>(BenchTableId, RowChangeType::Insert, indexId, sequence, true, AdoptRecord(record));
}

// 串起「记录构造 → 事件发布 → 入队 → 写线程取走 → 后端写入 → 记录归还」整条链路。
// 替身后端只记账，因此测到的是 DbAdapters 自身的开销，不含任何真实数据库的 IO。
class AsyncWriterPipeline
{
public:
    AsyncWriterPipeline() : probeSchemas_{ &OwnershipProbeRecord::GetSchema() }, schemaRegistry_(probeSchemas_, 1)
    {
        // 借用点放在交出所有权之前：构造之后 Db 归写库线程所有
        std::unique_ptr<RecordingDb> ownedRecorder = std::make_unique<RecordingDb>();
        recorder_ = ownedRecorder.get();
        writer_ = std::make_unique<AsyncDbWriter>(std::move(ownedRecorder), &schemaRegistry_);
        writer_->Start();
        // 同步状态由快照协议建立；基准只测写路径，直接置就绪
        writer_->OnSnapshotComplete(BenchTableId);
    }
    ~AsyncWriterPipeline()
    {
        writer_->Stop();
        writer_->Join();
    }
    AsyncWriterPipeline(const AsyncWriterPipeline&) = delete;
    AsyncWriterPipeline& operator=(const AsyncWriterPipeline&) = delete;

    std::size_t SubmitSingleRow()
    {
        const std::uint64_t expectedInsertCount = singleRowSubmittedCount_ + 1;
        writer_->OnRowChange(MakeInsertEvent(0, expectedInsertCount));
        SpinUntilDrained([this, expectedInsertCount] { return static_cast<std::uint64_t>(recorder_->InsertCount.load()) >= expectedInsertCount; },
                         "Pipeline.single_row");
        singleRowSubmittedCount_ = expectedInsertCount;
        return 1;
    }

    std::size_t SubmitBatch(int batchSize)
    {
        std::vector<RowChangeEventPtr> batchEvents;
        batchEvents.reserve(static_cast<std::size_t>(batchSize));
        for (int batchIndex = 0; batchIndex < batchSize; ++batchIndex)
        {
            batchEvents.push_back(MakeInsertEvent(static_cast<unsigned int>(batchIndex), ++eventSequence_));
        }
        const int expectedBatchCount = submittedBatchCount_ + 1;
        writer_->OnRowBatchChange(batchEvents);
        SpinUntilDrained([this, expectedBatchCount] { return recorder_->BatchInsertCount.load() >= expectedBatchCount; }, "Pipeline.batch");
        submittedBatchCount_ = expectedBatchCount;
        return static_cast<std::size_t>(batchSize);
    }

private:
    const TableSchema* probeSchemas_[1];
    StaticSchemaRegistry schemaRegistry_;
    RecordingDb* recorder_;
    std::unique_ptr<AsyncDbWriter> writer_;
    std::uint64_t singleRowSubmittedCount_ = 0;
    std::uint64_t eventSequence_ = 0;
    int submittedBatchCount_ = 0;
};
}

// 数值只在 Release 下有意义；Debug 构建会由 BenchmarkRunner 在表头前打印警告。
// 每行输出 name,ns_per_call,ns_slowest,calls_per_round：表内 ns_per_call 是"一次往返"的耗时，
// 批处理行的单记录成本按 batchSize 相除。
int main()
{
    Benchmark::PrintHeader();

    Benchmark::Measure("RowChangeEvent.make_shared",
                       []()
                       {
                           const RowChangeEventPtr event = MakeInsertEvent(0, 1);
                           return static_cast<std::size_t>(event->Sequence);
                       });

    AsyncWriterPipeline pipeline;
    Benchmark::Measure("Pipeline.single_row", [&pipeline]() { return pipeline.SubmitSingleRow(); });
    Benchmark::Measure("Pipeline.batch.1", [&pipeline]() { return pipeline.SubmitBatch(1); });
    Benchmark::Measure("Pipeline.batch.10", [&pipeline]() { return pipeline.SubmitBatch(10); });
    Benchmark::Measure("Pipeline.batch.100", [&pipeline]() { return pipeline.SubmitBatch(100); });
    Benchmark::Measure("Pipeline.batch.1000", [&pipeline]() { return pipeline.SubmitBatch(1000); });
    return 0;
}
