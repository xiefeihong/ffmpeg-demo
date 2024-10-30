#include "file_tool.h"

#include <iostream>
#include <fstream>
#include <string>

std::string readStringsFromFile(const char* filename) {
    std::string result;
    std::ifstream file(filename);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            result += line;
            result += '\n';
        }
        file.close();
    } else {
        std::cerr << "无法打开文件: " << filename << std::endl;
    }
    return result;
}