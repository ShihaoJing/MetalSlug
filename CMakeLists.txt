cmake_minimum_required(VERSION 3.7)
project(HandmadeHero)

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(source bin)
set(prj_bin ${CMAKE_CURRENT_BINARY_DIR}/bin)