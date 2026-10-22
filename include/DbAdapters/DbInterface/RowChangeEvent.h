#pragma once
#include <DbAdapters/DbInterface/RecordHandle.h>
#include <cstdint>
#include <memory>
#include <utility>


namespace DbAdapters
{
enum class RowChangeType : std::uint8_t
{
	Insert = 0,
	Update = 1,
	Erase = 2,
	EraseByIndex = 3,
	Truncate = 4
};

// 行变更事件：发布后不可变，全部订阅者共享同一份。
// Record 归事件所有（Insert 是内存表活记录的拷贝，Update / Erase 是离开表的原记录），
// 最后一个引用释放时记录回对象池。订阅者按需保留 shared_ptr，不得改写 Record。
struct RowChangeEvent
{
	RowChangeEvent(unsigned int eventTableId, RowChangeType eventType, unsigned int eventIndexId,
		std::uint64_t eventSequence, bool persistDatabase, RecordHandle eventRecord)
		:TableId(eventTableId), IndexId(eventIndexId), Sequence(eventSequence),
		Type(eventType), PersistDatabase(persistDatabase), Record(std::move(eventRecord))
	{
	}

	unsigned int TableId;
	unsigned int IndexId;
	std::uint64_t Sequence;
	RowChangeType Type;
	// 对应内存表 Update 的 updateDB 参数：false 表示这条变更不落库，订阅者仍照常收到
	bool PersistDatabase;
	RecordHandle Record;
};

using RowChangeEventPtr = std::shared_ptr<const RowChangeEvent>;
}
