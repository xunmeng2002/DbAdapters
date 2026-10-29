#pragma once

#include <DbAdapters/DbInterface/Db.h>
#include <DbAdapters/DbInterface/Schema.h>
#include <DbAdapters/DbInterface/TypedTable.h>
#include <Spark/TemplateLib/Benchmark/BenchmarkRunner.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace DbAdapters::WriteBenchmark
{
constexpr double TargetSampleNanoseconds = 50'000'000.0;
constexpr std::size_t MinRecordsPerSample = 1000;
constexpr std::size_t MaxRecordsPerSample = 200'000;
constexpr int SampleCount = 5;

// 混合类型的一行：比单 Int32 主键更接近真实表，测到的绑定与列类型覆盖也更完整
class WriteProbeRecord
{
public:
    static WriteProbeRecord* Allocate()
    {
        return new WriteProbeRecord();
    }
    void Deallocate()
    {
        delete this;
    }
    static const TableSchema& GetSchema();

    std::int32_t PK;
    std::int64_t Quantity;
    double Price;
    char Symbol[16];
    bool Active;
};

inline const TableSchema& WriteProbeRecord::GetSchema()
{
    static const FieldDescriptor fields[] = {
        {"PK", FieldType::Int32, offsetof(WriteProbeRecord, PK), 0},
        {"Quantity", FieldType::Int64, offsetof(WriteProbeRecord, Quantity), 0},
        {"Price", FieldType::Double, offsetof(WriteProbeRecord, Price), 0},
        {"Symbol", FieldType::Char, offsetof(WriteProbeRecord, Symbol), sizeof(WriteProbeRecord::Symbol)},
        {"Active", FieldType::Bool, offsetof(WriteProbeRecord, Active), 0},
    };
    static const int primaryKeyIndices[] = { 0 };
    static const TableSchema schema = { "t_write_bench", fields, 5, primaryKeyIndices, 1, nullptr, 0 };
    return schema;
}

inline void FillProbeRecords(std::vector<WriteProbeRecord>& records)
{
    for (std::size_t recordIndex = 0; recordIndex < records.size(); ++recordIndex)
    {
        WriteProbeRecord& record = records[recordIndex];
        record.PK = static_cast<std::int32_t>(recordIndex + 1);
        record.Quantity = static_cast<std::int64_t>(recordIndex) * 100;
        record.Price = static_cast<double>(recordIndex % 1000) * 0.01;
        std::snprintf(record.Symbol, sizeof(record.Symbol), "SYM%04u", static_cast<unsigned int>(recordIndex % 10000));
        record.Active = recordIndex % 2 == 0;
    }
}

inline std::size_t RoundUpToBatch(std::size_t recordCount, int batchSize)
{
    const std::size_t batchRecordCount = static_cast<std::size_t>(batchSize);
    return (recordCount + batchRecordCount - 1) / batchRecordCount * batchRecordCount;
}

// batchSize 为 1 走单条 Insert（每次自动提交），大于 1 走 BatchInsert（整批一个事务）：
// 两条入口的差异正是基准要回答的问题
inline double TimeInsertNanosecondsPerRecord(Db& db, const TableSchema& schema, const std::vector<const void*>& recordPointers,
                                             std::size_t recordCount, int batchSize)
{
    const auto beginTime = std::chrono::steady_clock::now();
    if (batchSize == 1)
    {
        for (std::size_t recordIndex = 0; recordIndex < recordCount; ++recordIndex)
        {
            db.Insert(&schema, recordPointers[recordIndex]);
        }
    }
    else
    {
        for (std::size_t offset = 0; offset < recordCount; offset += static_cast<std::size_t>(batchSize))
        {
            db.BatchInsert(&schema, &recordPointers[offset], batchSize);
        }
    }
    const double elapsedNanoseconds = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - beginTime).count();
    return elapsedNanoseconds / static_cast<double>(recordCount);
}

