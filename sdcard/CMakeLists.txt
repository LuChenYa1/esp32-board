# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# 问题: CMakeLists包含路径报红
# 解决方法:
# 1、终端分别输入 echo $IDF_PATH, which idf.py, 确保显示路径正确
# 2、删除 .build 文件夹, 重新构建项目
# 3、重新加载配置: ctrl + shift + p 打开命令面板, 运行 “CMake: Delete Cache and Reconfigure”
# 4、按下 Ctrl+Shift+P, 搜索 Preferences: Open Workspace Settings (JSON) 并打开,添加以下配置，将路径替换为实际 IDF_PATH:
# {
#     "cmake.configureEnvironment": {
#         "IDF_PATH": "/home/你的用户名/esp/esp-idf"
#     }
# }
# 5、保存文件, 再次执行第 3 步
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(sdcard)
