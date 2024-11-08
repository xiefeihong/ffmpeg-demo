#include <Magick++.h>

#include "gm_tool.h"

int main(){
    Magick::InitializeMagick(nullptr);
    int offsetX = 100;
    int offsetY = 100;
    double degrees = 10;
    const char *background_image = "001.png";
    const char *overlay_image = "overlay.png";
    const char *output_image = "result.png";

    Magick::Image background;
    background.read(background_image); // 替换为您的背景图像文件名
    Magick::Image overlay;
    overlay.read(overlay_image); // 替换为您的要旋转的图像文件名
    overlay.backgroundColor(Magick::Color("#ffffffff"));

    composite_to_frame_plus(&background, &overlay, offsetX, offsetY, degrees);
    background.write(output_image);
    return 0;
}