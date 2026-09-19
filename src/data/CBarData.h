//
// Created by cynickal on 9/18/26.
//

#ifndef MYAPP_CBARDATA_H
#define MYAPP_CBARDATA_H

#include <chrono>

class CBarData {
public:
    std::chrono::time_point<std::chrono::system_clock> m_StartTime;
    float m_Open{};
    float m_High{};
    float m_Low{};
    float m_Close{};
    float m_Volume{};
};

#endif //MYAPP_CBARDATA_H
