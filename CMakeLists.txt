cmake_minimum_required(VERSION 3.21)

project("Frigga Engine" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_DEBUG_POSTFIX "-d")

add_subdirectory(Frigga)
add_subdirectory(Editor)