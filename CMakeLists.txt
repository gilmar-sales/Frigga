cmake_minimum_required(VERSION 3.21)

project("Frigga Engine" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(Frigga)
add_subdirectory(Editor)

include(FetchContent)

FetchContent_Declare(
  freyr
  GIT_REPOSITORY "https://github.com/gilmar-sales/Freyr.git"
  GIT_TAG        "v0.12.4"
)
FetchContent_MakeAvailable(freyr)

FetchContent_Declare(
  freya
  GIT_REPOSITORY "https://github.com/gilmar-sales/Freya.git"
  GIT_TAG        "v0.8.6"
)
FetchContent_MakeAvailable(freya)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY "https://github.com/gilmar-sales/imgui.git"
  GIT_TAG        "docking"
)
FetchContent_MakeAvailable(imgui)

