cmake_minimum_required(VERSION 3.22)
project(compiler)

# compile flags
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS   "-g")                     # 调试信息
set(CMAKE_CXX_FLAGS   "-Wall")                  # 开启所有警告
# debug flags
# add_definitions(-DDEBUG_DFA)
# add_definitions(-DDEBUG_SCANNER)
# add_definitions(-DDEBUG_PARSER)

# output settings
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)


# include
include_directories(./include)

# third party libs
add_library(jsoncpp ./src/third_party/jsoncpp/jsoncpp.cpp)

# --------------------- from lib ---------------------
# link libxx.a
# u should rename libxx-x86-win.a or libxx-x86-linux.a to libxx.a according to ur own platform
link_directories(./lib)
# --------------------- from lib ---------------------

# build library
# every src file directory should be compile into a lib

aux_source_directory(./src/front FRONT_SRC)
add_library(Front ${FRONT_SRC})

# 为了 debug 方便，你可以选择通过源文件来构建 IR 测评机，但是请以链接静态库文件的方式去跑分（为了防止你们修改测评机，在OJ上我们会采取此方式）
# --------------------- from src ---------------------
# aux_source_directory(./src/ir IR_SRC)
# add_library(IR ${IR_SRC})
# aux_source_directory(./src/tools TOOLS_SRC)
# add_library(Tools ${TOOLS_SRC})
# --------------------- from src ---------------------


# executable
add_executable(compiler main.cpp)

# link
# every lib should be linked with [compiler]
target_link_libraries(compiler Tools-arm-mac Front IR-arm-mac jsoncpp)


# ---------------------
# 自动扫描 case 目录下的所有 .cpp 文件，并为每个文件创建一个可执行目标
file(GLOB CASE_FILES "ir_case/case*.cpp")

foreach(CASE_FILE IN LISTS CASE_FILES)
    get_filename_component(TARGET_NAME ${CASE_FILE} NAME_WE)

    add_executable(${TARGET_NAME} ${CASE_FILE})

    # 设置输出路径与主程序一致
    set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

    # 添加头文件包含目录
    target_include_directories(${TARGET_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

    # 链接需要的库
    target_link_libraries(${TARGET_NAME} IR-arm-mac Tools-arm-mac jsoncpp)

    # 添加 Front 的源码进来，方便调试
    target_sources(${TARGET_NAME} PRIVATE ${FRONT_SRC})
endforeach()