cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(MiniPack)

# 设置C++20标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer static CRT on MSVC: replace /MD with /MT in flags
if(MSVC)
    message(STATUS "Configuring MSVC to use static runtime (/MT)")
    foreach(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_MINSIZEREL
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
        if(DEFINED ${flag_var})
            string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
endif()

# All source files (kept for reference)
set(ALL_SRC_FILES
    main.cpp
    encoding.h
    utf_conv.h
    mini_pack_builder.cpp
    mini_pack_builder.h
    pack_reader_index.cpp
    pack_reader.h
    pack_reader_io.h
    pack_reader_io.cpp
    mini_pack_builder_file.h
    mini_pack_builder_file.cpp
    mini_pack_writer_vector.cpp
    mini_pack_writer_file.cpp
    utf8_to_utf16.cpp
    file_list_reader.cpp
    utf16_to_utf8.cpp
    utf32_to_utf8.cpp
    dir_scan.h
    dir_scan.cpp
)

if(WIN32)
    list(APPEND ALL_SRC_FILES encoding_windows.cpp)
else()
    list(APPEND ALL_SRC_FILES encoding_posix.cpp)
endif()

# Create a static library for building/writing minipack (no file-list-reader, no UTF conversions)
set(MINIPACK_WRITER_SOURCES
    mini_pack_builder.cpp
    mini_pack_builder.h
    mini_pack_builder_file.cpp
    mini_pack_builder_file.h
    mini_pack_writer_vector.cpp
    mini_pack_writer_file.cpp
)
add_library(minipack_writer STATIC ${MINIPACK_WRITER_SOURCES})

# Create a static library for UTF conversions and encoding (split out)
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

# Create a static library for reading minipack (pack_reader_io and related index)
set(MINIPACK_READER_SOURCES
    pack_reader_index.cpp
    pack_reader.h
    pack_reader_io.h
    pack_reader_io.cpp
)
add_library(minipack_reader STATIC ${MINIPACK_READER_SOURCES})

# Make reader depend on utf library
target_link_libraries(minipack_reader PRIVATE minipack_utf)

# Add executable and link against the libraries
# Include file_list_reader.cpp in the executable to provide read_file_list implementation
add_executable(${PROJECT_NAME} main.cpp file_list_reader.cpp dir_scan.cpp)
# Link utf library explicitly so it is built and available at link time
target_link_libraries(${PROJECT_NAME} PRIVATE minipack_writer minipack_reader minipack_utf)

# 设置编译器特定的选项
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

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
