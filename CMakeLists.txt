cmake_minimum_required(VERSION 3.30)
project(untitled)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")

if(WIN32)
    add_compile_definitions(UNICODE _UNICODE)
endif()

add_executable(untitled main.cpp)

target_link_libraries(untitled
        Shlwapi
        -static-libgcc
        -static-libstdc++
)