cmake_minimum_required(VERSION 4.1)
project(Steganography C)

set(CMAKE_C_STANDARD 11)

add_executable(Steganography src/main.c
        src/lsb_steg.c
        include/lsb_steg.h
)
