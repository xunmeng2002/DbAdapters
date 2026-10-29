#include "BenchWriteThroughput.h"

#include <DbAdapters/DuckdbWrapper/DuckdbWrapper.h>

using namespace DbAdapters;

int main()
{
    DuckdbWrapper database(":memory:");
    return WriteBenchmark::RunWriteThroughputBenchmark("duckdb", database) ? 0 : 1;
}
