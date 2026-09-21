add_library(myapp_sqlite3 STATIC
    ${CMAKE_SOURCE_DIR}/deps/sqlite/sqlite3.c
)
target_include_directories(myapp_sqlite3 PRIVATE ${CMAKE_SOURCE_DIR}/deps/sqlite)
target_compile_definitions(myapp_sqlite3 PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_DQS=0
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_USE_URI=1
)
set_source_files_properties(${CMAKE_SOURCE_DIR}/deps/sqlite/sqlite3.c PROPERTIES SKIP_LINTING ON)
set_target_properties(myapp_sqlite3 PROPERTIES
    C_CLANG_TIDY ""
    CXX_CLANG_TIDY ""
)
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(myapp_sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/schema/v1.sql" MARKET_DATA_SCHEMA_V1)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/market_data/schema_v1.inc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/schema_v1.inc"
    @ONLY
)

add_library(market-data STATIC
    src/market_data/SqliteVersion.cpp
    private/Sqlite.cpp
    src/market_data/Schema.cpp
    src/market_data/Store.cpp
    src/market_data/Time.cpp
    src/market_data/MboumMap.cpp
)
target_include_directories(market-data
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
    PRIVATE ${CMAKE_SOURCE_DIR}/deps/sqlite
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(market-data PRIVATE myapp_sqlite3)
if(UNIX AND NOT APPLE)
    target_link_libraries(market-data PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
target_compile_features(market-data PUBLIC cxx_std_20)
target_compile_options(market-data PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
)

if(MYAPP_ENABLE_CLANG_TIDY)
    set_target_properties(market-data PROPERTIES
        CXX_CLANG_TIDY "${MYAPP_CLANG_TIDY}"
    )
endif()

if(CYN_TESTING)
    add_executable(market_data_tests
        tests/test_main.cpp
        ${CMAKE_SOURCE_DIR}/deps/catch/catch_amalgamated.cpp
    )
    target_link_libraries(market_data_tests PRIVATE market-data)
    target_include_directories(market_data_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/deps/catch
    )
    target_compile_definitions(market_data_tests PRIVATE
        MYAPP_MARKET_DATA_SCHEMA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/schema"
    )
    target_compile_options(market_data_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
    )
    add_test(NAME market_data_tests COMMAND market_data_tests)
    if(MYAPP_ENABLE_CLANG_TIDY)
        set_target_properties(market_data_tests PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--quiet;-extra-arg=-Wno-unknown-warning-option"
        )
        set_source_files_properties(
            ${CMAKE_SOURCE_DIR}/deps/catch/catch_amalgamated.cpp
            PROPERTIES SKIP_LINTING ON
        )
    endif()
endif()
