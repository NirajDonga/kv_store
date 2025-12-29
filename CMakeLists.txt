cmake_minimum_required(VERSION 3.10)
project(DistributedKVStore)

set(CMAKE_CXX_STANDARD 17)

# Windows compatibility
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
endif()

include_directories(include)

add_library(hash_ring src/common/hash_ring.cpp)
add_executable(kv_server src/server/main.cpp)
add_executable(kv_proxy src/proxy/main.cpp)
add_executable(kv_client src/client/main.cpp)

# Platform-specific linking
if(WIN32)
    target_link_libraries(kv_server hash_ring ws2_32 crypt32)
    target_link_libraries(kv_proxy hash_ring ws2_32 crypt32)
    target_link_libraries(kv_client ws2_32 crypt32)
else()
    target_link_libraries(kv_server hash_ring pthread)
    target_link_libraries(kv_proxy hash_ring pthread)
    target_link_libraries(kv_client pthread)
endif()