cmake_minimum_required(VERSION 4.1)
project(Steganography C)

set(CMAKE_C_STANDARD 11)

add_executable(Steganography src/main.c
        src/image.c
        src/lsb.c
        src/compare.c)
