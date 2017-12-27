cmake_minimum_required(VERSION 3.1)
project(RainDrop)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
  "${CMAKE_SOURCE_DIR}/cmake_pkg/")

set(ENABLE_MULTITHREAD ON CACHE BOOL "Enable multithread updates")

find_package(Box2D REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_gfx REQUIRED)
find_package(SDL2_ttf REQUIRED)

set(SRC src/main.cpp)

add_executable(${PROJECT_NAME} ${SRC})

target_include_directories(${PROJECT_NAME}
  PRIVATE ${BOX2D_INCLUDE_DIR}
  PRIVATE ${SDL2_INCLUDE_DIR}
  PRIVATE ${SDL2_GFX_INCLUDE_DIRS}
  PRIVATE ${SDL2_TTF_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
  ${BOX2D_LIBRARY}
  ${SDL2_LIBRARY}
  ${SDL2_GFX_LIBRARIES}
  ${SDL2_TTF_LIBRARIES})

target_compile_features(${PROJECT_NAME}
  PRIVATE cxx_std_14)

if (ENABLE_MULTITHREAD)
  target_compile_definitions(${PROJECT_NAME}
    PRIVATE "-DENABLE_MULTITHREAD")
endif ()
