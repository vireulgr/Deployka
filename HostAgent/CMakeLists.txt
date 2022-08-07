#TODO
add_executable(HostAgent main.cpp)
include_directories("${PROJECT_SOURCE_DIR}/NetworkLib")
link_directories(${NetworkLib_LIBRARY_DIRS})
target_link_libraries(HostAgent NetworkLib)
