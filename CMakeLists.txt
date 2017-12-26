cmake_minimum_required(VERSION 3.1)
project(RainDrop)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
  "${CMAKE_SOURCE_DIR}/cmake_pkg/")

find_package(Box2D REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_gfx REQUIRED)

set(SRC src/main.cpp)

add_executable(${PROJECT_NAME} ${SRC})

target_include_directories(${PROJECT_NAME}
  PRIVATE ${BOX2D_INCLUDE_DIR}
  PRIVATE ${SDL2_INCLUDE_DIR}
  PRIVATE ${SDL2_GFX_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
  ${BOX2D_LIBRARY}
  ${SDL2_LIBRARY}
  ${SDL2_GFX_LIBRARIES})

target_compile_features(${PROJECT_NAME}
  PRIVATE cxx_std_14)
