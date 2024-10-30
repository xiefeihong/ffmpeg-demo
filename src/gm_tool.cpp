#include "gm_tool.h"

#include <Magick++.h>

extern "C" {
#include <libavutil/frame.h>
}

double to_radians(double degrees) {
    return degrees * PI / 180.0;
}

void image_to_frame(Magick::Image *image, AVFrame *frame) {
    int width = image->columns();
    int height = image->rows();
    // 获取每个像素并填充到AVFrame
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Magick::ColorRGB color = image->pixelColor(x, y);
            int offset = (y * width + x) * 3;
            frame->data[0][offset] = static_cast<uint8_t>(color.red() * 255);
            frame->data[0][offset + 1] = static_cast<uint8_t>(color.green() * 255);
            frame->data[0][offset + 2] = static_cast<uint8_t>(color.blue() * 255);
        }
    }
}

void composite_to_frame(Magick::Image *background, Magick::Image *overlay, int offsetX, int offsetY, double degrees){
    overlay->rotate(degrees); // 角度以度为单位
    background->composite(*overlay, offsetX, offsetY, Magick::OverCompositeOp);
}

void composite_to_frame_plus(Magick::Image *background, Magick::Image *overlay, int offsetX, int offsetY, double degrees) {
    Magick::Geometry size = overlay->size();
    int width = size.width();
    int height = size.height();

    double degrees_360 = fmod(degrees, 360);
    double degrees_90 = fmod(degrees, 90);
    double radians = to_radians(degrees_90);
    double cosa = cos(radians);
    double sina = sin(radians);

    double x, y;
    if (degrees_360 <= 90 || (degrees_360 >= 180 && degrees_360 <= 270)) {
        x = offsetX - (sina * height + cosa * width) / 2;
        y = offsetY - (cosa * height + sina * width) / 2;
    } else {
        x = offsetX - (sina * width + cosa * height) / 2;
        y = offsetY - (cosa * width + sina * height) / 2;
    }

    composite_to_frame(background, overlay, x, y, degrees);
}
