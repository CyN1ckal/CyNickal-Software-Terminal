find_package(CURL REQUIRED)

add_executable(ingest
    src/main.cpp
)

target_link_libraries(ingest PRIVATE market-data CURL::libcurl)
target_compile_features(ingest PUBLIC cxx_std_20)
target_compile_options(ingest PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
)

if(MYAPP_ENABLE_CLANG_TIDY)
    set_target_properties(ingest PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--quiet;-extra-arg=-Wno-unknown-warning-option"
    )
endif()
