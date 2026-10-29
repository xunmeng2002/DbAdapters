
add_application(TestDB         ${CMAKE_CURRENT_SOURCE_DIR}/TestDB ${CMAKE_CURRENT_SOURCE_DIR}/TestDB)
target_include_directories(TestDB PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Common)
target_link_libraries(TestDB PRIVATE Spark::Core AsyncDbWriter BackendLoaderStatic DuckdbWrapper SqliteWrapper MysqlWrapper MariadbWrapper)
if(WIN32)
    copy_dll_file(TestDB)
endif()

add_application(UnitTests      ${CMAKE_CURRENT_SOURCE_DIR}/UnitTests ${CMAKE_CURRENT_SOURCE_DIR}/UnitTests)
target_include_directories(UnitTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Common)
target_link_libraries(UnitTests PRIVATE doctest::doctest Spark::Core AsyncDbWriter)
if(WIN32)
    copy_dll_file(UnitTests)
endif()

# 基准随测试一并构建；其数值只在 Release 下有效，Debug 运行会自报警告
option(BUILD_BENCHMARKS "Build benchmarks (Release build gives meaningful numbers)" ON)
if(BUILD_BENCHMARKS)
    add_subdirectory(Benchmarks)
endif()
