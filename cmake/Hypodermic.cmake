# Hypodermic IoC (header-only). Pin: third_party/hypodermic @ ba5516d (v2.5.0 lineage).
# ADR 0007: linked only from viewer_bootstrap and tests — not kernel.

set(_hypodermic_root "${CMAKE_SOURCE_DIR}/third_party/hypodermic")
if(NOT EXISTS "${_hypodermic_root}/Hypodermic/Container.h")
  message(FATAL_ERROR
    "Hypodermic submodule missing. Run:\n"
    "  git submodule update --init third_party/hypodermic")
endif()

include(${BREP_REPO_ROOT}/cmake/BrepBoostHeaders.cmake)

add_library(Hypodermic INTERFACE)
add_library(Hypodermic::Hypodermic ALIAS Hypodermic)
target_include_directories(Hypodermic INTERFACE "${_hypodermic_root}")
target_link_libraries(Hypodermic INTERFACE brep_boost_headers)
target_compile_features(Hypodermic INTERFACE cxx_std_17)
