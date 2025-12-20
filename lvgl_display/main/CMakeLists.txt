# 指定源文件路径，使用相对路径（相对于当前CMakeLists.txt所在目录）
set(component_sources
    "src/lvgl_display.c"
    "src/lv_port.c"
    "src/ui_led.c"
    "src/ui_home.c"
    "img/humidity_img.c"
    "img/temp_img.c"
)

# 指定头文件目录，同样使用相对路径
set(component_include_dirs
    "inc"
)

idf_component_register(
    SRCS ${component_sources}
    INCLUDE_DIRS ${component_include_dirs}
    REQUIRES lvgl driver bsp)
