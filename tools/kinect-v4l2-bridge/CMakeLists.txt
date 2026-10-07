cmake_minimum_required(VERSION 3.16)
project(kinect_v4l2_bridge LANGUAGES CXX)

# Build and install the main libfreenect2 first; it provides freenect2.pc.
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREENECT2 REQUIRED IMPORTED_TARGET freenect2)

add_executable(kinect-v4l2-bridge kinect-v4l2-bridge.cpp)
target_compile_features(kinect-v4l2-bridge PRIVATE cxx_std_11)
target_compile_options(kinect-v4l2-bridge PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(kinect-v4l2-bridge PRIVATE PkgConfig::FREENECT2)
