# Locate llama.cpp (headers + shared libraries).
#
# Hints:
#   LLAMA_CPP_ROOT  installation prefix (default: ~/.local, /usr/local)
#
# Dex only needs the `llama` target; it links the shared library with an
# rpath so the model can be loaded at runtime without LD_LIBRARY_PATH.

find_path(LlamaCpp_INCLUDE_DIR
    NAMES llama.h
    HINTS
        ${LLAMA_CPP_ROOT}
        ENV LLAMA_CPP_ROOT
    PATH_SUFFIXES include
    PATHS "$ENV{HOME}/.local/include" /usr/local/include /usr/include
)

find_library(LlamaCpp_LIBRARY
    NAMES llama
    HINTS
        ${LLAMA_CPP_ROOT}
        ENV LLAMA_CPP_ROOT
    PATH_SUFFIXES lib lib64
    PATHS "$ENV{HOME}/.local/lib" /usr/local/lib /usr/lib /usr/lib64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LlamaCpp
    REQUIRED_VARS LlamaCpp_INCLUDE_DIR LlamaCpp_LIBRARY
    VERSION_VAR LlamaCpp_VERSION
)

if(LlamaCpp_FOUND AND NOT TARGET LlamaCpp::LlamaCpp)
    add_library(LlamaCpp::LlamaCpp UNKNOWN IMPORTED)
    set_target_properties(LlamaCpp::LlamaCpp PROPERTIES
        IMPORTED_LOCATION "${LlamaCpp_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LlamaCpp_INCLUDE_DIR}"
    )

    get_filename_component(_llama_lib_dir "${LlamaCpp_LIBRARY}" DIRECTORY)
    set_property(TARGET LlamaCpp::LlamaCpp APPEND PROPERTY
        INTERFACE_LINK_OPTIONS "-Wl,-rpath,${_llama_lib_dir}")
    unset(_llama_lib_dir)
endif()

mark_as_advanced(LlamaCpp_INCLUDE_DIR LlamaCpp_LIBRARY)
