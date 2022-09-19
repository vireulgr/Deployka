# TODO
add_executable(CollectArtifacts main.cpp)
include_directories("${PROJECT_SOURCE_DIR}/UtilityLib")
link_directories(${UtilityLib_LIBRARY_DIRS})
target_link_libraries(CollectArtifacts UtilityLib)
