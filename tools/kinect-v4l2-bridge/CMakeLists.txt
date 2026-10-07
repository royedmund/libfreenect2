cmake_minimum_required(VERSION 3.16)
project(kinect_v4l2_bridge LANGUAGES CXX)

# The installer builds libfreenect2 into ~/.local and adds that prefix to
# PKG_CONFIG_PATH. For manual builds, set PKG_CONFIG_PATH accordingly.
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREENECT2 REQUIRED IMPORTED_TARGET freenect2)

add_executable(kinect-v2-bridge kinect-v4l2-bridge.cpp)
target_compile_features(kinect-v2-bridge PRIVATE cxx_std_11)
target_compile_options(kinect-v2-bridge PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(kinect-v2-bridge PRIVATE PkgConfig::FREENECT2)
# Do not depend on a build-tree absolute path such as /home/royed/libfreenect2/build/lib.
# The runtime RPATH is injected by scripts/install-kinect-v2-webcam.sh.
