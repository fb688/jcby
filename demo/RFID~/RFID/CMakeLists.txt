cmake_minimum_required(VERSION 3.6)

set(CMAKE_C_COMPILER arm-linux-gcc)
set(CMAKE_C_COMPILER_AR arm-linux-ar)
set(CMAKE_CXX_COMPILER arm-linux-g++)

include_directories(include/)

add_executable(RFID RFID.c)
target_link_libraries(RFID pthread)