#TODO
set(BOOST_ROOT "E:/prog/thirdparty/boost_1_79_0")
find_package(Boost REQUIRED)

add_executable(TargetAgent main.cpp)

include_directories(${Boost_INCLUDE_DIRS})
target_link_directories(TargetAgent PRIVATE ${Boost_LIBRARY_DIRS})

if (WIN32)
  target_compile_options(TargetAgent PRIVATE /FI"SDKDDKVer.h")
endif()
