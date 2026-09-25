cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components/cjson")
# Compile the adapter's dependency closure; the SDK's display/network examples
# are not part of this headless Bluetooth application.
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(MESHBBS-MeshCore VERSION 0.1.0)
