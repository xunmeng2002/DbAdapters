#pragma once
#include "DbOperate.h"

#include <utility>
#include <vector>


namespace DbAdapters
{
class DbOperateImpl : public DbOperate
{
public:
    static DbOperateImpl* Allocate();

    virtual void Deallocate() override;

    const std::vector<RowChangeEventPtr>& GetBatchEvents() const { return batchEvents_; }
    void SetBatchEvents(std::vector<RowChangeEventPtr> events) { batchEvents_ = std::move(events); }

private:
    std::vector<RowChangeEventPtr> batchEvents_;
};
}
