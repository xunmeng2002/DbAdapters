# 可执行文件用 Bench 前缀，与 Test* 一起在 bin/<Config> 排在一处。
# 计时器复用 Spark 的公开头 <Spark/TemplateLib/Benchmark/BenchmarkRunner.h>；
# BenchSqlite / BenchDuckdb 共用同级目录的 BenchWriteThroughput.h。
add_application(BenchAsyncWriter    ${CMAKE_CURRENT_SOURCE_DIR}/BenchAsyncWriter ${CMAKE_CURRENT_SOURCE_DIR}/BenchAsyncWriter)
add_application(BenchSqlite         ${CMAKE_CURRENT_SOURCE_DIR}/BenchSqlite ${CMAKE_CURRENT_SOURCE_DIR}/BenchSqlite)
add_application(BenchDuckdb         ${CMAKE_CURRENT_SOURCE_DIR}/BenchDuckdb ${CMAKE_CURRENT_SOURCE_DIR}/BenchDuckdb)

target_include_directories(BenchAsyncWriter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Common)
target_include_directories(BenchSqlite PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(BenchDuckdb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(BenchAsyncWriter PRIVATE Spark::Core AsyncDbWriter)
target_link_libraries(BenchSqlite PRIVATE Spark::Core SqliteWrapper)
target_link_libraries(BenchDuckdb PRIVATE Spark::Core DuckdbWrapper)

foreach(benchmarkTarget BenchAsyncWriter BenchSqlite BenchDuckdb)
    if(WIN32)
        copy_dll_file(${benchmarkTarget})
    endif()
endforeach()
