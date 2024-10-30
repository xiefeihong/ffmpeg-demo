#ifndef FFMPEG_DEMO_JSON_TOOL_H
#define FFMPEG_DEMO_JSON_TOOL_H

#include <vector>

struct Position {
    int offsetX;
    int offsetY;
    double degrees;
};

std::map<int, Position> parsePositions(const char* json);

#endif //FFMPEG_DEMO_JSON_TOOL_H
