# 指定源文件路径，使用相对路径（相对于当前CMakeLists.txt所在目录）
set(component_sources
    "src/cst816t_driver.c"
    "src/st7789_driver.c"
    "src/dht11.c"
    "src/led_ws2812.c"
)

# 指定头文件目录，同样使用相对路径
set(component_include_dirs
    "inc"
)

idf_component_register(
    SRCS ${component_sources}
    INCLUDE_DIRS ${component_include_dirs}
    PRIV_REQUIRES esp_timer esp_lcd
)