#ifndef FFMPEG_DEMO_GM_TOOL_H
#define FFMPEG_DEMO_GM_TOOL_H

#include <Magick++/Image.h>

extern "C" {
#include <libavutil/frame.h>
}

#define PI 3.14159265358979323846

double to_radians(double degrees);

void image_to_frame(Magick::Image *image, AVFrame *frame);

void composite_to_frame(Magick::Image *background, Magick::Image *overlay, int offsetX, int offsetY, const double degrees);

void composite_to_frame_plus(Magick::Image *background, Magick::Image *overlay, int offsetX, int offsetY, const double degrees);

#endif
