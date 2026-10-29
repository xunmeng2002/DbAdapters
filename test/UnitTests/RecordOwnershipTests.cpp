#include "RecordOwnershipTestSupport.h"
#include "RecordingDb.h"

#include <DbAdapters/AsyncDbWriter/AsyncDbWriter.h>
#include <DbAdapters/DbInterface/RecordHandle.h>
#include <DbAdapters/DbInterface/RowChangeEvent.h>

#include "doctest/doctest.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

using namespace DbAdapters;
using namespace DbAdapters::RecordOwnershipTest;

namespace
{
// 行变更一律以不可变事件发布：写出侧只认事件，记录归属挂在事件的句柄上
RowChangeEventPtr MakeInsertEvent(unsigned int tableId, RecordHandle record, std::uint64_t sequence = 1)
{
    return std::make_shared<const RowChangeEvent>(tableId, RowChangeType::Insert, 0, sequence, true, std::move(record));
}

class WriterHarness
{
public:
    explicit WriterHarness(bool ConnectSucceeds = true)
        : probeSchemas_{ &OwnershipProbeRecord::GetSchema() },
          schemaRegistry_(probeSchemas_, 1),
          recorder_(nullptr),
          writer_(nullptr)
    {
        // 借用点放在交出所有权之前: writer 构造后 Db 已归写线程, 不再对外交出指针
        std::unique_ptr<RecordingDb> ownedRecorder = std::make_unique<RecordingDb>();
        recorder_ = ownedRecorder.get();
        writer_ = std::make_unique<AsyncDbWriter>(std::move(ownedRecorder), &schemaRegistry_);
        Recorder().ConnectSucceeds = ConnectSucceeds;
        writer_->Start();
    }
    // 表的同步状态由快照协议建立；归属测试只关心记录句柄，故跳过快照直接置就绪
    void MarkProbeTableReady() { writer_->OnSnapshotComplete(0); }
    ~WriterHarness()
    {
        writer_->Stop();
        writer_->Join();
    }
    WriterHarness(const WriterHarness&) = delete;
    WriterHarness& operator=(const WriterHarness&) = delete;

    RecordingDb& Recorder() { return *recorder_; }
    AsyncDbWriter& Writer() { return *writer_; }

private:
    const TableSchema* probeSchemas_[1];
    StaticSchemaRegistry schemaRegistry_;
    RecordingDb* recorder_;
    std::unique_ptr<AsyncDbWriter> writer_;
};
}

