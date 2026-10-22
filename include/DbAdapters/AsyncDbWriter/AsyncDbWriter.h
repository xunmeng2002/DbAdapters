#pragma once
#include <DbAdapters/AsyncDbWriter/AsyncDbWriterExport.h>
#include <DbAdapters/DbInterface/MdbSubscriber.h>
#include <DbAdapters/DbInterface/DbSubscriber.h>
#include <DbAdapters/DbInterface/Db.h>
#include <DbAdapters/DbInterface/FailureLogThrottle.h>
#include <DbAdapters/DbInterface/RecordHandle.h>
#include <DbAdapters/DbInterface/RowChangeEvent.h>
#include <DbAdapters/DbInterface/SchemaRegistry.h>
#include <Spark/Core/Core.h>
#include <cstdint>
#include <list>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace DbAdapters
{
class DbOperate;

// 每张表的同步状态：断连丢弃增量；快照期间接收（清表与批量必须进队）；就绪后正常接收
enum class TableSyncState : std::uint8_t
{
	Disconnected = 0,
	Resyncing = 1,
	Ready = 2
};

class ASYNCDBWRITER_EXPORTS AsyncDbWriter : public Spark::Core::ThreadBase, public MdbReplicaSubscriber
{
public:
	AsyncDbWriter(Db* db, SchemaRegistry* schemaRegistry);
	~AsyncDbWriter();
	void Subscribe(DbSubscriber* dbSubscriber);
	Db* GetDb();
	bool Connect();
	void DisConnect();
	// 待写操作数：断连期间恒为 0，非 0 即"已入队但还没提交给后端"
	int PendingOperateCount();

	virtual void OnRowChange(const RowChangeEventPtr& event) override;
	virtual void OnRowBatchChange(const std::vector<RowChangeEventPtr>& events) override;
	virtual void OnTableOp(DbOperateType op) override;
	virtual void OnSnapshotBegin(unsigned int tableId) override;
	virtual void OnSnapshotComplete(unsigned int tableId) override;

protected:
	virtual void Run() override;
	virtual void ThreadExit() override;
	void CheckConnect();
	void CheckDbOperate();
	void HandleDbOperate();
	DbOperate* GetDbOperate();

private:
	void AddDbOperate(DbOperate* dbOperate);
	void AddDbOperateIfReady(unsigned int tableId, DbOperate* dbOperate);
	DbOperate* CreateDbOperate(DbOperateType operate, unsigned int tableId);
	DbOperate* CreateRowChangeOperate(const RowChangeEventPtr& event);
	void DropPendingOperates(const char* triggerReason);
	bool IsTableSyncableLocked(unsigned int tableId) const;
	void SetTableSyncStateLocked(unsigned int tableId, TableSyncState state);

	void ExecuteDbOperate(DbOperate* dbOperate);
	void CreateTables(DbOperate* dbOperate);
	void DropTables(DbOperate* dbOperate);
	void TruncateTables(DbOperate* dbOperate);
	void InsertRecord(DbOperate* dbOperate);
	void DeleteRecord(DbOperate* dbOperate);
	void DeleteRecordByIndex(DbOperate* dbOperate);
	void UpdateRecord(DbOperate* dbOperate);
	void BatchInsertRecords(DbOperate* dbOperate);
	void TruncateTable(DbOperate* dbOperate);

	// 连接失败按固定重试次数节流上报：每轮 Run 都会重试一次连接，不节流会按 timeOut_ 频率刷屏
	static constexpr int connectFailureReportInterval_ = 100;
	Db* db_;
	SchemaRegistry* schemaRegistry_;
	DbSubscriber* dbSubscriber_;
	std::list<DbOperate*> dbOperates_;
	std::mutex mutex_;
	std::condition_variable conditionVariable_;
	FailureLogThrottle connectFailureLogThrottle_{ connectFailureReportInterval_ };
	// 每张表的同步状态，下标即 TableId；按需扩容，未出现的 ID 即 Disconnected。受 mutex_ 保护。
	// 与 dbOperates_ 共用一把锁：查状态 + 入队、清状态 + 换队列各在同一次加锁内，断连瞬间不会漏出幽灵操作
	std::vector<std::uint8_t> syncStates_;
};
}
