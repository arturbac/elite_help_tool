# eht_vision: a process of its own beside the tool - the recorder of pictures of the screen now, a model
# reading them later. Nothing of it runs inside the game or inside the tool
find_package(PNG REQUIRED)

add_library(eht_vision_core STATIC)
target_sources(eht_vision_core
  PRIVATE
    src/vision_recorder.cc
  PUBLIC FILE_SET headers
    TYPE HEADERS
    BASE_DIRS include
    FILES
      include/vision_recorder.h
)
target_link_libraries(eht_vision_core PUBLIC eht PRIVATE PNG::PNG)

add_executable(eht_vision)
target_sources(eht_vision PRIVATE src/eht_vision.cc)
target_link_libraries(eht_vision PRIVATE eht_vision_core)
