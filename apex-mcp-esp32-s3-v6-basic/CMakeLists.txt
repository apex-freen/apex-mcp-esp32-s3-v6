# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

# 告诉 IDF，除了标准的 components，还要去 framework 文件夹找组件
set(EXTRA_COMPONENT_DIRS "apex_frame" "common")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(apex-mcp-esp32-s3-v6-basic)
