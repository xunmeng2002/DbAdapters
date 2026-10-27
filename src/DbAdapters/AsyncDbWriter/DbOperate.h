#pragma once
#include <DbAdapters/DbInterface/RowChangeEvent.h>
#include <DbAdapters/Types.h>

namespace DbAdapters
{
class DbOperate
{
public:
	virtual ~DbOperate() = default;

	virtual void Deallocate() = 0;

	DbOperateType Operate = DbOperateType::Insert;
	unsigned int TableId = 0;
	unsigned int IndexId = 0;
	// 行变更操作携带的事件；记录与序号都归事件所有，操作本身不持有记录
	RowChangeEventPtr Event;
};
}
