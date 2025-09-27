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

find_package(xxHash CONFIG REQUIRED)

# Source files
set(SRC_FILES
    main.cpp
    encoding.h
    utf8_to_utf16.h
)

if(WIN32)
    list(APPEND SRC_FILES utf8_to_utf16_windows.cpp encoding_windows.cpp)
else()
    list(APPEND SRC_FILES utf8_to_utf16_posix.cpp encoding_posix.cpp)
endif()

# 添加可执行文件
add_executable(${PROJECT_NAME} ${SRC_FILES})

# 设置编译器特定的选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    # link with iconv on posix if available
    find_library(ICONV_LIB NAMES iconv)
    if(ICONV_LIB)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${ICONV_LIB})
    endif()
endif()

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)