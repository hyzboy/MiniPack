cmake_minimum_required(VERSION 3.10)

# Project
project(MiniPack)

# Option: use static C runtime
option(USE_STATIC_CRT "Use static C runtime (MSVC: /MT, others: -static-libgcc -static-libstdc++)" ON)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer static CRT on MSVC: replace /MD with /MT in flags when requested
if(MSVC)
    if(USE_STATIC_CRT)
        message(STATUS "Configuring MSVC to use static runtime (/MT)")
        foreach(flag_var
            CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_MINSIZEREL
            CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
            if(DEFINED ${flag_var})
                string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
            endif()
        endforeach()
    else()
        message(STATUS "Using dynamic C runtime on MSVC (/MD)")
    endif()
else()
    # Non-MSVC: if requested, instruct linker to use static libgcc/libstdc++ where possible
    if(USE_STATIC_CRT)
        message(STATUS "Requesting static libgcc/libstdc++ on non-MSVC toolchains")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    else()
        message(STATUS "Using dynamic runtime on non-MSVC toolchains")
    endif()
endif()

# Core writer library (no file-list-reader, no UTF conversions)
set(MINIPACK_WRITER_SOURCES
    mini_pack_builder.cpp
    mini_pack_builder.h
    mini_pack_builder_file.cpp
    mini_pack_builder_file.h
    mini_pack_writer_vector.cpp
    mini_pack_writer_file.cpp
)
add_library(minipack_writer STATIC ${MINIPACK_WRITER_SOURCES})

# UTF/encoding support
set(MINIPACK_UTF_SOURCES
    utf8_to_utf16.cpp
    utf16_to_utf8.cpp
    utf32_to_utf8.cpp
    utf_conv.h
    encoding.h
)
if(WIN32)
    list(APPEND MINIPACK_UTF_SOURCES encoding_windows.cpp)
else()
    list(APPEND MINIPACK_UTF_SOURCES encoding_posix.cpp)
endif()
add_library(minipack_utf STATIC ${MINIPACK_UTF_SOURCES})

# Reader/index support
set(MINIPACK_READER_SOURCES
    pack_reader_index.cpp
    pack_reader.h
    pack_reader_io.h
    pack_reader_io.cpp
)
add_library(minipack_reader STATIC ${MINIPACK_READER_SOURCES})

# Reader depends on utf library
target_link_libraries(minipack_reader PRIVATE minipack_utf)

# Executable: include file_list_reader and dir_scan (dir_scan only used by the exe)
add_executable(${PROJECT_NAME} main.cpp file_list_reader.cpp dir_scan.cpp)

# Link libraries to executable
target_link_libraries(${PROJECT_NAME} PRIVATE minipack_writer minipack_reader minipack_utf)

# Compiler warnings/options
if(MSVC)
    target_compile_options(minipack_writer PRIVATE /W4)
    target_compile_options(minipack_reader PRIVATE /W4)
    target_compile_options(minipack_utf PRIVATE /W4)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(minipack_writer PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(minipack_reader PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(minipack_utf PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    # link with iconv on posix if available (needed by utf conversions)
    find_library(ICONV_LIB NAMES iconv)
    if(ICONV_LIB)
        target_link_libraries(minipack_utf PRIVATE ${ICONV_LIB})
    endif()
endif()

# Set executable output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
