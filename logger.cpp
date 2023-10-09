#include "logger.h"
#include "public.h"

#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>

static std::string localtime()
{
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    char buf[100] = { 0 };
    //std::strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S", std::localtime(&now));//12
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));//24
    return buf;
}

static std::string logfilefolder()
{
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::string daytime = "";
    char buf[100] = { 0 };
    std::strftime(buf, sizeof(buf), "%Y%m%d", std::localtime(&now));//24
    daytime = buf;
    
    std::string apppath = getexepath(); apppath = str_replace(apppath, std::string("\\"), std::string("/"));
    std::string logfolder = apppath + std::string("/logfile/");
    logfolder += daytime;

    return logfolder;
}

static std::string timeprefix()
{
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    char buf[100] = { 0 };
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H_%M_%S", std::localtime(&now));//24
    return buf;
}

//
pthread_mutex_t mutex_list;
pthread_t threadid_writelog;
void* pthread_writelog(void* arg)
{
    FileWriter* pWriter = (FileWriter*)arg;
    if (pWriter)
    {
        while (true)
        {
            _sleep(10);
            if (!pWriter->list_.empty())
            {
                pthread_mutex_lock(&mutex_list);
                std::string log_str = pWriter->list_.front();
                pWriter->list_.pop_front();
                pthread_mutex_unlock(&mutex_list);

                pWriter->writelog(log_str);
            }
        }
    }

    return nullptr;
}

FileWriter::FileWriter(const std::string& filename, int level, bool async) 
    : log_level_(level)
    , log_async_(async)
    , filename_(filename)
    , folder_(logfilefolder())
{
    std::string logfilepath = filename_;
    if (create_directories(folder_.c_str()))
        logfilepath = folder_ + "/" + timeprefix() + filename_;
    file_.open(logfilepath, std::ios::out | std::ios::app);

    pthread_mutex_init(&mutex_list, NULL);
    int ret = pthread_create(&threadid_writelog, nullptr, pthread_writelog, this);
    if (ret != 0)
        log_async_ = false; 
}

FileWriter::~FileWriter() 
{
    file_.close();
}

void FileWriter::resetlogfile()
{
    std::string newfolder_ = logfilefolder();
    if (newfolder_ != folder_)
    {
        std::lock_guard<std::mutex> lock(mutex_);//局部变量，退出函数自动释放
        file_.flush();
        file_.close();
        folder_ = newfolder_;

        //open newfile
        std::string logfilepath = filename_;
        if (create_directories(folder_.c_str()))
            logfilepath = folder_ + "/" + timeprefix() + filename_;
        file_.open(logfilepath, std::ios::out | std::ios::app);
    }
}

void FileWriter::writelog(const std::string& log_str)
{
    resetlogfile();
    std::lock_guard<std::mutex> lock(mutex_);//局部变量，退出函数自动释放
    
    file_ << log_str;
    file_.flush();
}

void FileWriter::writelog_async(const std::string& log_str)
{
    pthread_mutex_lock(&mutex_list);
    list_.push_back(log_str);
    pthread_mutex_unlock(&mutex_list);
}

void writeToFile(FileWriter& writer, int level, const std::string& str) 
{
    std::string format_str = "", level_type = "";
    if (level == 0) level_type = "INFO";
    if (level == 1) level_type = "MAIN";

    format_str += localtime();
    format_str += "  ";
    format_str += level_type;
    format_str += "  ";
    format_str += str;
    
    char backchar = str[strlen(str.c_str()) - 1];
    if(backchar!='\n') format_str += "\n";

    if (level >= writer.log_level_)
    {
        if(writer.log_async_)
            writer.writelog_async(format_str);
        else
            writer.writelog(format_str);
    }   
}

