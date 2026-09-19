add_library(market-data
    src/CBarData.cpp
    src/CBarData.h
    src/market-data.h
)

target_include_directories(market-data PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(market-data PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
)

if(MYAPP_ENABLE_CLANG_TIDY)
    set_target_properties(market-data PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--quiet;-extra-arg=-Wno-unknown-warning-option"
    )
endif()
