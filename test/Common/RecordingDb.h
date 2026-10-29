#pragma once

#include <DbAdapters/DbInterface/Db.h>

#include <atomic>
#include <stdexcept>

namespace DbAdapters
{
namespace RecordOwnershipTest
{
// 记录调用次数并可按需抛出后端的替身: 计数在同一线程内先写数据再自增, 观察方据自增后的
// acquire 语义即可安全读到前面写的数据.
class RecordingDb : public Db
{
public:
    bool Connect() override
    {
        ConnectCount.fetch_add(1);
        return ConnectSucceeds.load();
    }
    void DisConnect() override
    {
        DisconnectCount.fetch_add(1);
    }
    void Exec(const char*) override {}
    void CreateTable(const TableSchema*) override {}
    void DropTable(const char*) override {}
    void TruncateTable(const char*) override
    {
        TruncateTableCount.fetch_add(1);
    }
    void CreateTables(const TableSchema* const*, int) override {}
    void DropTables(const TableSchema* const*, int) override {}
    void TruncateTables(const TableSchema* const*, int) override {}
    void Insert(const TableSchema*, const void* record) override
    {
        InsertedRecord = record;
        InsertCount.fetch_add(1);
        if (InsertThrows.load())
        {
            throw std::runtime_error("RecordingDb: Insert throws by request");
        }
    }
    void BatchInsert(const TableSchema*, const void* const* records, int count) override
    {
        LastBatchRecordCount = count;
        LastBatchFirstRecord = count > 0 ? records[0] : nullptr;
        LastBatchLastRecord = count > 0 ? records[count - 1] : nullptr;
        BatchInsertCount.fetch_add(1);
        if (BatchInsertThrows.load())
        {
            throw std::runtime_error("RecordingDb: BatchInsert throws by request");
        }
    }
    void Update(const TableSchema*, const void*) override
    {
        UpdateCount.fetch_add(1);
    }
    void Delete(const TableSchema*, const void*, const int*, int) override
    {
        DeleteCount.fetch_add(1);
    }
    void SelectAll(const TableSchema*, void*, const RecordFactory&) override {}
    void SelectWithSql(const char*, const TableSchema*, void*, const RecordFactory&) override {}

    std::atomic<bool> ConnectSucceeds{ true };
    std::atomic<bool> InsertThrows{ false };
    std::atomic<bool> BatchInsertThrows{ false };
    std::atomic<int> ConnectCount{ 0 };
    std::atomic<int> DisconnectCount{ 0 };
    std::atomic<int> InsertCount{ 0 };
    std::atomic<int> BatchInsertCount{ 0 };
    std::atomic<int> UpdateCount{ 0 };
    std::atomic<int> DeleteCount{ 0 };
    std::atomic<int> TruncateTableCount{ 0 };
    const void* InsertedRecord = nullptr;
    const void* LastBatchFirstRecord = nullptr;
    const void* LastBatchLastRecord = nullptr;
    int LastBatchRecordCount = 0;
};
}
}
