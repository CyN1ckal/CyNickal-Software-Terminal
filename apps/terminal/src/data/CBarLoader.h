//
// Created by cynickal on 9/18/26.
//

#ifndef MYAPP_CBARLOADER_H
#define MYAPP_CBARLOADER_H

#include "CBarData.h"

#include <filesystem>
#include <string>
#include <vector>

class CBarLoader {
public:
    struct HeaderNames {
        std::string Open{"Open"};
        std::string High{"High"};
        std::string Low{"Low"};
        std::string Close{"Close"};
        std::string Volume{"Volume"};
        std::string Date{"Date"};
    };

    static std::vector<CBarData> LoadFromFile(const std::filesystem::path& filePath);
    static std::vector<CBarData> LoadFromFile(const std::filesystem::path& filePath,
                                              const HeaderNames& headers);
};

#endif //MYAPP_CBARLOADER_H
