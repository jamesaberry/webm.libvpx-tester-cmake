cmake_minimum_required(VERSION 2.9.0)

project(libvpx-tester)

add_definitions(-w -D API -D HAVE_STDINT_H -D CONFIG_MD5=1 -D COMP_GCC -D INLINE= )

include_directories(include include/release-64)

file(GLOB SOURCES
        "src/*.cpp"
        "src/util/psnr/*.cpp"
        "src/util/vpx/*.cpp"
        "src/util/vpx/*.c"
        "src/tools/*.cpp"
        "src/util/*.cpp"
        "src/tests/*.cpp"
        )

add_executable(libvpx-tester ${SOURCES})

add_library(vpx_Lin64 STATIC IMPORTED)
set_property(TARGET vpx_Lin64 PROPERTY IMPORTED_LOCATION lib/libvpx_Lin64.a)
target_link_libraries(libvpx-tester vpx_Lin64 -lpthread -fprofile-arcs -ftest-coverage)
