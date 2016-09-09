//
// Created by lorenzodonini on 09.09.16.
//

#ifndef S3TP_LOGGER_H
#define S3TP_LOGGER_H

#include <iostream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <string>

#define LOG_LEVEL_OFF 0
#define LOG_LEVEL_FATAL 1
#define LOG_LEVEL_ERROR 2
#define LOG_LEVEL_WARNING 3
#define LOG_LEVEL_INFO 4
#define LOG_LEVEL_DEBUG 5

extern const int LOG_LEVEL;

inline void s3tp_log(std::string logStr) {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    //char mbstr[32];
    //std::strftime(mbstr, sizeof(mbstr), "%A %c", tm))
    std::cout << std::put_time(&tm, "[%d.%m.%Y %H:%M:%S] ") << logStr << std::endl;
}

inline void LOG_DEBUG(std::string logStr) {
    if (LOG_LEVEL < LOG_LEVEL_DEBUG) {
        return;
    }
    s3tp_log(logStr);
}

inline void LOG_INFO(std::string logStr) {
    if (LOG_LEVEL < LOG_LEVEL_INFO) {
        return;
    }
    s3tp_log(logStr);
}

inline void LOG_WARN(std::string logStr) {
    if (LOG_LEVEL < LOG_LEVEL_WARNING) {
        return;
    }
    s3tp_log(logStr);
}

inline void LOG_ERROR(std::string logStr) {
    if (LOG_LEVEL < LOG_LEVEL_ERROR) {
        return;
    }
    s3tp_log(logStr);
}

inline void LOG_FATAL(std::string logStr) {
    if (LOG_LEVEL < LOG_LEVEL_FATAL) {
        return;
    }
    s3tp_log(logStr);
}


#endif //S3TP_LOGGER_H
