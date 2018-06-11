cmake_minimum_required(VERSION 2.8)

project(libshot2jpeg_project)

add_library(shot2jpeg STATIC shot2jpeg.c)

add_executable(http_example_server http_example_server.c)
target_link_libraries(http_example_server httpd)

add_executable(shot2mjpeg shot2jpeg.c shot2mjpeg_server.c)
target_link_libraries(shot2mjpeg httpd xcb xcb-image jpeg)