// 写入口不返回成败：库句柄为空、表不存在、主键冲突都只记日志。基准必须自己数一遍行，
// 否则"一条都没写进去"会得到一个非常漂亮的数字
inline bool VerifyStoredRowCount(const char* backendName, Db& db, std::size_t expectedRowCount)
{
    TypedTable<WriteProbeRecord> table(&db);
    std::vector<WriteProbeRecord*> storedRecords;
    table.SelectAll(storedRecords);
    const bool rowCountMatched = storedRecords.size() == expectedRowCount;
    if (!rowCountMatched)
    {
        std::fprintf(stderr, "%s: expected %zu stored rows, found %zu\n", backendName, expectedRowCount, storedRecords.size());
    }
    for (WriteProbeRecord* record : storedRecords)
    {
        record->Deallocate();
    }
    return rowCountMatched;
}

// 每个档位先探测单记录成本，再把样本行数凑到 ~50 ms：固定行数时快库一轮只有几毫秒、
// 慢库一轮要跑几十秒。每轮取样前清表，保证各样本面对的行数相同
inline bool RunWriteThroughputBenchmark(const char* backendName, Db& db)
{
    Spark::Benchmark::WarnIfUnoptimized();
    if (!db.Connect())
    {
        std::fprintf(stderr, "%s: connect failed\n", backendName);
        return false;
    }
    const TableSchema& schema = WriteProbeRecord::GetSchema();
    db.CreateTable(&schema);

    std::vector<WriteProbeRecord> records(MaxRecordsPerSample);
    FillProbeRecords(records);
    std::vector<const void*> recordPointers(MaxRecordsPerSample);
    for (std::size_t recordIndex = 0; recordIndex < MaxRecordsPerSample; ++recordIndex)
    {
        recordPointers[recordIndex] = &records[recordIndex];
    }

    std::printf("# backend,batch_size,ns_per_record,ns_slowest,records_per_sample\n");
    const int batchSizes[] = { 1, 10, 100, 1000 };
    for (int batchSize : batchSizes)
    {
        db.TruncateTable(schema.tableName);
        const std::size_t probeRecordCount = RoundUpToBatch(std::max(MinRecordsPerSample, static_cast<std::size_t>(batchSize)), batchSize);
        const double probeNanoseconds = TimeInsertNanosecondsPerRecord(db, schema, recordPointers, probeRecordCount, batchSize);
        if (!VerifyStoredRowCount(backendName, db, probeRecordCount))
        {
            return false;
        }

        const double targetRecords = probeNanoseconds > 0.0 ? TargetSampleNanoseconds / probeNanoseconds
                                                            : static_cast<double>(MaxRecordsPerSample);
        const std::size_t recordsPerSample = RoundUpToBatch(
            static_cast<std::size_t>(std::clamp(targetRecords, static_cast<double>(MinRecordsPerSample), static_cast<double>(MaxRecordsPerSample))),
            batchSize);

        db.TruncateTable(schema.tableName);
        const std::size_t warmupRecordCount = RoundUpToBatch(MinRecordsPerSample, batchSize);
        TimeInsertNanosecondsPerRecord(db, schema, recordPointers, warmupRecordCount, batchSize);
        if (!VerifyStoredRowCount(backendName, db, warmupRecordCount))
        {
            return false;
        }

        double samples[SampleCount];
        for (int sampleIndex = 0; sampleIndex < SampleCount; ++sampleIndex)
        {
            db.TruncateTable(schema.tableName);
            samples[sampleIndex] = TimeInsertNanosecondsPerRecord(db, schema, recordPointers, recordsPerSample, batchSize);
        }
        if (!VerifyStoredRowCount(backendName, db, recordsPerSample))
        {
            return false;
        }

        std::sort(samples, samples + SampleCount);
        std::printf("%s,%d,%.1f,%.1f,%zu\n", backendName, batchSize, samples[SampleCount / 2], samples[SampleCount - 1], recordsPerSample);
    }
    return true;
}
}
