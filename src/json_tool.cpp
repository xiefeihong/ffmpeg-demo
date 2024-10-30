#include <map>
#include "json_tool.h"

#include "rapidjson/document.h"

std::map<int, Position> parsePositions(const char *json) {
    std::map<int, Position> result;
    rapidjson::Document doc;
    doc.Parse(json);
    if (doc.IsObject()) {
        for (rapidjson::Value::ConstMemberIterator it = doc.MemberBegin(); it!= doc.MemberEnd(); ++it) {
            int key = std::stoi(it->name.GetString());
            const rapidjson::Value& value = it->value;
            Position pos;
            if (value.HasMember("offsetX")) {
                pos.offsetX = value["offsetX"].GetInt();
            }
            if (value.HasMember("offsetY")) {
                pos.offsetY = value["offsetY"].GetInt();
            }
            if (value.HasMember("degrees")) {
                pos.degrees = value["degrees"].GetDouble();
            }
            result[key] = pos;
        }
    }
    return result;
}
