cmake_minimum_required(VERSION 3.22)

add_subdirectory(vendor/raylib)

project(TankController LANGUAGES C)
add_executable(
    TankController
    controller/src/byteswap.c
    controller/src/controller.c
    controller/src/main.c
    controller/src/gui.c)
target_link_libraries(TankController PUBLIC ws2_32 raylib)
