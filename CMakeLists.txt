cmake_minimum_required(VERSION 3.5)
project(predix)

set(CMAKE_CXX_STANDARD 11)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(SOURCE_FILES
        sensor/Sampler.cpp
        sensor/Sampler.h
        sensor/Sender.cpp
        sensor/Sender.h
        sensor/Messages.h
        sensor/errors.h
        sensor/Application.cpp
        sensor/Application.h
        sensor/WSClient.cpp
        sensor/WSClient.h sensor/HTTPClient.cpp sensor/HTTPClient.h)

add_executable(sensor ${SOURCE_FILES})
target_link_libraries(sensor ${CONAN_LIBS})
target_compile_options(sensor PUBLIC -DPOCO_LOG_DEBUG)

#add_executable(sensor_test test.cpp)
#target_link_libraries(sensor_test ${CONAN_LIBS})
#target_compile_options(sensor_test PUBLIC -g -O0 -DPOCO_LOG_DEBUG)
