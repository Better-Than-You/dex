# Locate the sherpa-onnx C API (headers + shared library).
#
# The C++ wrapper (libsherpa-onnx-cxx-api) is built with the pre-C++11
# libstdc++ ABI, so Dex deliberately links the C API instead: it is
# ABI-neutral and exposes everything needed for VAD + offline recognition.
#
# Hints:
#   SHERPA_ONNX_ROOT   installation prefix (default: /usr/local)
#   SherpaOnnx_ROOT    CMake-style install prefix

find_path(SherpaOnnx_INCLUDE_DIR
    NAMES sherpa-onnx/c-api/c-api.h
    HINTS
        ${SHERPA_ONNX_ROOT}
        ${SherpaOnnx_ROOT}
        ENV SHERPA_ONNX_ROOT
    PATH_SUFFIXES include
    PATHS /usr/local/include /usr/include
)

find_library(SherpaOnnx_C_API_LIBRARY
    NAMES sherpa-onnx-c-api
    HINTS
        ${SHERPA_ONNX_ROOT}
        ${SherpaOnnx_ROOT}
        ENV SHERPA_ONNX_ROOT
    PATH_SUFFIXES lib lib64
    PATHS /usr/local/lib /usr/lib
)

find_library(SherpaOnnx_ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    HINTS
        ${SHERPA_ONNX_ROOT}
        ${SherpaOnnx_ROOT}
        ENV SHERPA_ONNX_ROOT
    PATH_SUFFIXES lib lib64
    PATHS /usr/local/lib /usr/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SherpaOnnx
    REQUIRED_VARS SherpaOnnx_INCLUDE_DIR SherpaOnnx_C_API_LIBRARY
    VERSION_VAR SherpaOnnx_VERSION
)

if(SherpaOnnx_FOUND AND NOT TARGET SherpaOnnx::SherpaOnnx)
    add_library(SherpaOnnx::SherpaOnnx UNKNOWN IMPORTED)
    set_target_properties(SherpaOnnx::SherpaOnnx PROPERTIES
        IMPORTED_LOCATION "${SherpaOnnx_C_API_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SherpaOnnx_INCLUDE_DIR}"
    )

    # Runtime lookup: the prebuilt bundle uses RPATH=$ORIGIN, but the system
    # install may live in a prefix that is not on the default loader path.
    get_filename_component(_sherpa_lib_dir "${SherpaOnnx_C_API_LIBRARY}" DIRECTORY)
    set_property(TARGET SherpaOnnx::SherpaOnnx APPEND PROPERTY
        INTERFACE_LINK_OPTIONS "-Wl,-rpath,${_sherpa_lib_dir}")
    unset(_sherpa_lib_dir)

    if(SherpaOnnx_ONNXRUNTIME_LIBRARY)
        set_property(TARGET SherpaOnnx::SherpaOnnx APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES "${SherpaOnnx_ONNXRUNTIME_LIBRARY}")
    endif()
endif()

mark_as_advanced(SherpaOnnx_INCLUDE_DIR SherpaOnnx_C_API_LIBRARY SherpaOnnx_ONNXRUNTIME_LIBRARY)
