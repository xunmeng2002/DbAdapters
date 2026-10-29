#include "BenchWriteThroughput.h"

#include <DbAdapters/SqliteWrapper/SqliteWrapper.h>

using namespace DbAdapters;

int main()
{
    SqliteWrapper database(":memory:");
    return WriteBenchmark::RunWriteThroughputBenchmark("sqlite", database) ? 0 : 1;
}
