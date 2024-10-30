cmake_minimum_required(VERSION 3.29)
project(ffmpeg_demo)

set(CMAKE_CXX_STANDARD 20)

#set(FFMPEG_HOME /usr/local/ffmpeg-6.0)
set(FFMPEG_HOME /usr/local/ffmpeg-6.1.2)
#set(FFMPEG_HOME /usr/local/ffmpeg-7.0.2)
#set(FFMPEG_HOME /usr/local/ffmpeg-7.1)
set(GM_HOME /usr/local/GraphicsMagick-1.3.45)

set(FFMPEG_LIB avcodec avutil swresample)
set(GM_LIB GraphicsMagick++ GraphicsMagick tiff sharpyuv webp webpmux freetype jpeg png16 Xext X11 xml2 zstd lzma bz2 z m pthread gomp)

include_directories(
        ${FFMPEG_HOME}/include
        ${GM_HOME}/include/GraphicsMagick
        /usr/include/rapidjson
        include
)

link_directories(
        ${FFMPEG_HOME}/lib
        ${GM_HOME}/lib
)

add_executable(mp4_to_img src/mp4_to_img.c)
add_executable(mp4_to_bmp src/mp4_to_bmp.c)
add_executable(mp4_to_png src/mp4_to_png.c)
add_executable(encode_video src/encode_video.c)
add_executable(img_to_mp4 src/img_to_mp4.c)
add_executable(gm_create src/gm_create.cpp)
add_executable(gm_composite src/gm_composite.cpp src/gm_tool.cpp)
add_executable(ffmpeg_demo src/gm_to_ff.cpp src/gm_tool.cpp src/json_tool.cpp src/file_tool.cpp)

target_link_libraries(mp4_to_img
        ${FFMPEG_LIB} avformat
)

target_link_libraries(mp4_to_bmp
        ${FFMPEG_LIB} swscale avformat
)

target_link_libraries(mp4_to_png
        ${FFMPEG_LIB} swscale avformat
        png16
)

target_link_libraries(encode_video
        ${FFMPEG_LIB}
)

target_link_libraries(img_to_mp4
        ${FFMPEG_LIB} swscale avformat
        png16
)

target_link_libraries(gm_composite
        ${GM_LIB}
)

target_link_libraries(gm_create
        ${GM_LIB}
)

target_link_libraries(ffmpeg_demo
        ${FFMPEG_LIB} swscale avformat
        ${GM_LIB}
)
