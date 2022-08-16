cmake_minimum_required(VERSION 3.23)
project(Deployka)

set(POSSIBLE_BOOST_ROOT 
    "E:/prog/thirdparty/boost_1_79_0"
    "G:/prog/thirdparty/boost_1_79_0"
    "I:/prog_n_gamedev/thirdparty/boost_1_79_0")

foreach(ITEM ${POSSIBLE_BOOST_ROOT})
  if (EXISTS ${ITEM})
    set(BOOST_ROOT ${ITEM})
  endif()
endforeach()


message(NOTICE "Boost root is set to ${BOOST_ROOT}")

# boost must be compiled with this flags
# .\b2
#     toolset=msvc143
#     runtime-link=shared
#     link=static
#     variant=debug
#     address-model=64
#     threading=multi

find_package(Boost REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

if (WIN32)
  add_compile_options(/FI"SDKDDKVer.h" /W4 /WX)
endif()

add_subdirectory(CollectArtifacts)
add_subdirectory(DeployArtifacts)
add_subdirectory(NetworkLib)
add_subdirectory(HostAgent)
add_subdirectory(TargetAgent)

