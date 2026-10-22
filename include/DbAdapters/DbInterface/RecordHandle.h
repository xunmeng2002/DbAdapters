#pragma once

namespace DbAdapters
{
//契约：归还回调不得抛异常——RecordHandle 析构为 noexcept，违约直接 std::terminate
using ReleaseRecord = void (*)(void*);

class RecordHandle
{
public:
    RecordHandle() noexcept = default;
    RecordHandle(void* record, ReleaseRecord releaseRecord) noexcept
        : record_(record), releaseRecord_(releaseRecord)
    {
    }
    ~RecordHandle() noexcept { Reset(); }

    RecordHandle(RecordHandle&& other) noexcept
        : record_(other.record_), releaseRecord_(other.releaseRecord_)
    {
        other.record_ = nullptr;
        other.releaseRecord_ = nullptr;
    }
    RecordHandle& operator=(RecordHandle&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            record_ = other.record_;
            releaseRecord_ = other.releaseRecord_;
            other.record_ = nullptr;
            other.releaseRecord_ = nullptr;
        }
        return *this;
    }
    RecordHandle(const RecordHandle&) = delete;
    RecordHandle& operator=(const RecordHandle&) = delete;

    const void* Get() const noexcept { return record_; }

    void Reset() noexcept
    {
        if (releaseRecord_ != nullptr && record_ != nullptr)
        {
            releaseRecord_(record_);
        }
        record_ = nullptr;
        releaseRecord_ = nullptr;
    }

private:
    void* record_ = nullptr;
    ReleaseRecord releaseRecord_ = nullptr;
};

template <typename Record>
RecordHandle AdoptRecord(Record* record) noexcept
{
    return RecordHandle(record, [](void* rawRecord) { static_cast<Record*>(rawRecord)->Deallocate(); });
}
}