TEST_SUITE("RecordOwnership")
{
TEST_CASE("移交恰好归还一次")
{
    WriterHarness harness;
    harness.MarkProbeTableReady();
    std::atomic<int> releaseCount{ 0 };
    harness.Writer().OnRowChange(MakeInsertEvent(0, AdoptRecord(AllocateProbeRecord(releaseCount, 2))));
    CHECK(WaitUntil([&] { return releaseCount.load() == 1; }, 3000));
}

TEST_CASE("批元素逐个恰好归还一次")
{
    WriterHarness harness;
    harness.MarkProbeTableReady();
    std::atomic<int> releaseCount{ 0 };
    OwnershipProbeRecord* firstRecord = AllocateProbeRecord(releaseCount, 20);
    OwnershipProbeRecord* middleRecord = AllocateProbeRecord(releaseCount, 21);
    OwnershipProbeRecord* lastRecord = AllocateProbeRecord(releaseCount, 22);
    std::vector<RowChangeEventPtr> batchEvents;
    batchEvents.push_back(MakeInsertEvent(0, AdoptRecord(firstRecord), 1));
    batchEvents.push_back(MakeInsertEvent(0, AdoptRecord(middleRecord), 2));
    batchEvents.push_back(MakeInsertEvent(0, AdoptRecord(lastRecord), 3));
    harness.Writer().OnRowBatchChange(batchEvents);
    batchEvents.clear();   // 批已由写库端持有；本函数这一份放手后才看得到归还
    REQUIRE(WaitUntil([&] { return harness.Recorder().BatchInsertCount.load() >= 1; }, 3000));
    CHECK(WaitUntil([&] { return releaseCount.load() == 3; }, 3000));
    CHECK(harness.Recorder().LastBatchRecordCount == 3);
    CHECK(harness.Recorder().LastBatchFirstRecord == firstRecord);
    CHECK(harness.Recorder().LastBatchLastRecord == lastRecord);
}

TEST_CASE("执行抛异常仍恰好归还一次")
{
    WriterHarness harness;
    harness.MarkProbeTableReady();
    harness.Recorder().InsertThrows = true;
    std::atomic<int> releaseCount{ 0 };
    harness.Writer().OnRowChange(MakeInsertEvent(0, AdoptRecord(AllocateProbeRecord(releaseCount, 30))));
    CHECK(WaitUntil([&] { return releaseCount.load() == 1; }, 15000));
    CHECK(harness.Recorder().DisconnectCount.load() >= 1);
}

TEST_CASE("断开丢弃待办时恰好归还一次")
{
    WriterHarness harness(false);
    harness.MarkProbeTableReady();
    std::atomic<int> releaseCount{ 0 };
    harness.Writer().OnRowChange(MakeInsertEvent(0, AdoptRecord(AllocateProbeRecord(releaseCount, 40))));
    harness.Writer().OnRowChange(MakeInsertEvent(0, AdoptRecord(AllocateProbeRecord(releaseCount, 41))));
    harness.Writer().DisConnect();
    CHECK(WaitUntil([&] { return releaseCount.load() == 2; }, 3000));
}

TEST_CASE("析构抽干待办队列并归还记录")
{
    std::atomic<int> releaseCount{ 0 };
    {
        WriterHarness harness(false);
        harness.MarkProbeTableReady();
        harness.Writer().OnRowChange(MakeInsertEvent(0, AdoptRecord(AllocateProbeRecord(releaseCount, 33))));
        CHECK(releaseCount.load() == 0);
    }
    CHECK(releaseCount.load() == 1);
}

TEST_CASE("断连后写线程自行重连")
{
    WriterHarness harness;
    REQUIRE(WaitUntil([&] { return harness.Recorder().ConnectCount.load() >= 1; }, 3000));
    const int connectCountBeforeDisconnect = harness.Recorder().ConnectCount.load();
    harness.Writer().DisConnect();
    CHECK(harness.Recorder().DisconnectCount.load() >= 1);
    CHECK(WaitUntil([&] { return harness.Recorder().ConnectCount.load() > connectCountBeforeDisconnect; }, 3000));
}

TEST_CASE("断连期间增量就地丢弃")
{
    WriterHarness harness;
    REQUIRE(WaitUntil([&] { return harness.Recorder().ConnectCount.load() >= 1; }, 3000));
    harness.MarkProbeTableReady();
    harness.Writer().DisConnect();
    std::atomic<int> releaseCount{ 0 };
    harness.Writer().OnRowChange(MakeInsertEvent(0, AdoptRecord(AllocateProbeRecord(releaseCount, 50))));
    CHECK(releaseCount.load() == 1);
    CHECK(harness.Writer().PendingOperateCount() == 0);
    CHECK(harness.Recorder().InsertCount.load() == 0);
}

TEST_CASE("快照批量在同步期间不被丢弃")
{
    WriterHarness harness;
    harness.Writer().OnSnapshotBegin(0);
    std::atomic<int> releaseCount{ 0 };
    std::vector<RowChangeEventPtr> batchEvents;
    batchEvents.push_back(MakeInsertEvent(0, AdoptRecord(AllocateProbeRecord(releaseCount, 60))));
    harness.Writer().OnRowBatchChange(batchEvents);
    batchEvents.clear();   // 批已由写库端持有；本函数这一份放手后才看得到归还
    REQUIRE(WaitUntil([&] { return harness.Recorder().BatchInsertCount.load() >= 1; }, 3000));
    CHECK(harness.Recorder().TruncateTableCount.load() >= 1);
    CHECK(harness.Recorder().LastBatchRecordCount == 1);
    CHECK(WaitUntil([&] { return releaseCount.load() == 1; }, 3000));
}
}
