add_library(tools INTERFACE)

target_include_directories(tools INTERFACE ${PROJECT_SOURCE_DIR})
target_link_libraries(tools INTERFACE yaml-cpp)
