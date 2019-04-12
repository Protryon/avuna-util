cmake_minimum_required(VERSION 3.12)
project(avuna-util C)

set(CMAKE_VERBOSE_MAKEFILE on)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wno-discarded-qualifiers")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

file(GLOB util_headers
        "include/avuna/*.h"
)

file(GLOB util_src
    "src/*.c"
)

add_library(avuna-util SHARED ${util_src} ${util_headers})
target_include_directories(avuna-util PUBLIC include/)
target_include_directories(avuna-util PRIVATE src/)
target_link_libraries(avuna-util -lssl -lcrypto -lpthread)

install(TARGETS avuna-util
        LIBRARY DESTINATION lib)

install(FILES ${util_headers} DESTINATION "include/avuna/")

set(CPACK_DEBIAN_PACKAGE_NAME avunautils)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_PACKAGE_VERSION 1.0.0)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libssl1.1")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Max Bruce")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "General utilities for avuna projects")
set(CPACK_GENERATOR DEB)

include(CPack)