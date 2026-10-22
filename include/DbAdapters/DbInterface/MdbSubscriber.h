#pragma once
#include <DbAdapters/DbInterface/RecordHandle.h>
#include <DbAdapters/DbInterface/RowChangeEvent.h>
#include <Spark/Types.h>
#include <atomic>
#include <vector>


namespace DbAdapters
{
// 行变更订阅者：无条件接收全部行变更，数据库连接状态不影响投递
class MdbSubscriber
{
public:
	MdbSubscriber()
		:connected_(false)
	{
	}
	virtual ~MdbSubscriber() = default;

	virtual void OnRowChange(const RowChangeEventPtr& event) {}
	virtual void OnRowBatchChange(const std::vector<RowChangeEventPtr>& events) {}

public:
	std::atomic<bool> connected_;
};

// 副本订阅者：在行变更之外追加同步控制（建表 / 删表 / 快照），全库至多一个，
// 用 Mdb::SubscribeReplica 注册，不进入普通订阅者列表，避免重复投递
class MdbReplicaSubscriber : public MdbSubscriber
{
public:
	virtual void OnTableOp(DbOperateType op) {}
	// 快照开始：接收方清掉目标表的旧数据
	virtual void OnSnapshotBegin(unsigned int tableId) {}
	// 快照完成：接收方在自己的同步原语内置就绪位，此后该表的行变更才继续入队
	virtual void OnSnapshotComplete(unsigned int tableId) {}
};
}
