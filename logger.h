#pragma once
#include <iostream>
#include <list>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>

class FileWriter
{
public:
    FileWriter(const std::string& filename, int level = 0, bool async = true);
    ~FileWriter();

    void resetlogfile();
    void writelog(const std::string& log_str);
    void writelog_async(const std::string& log_str);

public:
    int     log_level_;
    bool    log_async_;
    std::list<std::string> list_;

private:
    std::string     folder_;
    std::string     filename_;
    std::ofstream   file_;
    std::mutex      mutex_;//for file_
};

void writeToFile(FileWriter& writer, int level, const std::string& str);


