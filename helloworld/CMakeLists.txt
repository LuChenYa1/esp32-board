# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# 项目无法生成compile_commands.json文件的问题修复
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 在include之前添加IDF_PATH的检查和处理
if(NOT DEFINED ENV{IDF_PATH})
    # 如果环境变量不存在，尝试设置默认路径
    set(ENV{IDF_PATH} "/home/luchenya/project/01_learning_notes/esp32/esp-idf")
    message(STATUS "IDF_PATH not set, using default: $ENV{IDF_PATH}")
endif()

# source export.sh 每次打开新终端都需要执行此命令，才能使用idf.py命令
# 所以直接将export.sh的source命令添加到~/.bashrc文件中，以便每次打开终端时自动执行
# 遇到espidf插件命令无法执行，尝试关闭当前文件夹
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(main)
