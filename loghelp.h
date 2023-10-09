#pragma once
#include "logger.h"

//
static FileWriter loger_main("main.log");
template< typename... Args >
void printf_tofile(int level, const char* format, Args... args) 
{
    int length = std::snprintf(nullptr, 0, format, args...);
    if (length <= 0) {
        return;
    }

    char* buf = new char[length + 1];
    std::snprintf(buf, length + 1, format, args...);

    std::string str(buf);
    delete[] buf;

    std::string loginfo = std::move(str);
    char backchar = loginfo[strlen(loginfo.c_str()) - 1];
    if (backchar != '\n') loginfo += "\n";

    writeToFile(loger_main, level, loginfo);
    if (level >= 1)
        std::cout << loginfo;
}

//
template< typename... Args >
void printf_tofile(FileWriter& loger, int level, const char* format, Args... args)
{
    int length = std::snprintf(nullptr, 0, format, args...);
    if (length <= 0) {
        return;
    }

    char* buf = new char[length + 1];
    std::snprintf(buf, length + 1, format, args...);

    std::string str(buf);
    delete[] buf;

    std::string loginfo = std::move(str);
    char backchar = loginfo[strlen(loginfo.c_str()) - 1];
    if (backchar != '\n') loginfo += "\n";

    writeToFile(loger, level, loginfo);
    if (level >= 1)
        std::cout << loginfo;
}




