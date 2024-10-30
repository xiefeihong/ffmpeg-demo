#include <Magick++.h>

int main() {
    Magick::InitializeMagick(nullptr);

    // 创建一个40x40像素的图像，背景为透明
    Magick::Image image(Magick::Geometry(60, 40), Magick::Color("transparent"));

    // 创建一个Geometry对象，指定边框的宽度和高度（对于方形边框，宽度和高度通常是相同的）
    Magick::Geometry borderGeometry(1, 1); // 1像素宽的边框，1像素高的边框

    // 正确的做法是同时指定颜色和Geometry对象
    image.border(borderGeometry); // 正确：设置红色边框，宽度和高度都是1像素

    // 保存图片为png格式，确保透明度被保留
    image.write("overlay.png");

    return 0;
}