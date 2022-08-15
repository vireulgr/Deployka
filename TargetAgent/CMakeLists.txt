# TargetAgent
add_executable(TargetAgent main.cpp tests.cpp tests.h)
include_directories("${PROJECT_SOURCE_DIR}/NetworkLib")
link_directories(${NetworkLib_LIBRARY_DIRS})
target_link_libraries(TargetAgent NetworkLib)
