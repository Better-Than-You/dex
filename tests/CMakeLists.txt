add_library(dex_test_support INTERFACE)
target_link_libraries(dex_test_support INTERFACE dex_core Qt6::Test)

function(dex_add_test target)
    qt_add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE dex_test_support)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

dex_add_test(tst_normalizer)
dex_add_test(tst_math)
dex_add_test(tst_router)
dex_add_test(tst_config)
dex_add_test(tst_engine)
dex_add_test(tst_ipc)
dex_add_test(tst_provider)
dex_add_test(tst_audio)

# Live microphone test: needs a real input device plus the model bundles, so
# it soft-skips when either is missing. Label it so it can be excluded with
# `ctest -LE live`.
dex_add_test(tst_live_input)
set_tests_properties(tst_live_input PROPERTIES
    LABELS live
    TIMEOUT 120
)

target_compile_definitions(tst_router PRIVATE
    DEX_COMMANDS_TOML="${CMAKE_SOURCE_DIR}/config/commands.toml"
)
target_compile_definitions(tst_config PRIVATE
    DEX_CONFIG_TOML="${CMAKE_SOURCE_DIR}/config/config.toml"
    DEX_COMMANDS_TOML="${CMAKE_SOURCE_DIR}/config/commands.toml"
)
target_compile_definitions(tst_engine PRIVATE
    DEX_COMMANDS_TOML="${CMAKE_SOURCE_DIR}/config/commands.toml"
)
