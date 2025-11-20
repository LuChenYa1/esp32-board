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

# 第一次配置esp-idf环境变量时的步骤记录：
# source export.sh 每次打开新终端都需要执行此命令，才能使用idf.py命令
# 所以直接将export.sh的source命令添加到~/.bashrc文件中，以便每次打开终端时自动执行
# 遇到espidf插件命令无法执行，尝试关闭当前文件夹

# 二次优化：
# 1、c/c++ 插件设置中，修改cpu占用为medium 
# 2、设置USB串口权限, 然后重启虚拟机: sudo usermod -aG dialout usrname  usrname需要换成你的用户名
# 3、虚拟机USB控制器的连接兼容性设置为 usb3.1, 然后重新连接（虚拟机右下角）
# 4、将windows的ssh密钥复制到虚拟机的~/.ssh目录下，并设置权限为600, 这样就不用每次远程连接时都输入密码
# 5、切换型号: idf.py set-target esp32c3
# 6、配置每个项目用到的系统资源（系统主频，flash大小, PSRAM等）: idf.py menuconfig
# 7、退出monitor: Ctrl+]

# 三次优化：程序无法烧录的解决方法
# 1、拔掉开发板的USB线，等待几秒钟后再重新插上，这样可以确保所有之前的连接被彻底重置
# 2、切换拓展坞的USB接口，有时某些接口可能会有兼容性问题
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(main)
