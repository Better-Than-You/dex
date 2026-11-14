cmake_minimum_required(VERSION 3.21)

project(Dex VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia Network Test)
find_package(LayerShellQt REQUIRED)
find_package(tomlplusplus REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(SherpaOnnx REQUIRED)
find_package(LlamaCpp QUIET)

qt_standard_project_setup()

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

# ---------------------------------------------------------------------------
# dex_core: deterministic engine, config, IPC and transcript providers.
# No QML dependency so the whole engine is unit-testable in isolation.
# ---------------------------------------------------------------------------
add_library(dex_core STATIC
    src/core/Config.hpp
    src/core/Config.cpp
    src/core/IpcServer.hpp
    src/core/IpcServer.cpp
    src/engine/TextNormalizer.hpp
    src/engine/TextNormalizer.cpp
    src/engine/MathEvaluator.hpp
    src/engine/MathEvaluator.cpp
    src/engine/SymbolicDifferentiator.hpp
    src/engine/SymbolicDifferentiator.cpp
    src/engine/LlamaMathTranslator.hpp
    src/engine/LlamaMathTranslator.cpp
    src/engine/RuleRouter.hpp
    src/engine/RuleRouter.cpp
    src/engine/ActionDispatcher.hpp
    src/engine/ActionDispatcher.cpp
    src/engine/CommandEngine.hpp
    src/engine/CommandEngine.cpp
    src/provider/ITranscriptProvider.hpp
    src/provider/StubTranscriptProvider.hpp
    src/provider/StubTranscriptProvider.cpp
    src/provider/SpscRingBuffer.hpp
    src/provider/AudioCapture.hpp
    src/provider/AudioCapture.cpp
    src/provider/ModelLayout.hpp
    src/provider/ModelLayout.cpp
    src/provider/SherpaTranscriptProvider.hpp
    src/provider/SherpaTranscriptProvider.cpp
    src/provider/ProviderFactory.hpp
    src/provider/ProviderFactory.cpp
)

target_include_directories(dex_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(dex_core PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Multimedia
    tomlplusplus::tomlplusplus
    SherpaOnnx::SherpaOnnx
)
target_compile_features(dex_core PUBLIC cxx_std_20)

if(LlamaCpp_FOUND)
    target_link_libraries(dex_core PRIVATE LlamaCpp::LlamaCpp)
    target_compile_definitions(dex_core PRIVATE DEX_HAVE_LLAMA=1)
    message(STATUS "Dex: maths SLM enabled (llama.cpp: ${LlamaCpp_LIBRARY})")
else()
    message(STATUS "Dex: llama.cpp not found - maths SLM disabled, deterministic maths only")
endif()

# ---------------------------------------------------------------------------
# dex executable + QML module (URI Dex)
# ---------------------------------------------------------------------------
qt_add_executable(dex src/main.cpp)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(dex
    URI Dex
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/components/StatusPill.qml
        qml/components/MathResultCard.qml
        qml/components/ConfirmBar.qml
    SOURCES
        src/core/AppController.hpp
        src/core/AppController.cpp
)

target_link_libraries(dex PRIVATE
    dex_core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    LayerShellQt::Interface
)

# qmltyperegistrar emits `#include <AppController.hpp>`.
target_include_directories(dex PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core)

target_compile_definitions(dex PRIVATE
    DEX_SOURCE_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/config"
)

install(TARGETS dex RUNTIME DESTINATION bin)
install(DIRECTORY config/ DESTINATION share/dex/config)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
