cmake_minimum_required(VERSION 3.12.0)

project(soundboard)

find_package(SDL3 REQUIRED)
add_executable(soundboard soundboard.c)
target_include_directories(soundboard PRIVATE ${SDL3_INCLUDE_DIRS} ${SDL3_INCLUDE_DIR})
target_link_libraries(soundboard ${SDL3_LIBRARIES} ${SDL3_LIBRARY})

# end of CMakeLists.txt ...

