cmake_minimum_required(VERSION 3.23)
project(Deployka)

# boost compiled with this flags
# .\b2
# toolset=vc143
# runtime-link=shared
# link=static
# variant=debug
# address-model=64
# threading=multi

add_subdirectory(CollectArtifacts)
add_subdirectory(DeployArtifacts)
add_subdirectory(HostAgent)
add_subdirectory(TargetAgent)

