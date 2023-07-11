# 秋招项目里面
cmake_minimum_required(VERSION 2.5) #最低版本号
project(mymuduo)

# cmake => makefile   make
# mymuduo最终编译成so动态库，设置动态库的路径，放在根目录的lib文件夹下面
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)  #LIBRARY_OUTPUT_PATH 输出路径,当前目录的lib下

# 设置调试信息 以及 启动C++11语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

# 定义参与编译的源文件
aux_source_directory(. SRC_LIST)

#  编译动态库mymuduo.os
add_library(mymuduo SHARED ${SRC_LIST})