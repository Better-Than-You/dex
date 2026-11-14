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
