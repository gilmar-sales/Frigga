file(GLOB_RECURSE EDITOR_SRC *.cpp *.hpp)

add_executable(Editor ${EDITOR_SRC})

if(MSVC)
  target_compile_options(Editor PUBLIC "/ZI")
  target_link_options(Editor PUBLIC "/INCREMENTAL")
endif()

target_link_libraries(Editor PUBLIC frigga)

file(COPY Resources DESTINATION ${CMAKE_BINARY_DIR}/Editor)
file(COPY imgui.ini DESTINATION ${CMAKE_BINARY_DIR}/Editor)
