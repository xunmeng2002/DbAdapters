#pragma once
#include <stdint.h>

namespace DbAdapters
{
enum class DbOperateType : int32_t
{
    CreateTables = 0,
    DropTables = 1,
    TruncateTables = 2,
    Insert = 3,
    Delete = 4,
    DeleteByIndex = 5,
    Update = 6,
    BatchInsert = 7,
    Truncate = 8,
};

enum class DbTypeType : int32_t
{
    DuckDb = 0,
    SqliteDb = 1,
    MysqlDb = 2,
    MariaDb = 3,
};
}
