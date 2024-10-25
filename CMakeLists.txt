# MTR https://github.com/ougi-washi/m3u8-to-rtmp

## General
cmake_minimum_required(VERSION 3.22.1)

project(m3u8_to_rtmp LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

if(NOT CMAKE_GENERATOR)
    find_program(NINJA_EXECUTABLE ninja)
    if(NINJA_EXECUTABLE)
        set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "Ninja")
        message(STATUS "Ninja build system found. Using Ninja generator.")
    else()
        message(STATUS "Ninja build system not found. Using default generator.")
    endif()
endif()

# Directory setup
set(LIB_DIR "lib")
set(SRC_DIR "src")
set(BIN_DIR "bin")

# setup library macro
macro(setup_library arg_lib_name arg_source_path arg_include_path)
	message(STATUS "Adding ${arg_lib_name}")
	file(GLOB CURRENT_SRC_FILES	"${arg_source_path}/*.c" 
								"${arg_source_path}/*.cpp"
								"${arg_source_path}/**/*.c" 
								"${arg_source_path}/**/*.cpp")
	add_library(${arg_lib_name} STATIC	${CURRENT_SRC_FILES})
	target_include_directories(${arg_lib_name} PUBLIC ${arg_include_path})
endmacro()

find_package(Threads REQUIRED)

# Main module
set(MAIN_MODULE_INCLUDES ${PROJECT_SOURCE_DIR}/${SRC_DIR} ${PROJECT_SOURCE_DIR}/${LIB_DIR})
setup_library(main_module ${SRC_DIR} "${MAIN_MODULE_INCLUDES}")
target_link_libraries(main_module PUBLIC avformat avcodec avdevice avfilter avutil swscale swresample Threads::Threads)

# Executable
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC main_module)
set_target_properties(${PROJECT_NAME} PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/${BIN_DIR})

## Definitions
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
