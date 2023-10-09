#include "public.h"

//get exe path 
#if defined WIN32
#include <direct.h>
std::string getexepath()
{
    char* buffer = NULL;
    buffer = _getcwd(NULL, 0);
    if (buffer)
    {
        std::string path = buffer;
        free(buffer);
        return path;
    }

    return "";
}
#else
#include <unistd.h>
std::string getexepath()
{
    char* buffer = NULL;
    buffer = getcwd(NULL, 0);
    if (buffer)
    {
        std::string path = buffer;
        free(buffer);
        return path;
    }

    return "";
}
#endif

//create folder
#ifdef _WIN32
    #include <direct.h>
    #define mkdir(x) _mkdir(x)
    bool create_directory(const char* path)
    {
        int status = 0;
        status = mkdir(path);
        if (status == 0)
        {
            std::cout << "Directory created at " << path << std::endl;
            return true;
        }
        else
        {
            std::cerr << "Unable to create directory at " << path << std::endl;
            return false;
        }
    }
    bool folderExists(const char* folderPath) {
        DWORD attributes = GetFileAttributesA(folderPath);
        return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
    }

#else

#include <sys/stat.h>
    bool create_directory(const char* path)
    {
        int status = 0;
        status = mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (status == 0)
        {
            std::cout << "Directory created at " << path << std::endl;
            return true;
        }
        else
        {
            std::cerr << "Unable to create directory at " << path << std::endl;
            return false;
        }
    }
    bool folderExists(const char* folderPath) {
        struct stat info;
        if (stat(folderPath, &info) != 0) {
            return false;
        }
        return (info.st_mode & S_IFDIR);
    }

#endif
bool create_directories(const char* path)
{
    if (folderExists(path))//create exist folder will false
        return true;
    std::string total_path = path;
    if (total_path.find(':') == std::string::npos)//must absolute path
        return false;

    std::string current_path = "";
    std::string delimiter = "/";
    size_t pos = 0;
    std::string token;

    int ntimes = 0;
    while ((pos = total_path.find(delimiter)) != std::string::npos)
    {
        token = total_path.substr(0, pos);
        current_path += token + delimiter;
        if (current_path.length() > 3 && !folderExists(current_path.c_str()))//a= D:/ not need create,b=create exist folder will false
            create_directory(current_path.c_str());
        total_path.erase(0, pos + delimiter.length());

        if (++ntimes > 10)//keep right
            break;
    }

    return create_directory(path);
}

//delete folder
#include <filesystem>
#include <experimental/filesystem>
//_SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING  忽略C++17 不使用<experimental/filesystem>警告
namespace fs = std::experimental::filesystem;
bool delete_directories(const char* path)
{
    fs::path folderPath(path);
    if (!fs::exists(folderPath))
        return true;

    if (fs::is_directory(folderPath))
    {
        for (const auto& entry : fs::directory_iterator(folderPath)) 
        {
            if (fs::is_directory(entry)) 
            {
                std::string filepath = entry.path().generic_string();
                delete_directories(filepath.c_str());
            }
            else 
            {
                fs::remove(entry.path());
            }
        }
        fs::remove(folderPath);
    }

    return true;
}

//
void unicode_to_utf8(const wchar_t* in, size_t len, std::string& out)
{
#if defined WIN32
    int out_len = ::WideCharToMultiByte(CP_UTF8, 0, in, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
    if (out_len > 0) {
        char* lpBuf = new char[static_cast<unsigned int>(out_len + 1)];
        if (lpBuf) {
            memset(lpBuf, 0, static_cast<unsigned int>(out_len + 1));
            int return_len = ::WideCharToMultiByte(CP_UTF8, 0, in, static_cast<int>(len), lpBuf, out_len, nullptr, nullptr);
            if (return_len > 0) out.assign(lpBuf, static_cast<unsigned int>(return_len));
            delete[]lpBuf;
        }
    }
#else
    /*if (wcslen(in) <= 0 || len <= 0) return;
    std::lock_guard<std::mutex> guard(g_ConvertCodeMutex);
    size_t w_len = len * 4 + 1;
    setlocale(LC_CTYPE, "en_US.UTF-8");
    std::unique_ptr<char[]> p(new char[w_len]);
    size_t return_len = wcstombs(p.get(), in, w_len);
    if (return_len > 0) out.assign(p.get(), return_len);
    setlocale(LC_CTYPE, "C");*/

    size_t w_len = len * (sizeof(wchar_t) / sizeof(char)) + 1U;
    char* save = new char[w_len];
    memset(save, '\0', w_len);
    iconv_convert("UTF-32", "UTF-8//IGNORE", save, w_len, (char*)(in), w_len);
    out = save;
    delete[]save;
#endif
}

void utf8_to_unicode(const char* in, size_t len, std::wstring& out)
{
#if defined WIN32
    int out_len = ::MultiByteToWideChar(CP_UTF8, 0, in, static_cast<int>(len), nullptr, 0);
    if (out_len > 0) {
        wchar_t* lpBuf = new wchar_t[static_cast<unsigned int>(out_len + 1)];
        if (lpBuf) {
            memset(lpBuf, 0, (static_cast<unsigned int>(out_len + 1)) * sizeof(wchar_t));
            int return_len = ::MultiByteToWideChar(CP_UTF8, 0, in, static_cast<int>(len), lpBuf, out_len);
            if (return_len > 0) out.assign(lpBuf, static_cast<unsigned int>(return_len));
            delete[]lpBuf;
        }
    }
#else
    /*if (strlen(in) <= 0 || len <= 0) return;
    std::lock_guard<std::mutex> guard(g_ConvertCodeMutex);
    setlocale(LC_CTYPE, "en_US.UTF-8");
    std::unique_ptr<wchar_t[]> p(new wchar_t[sizeof(wchar_t) * (len + 1)]);
    size_t return_len = mbstowcs(p.get(), in, len + 1);
    if (return_len > 0) out.assign(p.get(), return_len);
    setlocale(LC_CTYPE, "C");*/

    size_t w_len = len * (sizeof(wchar_t) / sizeof(char)) + 1U;
    char* save = new char[w_len];
    memset(save, '\0', w_len);
    iconv_convert("UTF-8", "UTF-32//IGNORE", save, w_len, (char*)(in), len);
    out = (wchar_t*)save + 1;
    delete[]save;
#endif
}

void ansi_to_unicode(const char* in, size_t len, std::wstring& out)
{
#if defined WIN32
    int out_len = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, in, static_cast<int>(len), nullptr, 0);
    if (out_len > 0) {
        wchar_t* lpBuf = new wchar_t[static_cast<unsigned int>(out_len + 1)];
        if (lpBuf) {
            memset(lpBuf, 0, (static_cast<unsigned int>(out_len + 1)) * sizeof(wchar_t));
            int return_len = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, in, static_cast<int>(len), lpBuf, out_len);
            if (return_len > 0) out.assign(lpBuf, static_cast<unsigned int>(return_len));
            delete[]lpBuf;
        }
    }
#else
    /*mbstate_t state;
    memset (&state, '\0', sizeof (state));
    std::lock_guard<std::mutex> guard(g_ConvertCodeMutex);
    //setlocale(LC_CTYPE, "zh_CN.UTF-8");
    setlocale(LC_CTYPE, "en_US.UTF-8");
    size_t out_len= mbsrtowcs(nullptr, &in, 0, &state);
    if (out_len > 0 &&  out_len < UINT_MAX && len > 0)
    {
        std::unique_ptr<wchar_t[]> lpBuf(new wchar_t[sizeof(wchar_t) * (out_len + 1)]);
        size_t return_len = mbsrtowcs(lpBuf.get(), &in, out_len+1, &state);
        if (return_len > 0) out.assign(lpBuf.get(), return_len);
    }
    setlocale(LC_CTYPE, "C");*/

    size_t w_len = len * (sizeof(wchar_t) / sizeof(char)) + 1U;
    char* save = new char[w_len];
    memset(save, '\0', w_len);

    iconv_convert("GBK", "UTF-32//IGNORE", save, w_len, (char*)(in), len);
    out = (wchar_t*)save + 1;
    delete[]save;
#endif
}

void unicode_to_ansi(const wchar_t* in, size_t len, std::string& out)
{
#if defined WIN32
    int out_len = ::WideCharToMultiByte(CP_ACP, 0, in, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
    if (out_len > 0) {
        char* lpBuf = new char[static_cast<unsigned int>(out_len + 1)];
        if (lpBuf) {
            memset(lpBuf, 0, static_cast<unsigned int>(out_len + 1));
            int return_len = ::WideCharToMultiByte(CP_ACP, 0, in, static_cast<int>(len), lpBuf, out_len, nullptr, nullptr);
            if (return_len > 0) out.assign(lpBuf, static_cast<unsigned int>(return_len));
            delete[]lpBuf;
        }
    }
#else
    /*mbstate_t state;
    memset(&state, '\0', sizeof(state));
    std::lock_guard<std::mutex> guard(g_ConvertCodeMutex);
    setlocale(LC_CTYPE, "en_US.UTF-8");
    size_t out_len = wcsrtombs(nullptr, &in, 0, &state);
    if (out_len > 0 && out_len < UINT_MAX && len > 0)
    {
        std::unique_ptr<char[]> lpBuf(new char[sizeof(char) *(out_len + 1)]);
        size_t return_len = wcsrtombs(lpBuf.get(), &in, out_len+1, &state);//wcstombs(lpBuf.get(), in, len + 1);
        if (return_len > 0) out.assign(lpBuf.get(), return_len);
    }
    setlocale(LC_CTYPE, "C");*/

    size_t w_len = len * (sizeof(wchar_t) / sizeof(char)) + 1U;
    char* save = new char[w_len];
    memset(save, '\0', w_len);
    iconv_convert("UTF-32", "GBK//IGNORE", save, w_len, (char*)(in), w_len - 1U);
    out = save;
    delete[]save;
#endif
}

void ansi_to_utf8(const char* in, size_t len, std::string& out)
{
#if defined WIN32
    std::wstring strUnicode;
    ansi_to_unicode(in, len, strUnicode);
    return unicode_to_utf8(strUnicode.c_str(), strUnicode.length(), out);
#else
    /*std::wstring strUnicode;
    ansi_to_unicode(in, len, strUnicode);
    return unicode_to_utf8(strUnicode.c_str(), strUnicode.length(), out);*/

    size_t w_len = len * (sizeof(wchar_t) / sizeof(char));
    char* save = new char[w_len];
    memset(save, '\0', w_len);
    iconv_convert("GBK", "UTF-8//IGNORE", save, w_len, (char*)(in), len);
    out = save;
    delete[]save;
#endif
}

void utf8_to_ansi(const char* in, size_t len, std::string& out)
{
#if defined WIN32
    std::wstring strUnicode;
    utf8_to_unicode(in, len, strUnicode);
    return unicode_to_ansi(strUnicode.c_str(), strUnicode.length(), out);
#else
    /*std::wstring strUnicode;
    utf8_to_unicode(in, len, strUnicode);
    return unicode_to_ansi(strUnicode.c_str(), strUnicode.length(), out);*/

    size_t w_len = len * (sizeof(wchar_t) / sizeof(char));
    char* save = new char[w_len];
    memset(save, '\0', w_len);
    iconv_convert("UTF-8", "GBK//IGNORE", save, w_len, (char*)(in), len);
    out = save;
    delete[]save;
#endif
}

//
int globalStrToIntDef(const LPTSTR valuechar, int& value, int defaultvalue, int base)
{
    if (base < 2 || base > 16) base = 10;
    value = defaultvalue;

    char* endptr;
    errno = 0;
    long val = strtol(valuechar, &endptr, base);
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0))
    {
        return -1;
    }

    if (endptr == const_cast<char*>(valuechar))//(char*)valuechar)//没有找到有效的字符
    {
        return -1;
    }
    if (*endptr != '\0')
    {
        //value = (int)val;
        //return 0;
        return -1;
    }

    value = static_cast<int>(val);//(int)val;
    return 1;
}

int globalStrToInt(const LPTSTR valuechar, int defaultvalue, int base)
{
    if (base < 2 || base > 16) base = 10;

    char* endptr;
    errno = 0;
    long val = strtol(valuechar, &endptr, base);
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0))
    {
        return defaultvalue;
    }

    if (endptr == const_cast<char*>(valuechar))//(char*)valuechar)//没有找到有效的字符
    {
        return defaultvalue;
    }
    if (*endptr != '\0')
    {
        return -1;
    }

    return static_cast<int>(val);
}

void globalSpliteString(const std::string& str, std::vector<std::string>& strVector, std::string splitStr, int maxCount)
{
    std::string::size_type pos = 0U;
    std::string::size_type pre = 0U;
    std::string::size_type len = str.length();
    if (splitStr.empty()) splitStr = ("|");
    std::string::size_type splitLen = splitStr.length();
    std::string curStr;
    if (maxCount < 1) maxCount = 1;
    do {
        if (static_cast<int>(strVector.size()) >= maxCount - 1) {//如果超过最大个数则不解析最后的分隔字符串，直接将其放在最后一个里面
            curStr = str.substr(pre, len - pre);
            strVector.push_back(curStr);
            break;
        }
        pos = str.find(splitStr, pre);
        if (pos == 0U) {
            pre = pos + splitLen;
            continue;
        }
        else if (pos == std::string::npos) {
            curStr = str.substr(pre, len - pre);
        }
        else {
            curStr = str.substr(pre, pos - pre);
            pre = pos + splitLen;
        }
        strVector.push_back(curStr);
    } while (pos != std::string::npos && pos != (len - splitLen));
}

void globalCreateGUID(std::string& GuidStr)
{
    std::string result = getguidtext();
    GuidStr = result;
}

//
unsigned short Checksum(const unsigned short* buf, int size)
{
    unsigned long sum = 0;

    if (buf != (unsigned short*)NULL)
    {
        while (size > 1)
        {
            sum += *(buf++);
            if (sum & 0x80000000)
            {
                sum = (sum & 0xffff) + (sum >> 16);
            }
            size -= sizeof(unsigned short);
        }

        if (size)
        {
            sum += (unsigned short)*(const unsigned char*)buf;
        }
    }

    while (sum >> 16)
    {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return (unsigned short)~sum;
}

bool str_existsubstr(std::string str, std::string substr)
{
    if (str.find(substr) != std::string::npos) {
        return true;
    }
    return false;
}

bool str_prefixsame(const std::string str, const std::string prefix)
{
    int len = prefix.length();
    if (std::strncmp(str.c_str(), prefix.c_str(), len) == 0)
        return true;

    return false;
}

std::string str_replace(std::string str, std::string old, std::string now)
{
    if (str.empty())
        return "";

    int oldPos = 0;
    while (str.find(old, oldPos) != -1)
    {
        int start = str.find(old, oldPos);
        str.replace(start, old.size(), now);
        oldPos = start + now.size();
    }
    return str;
}

std::string jsonstr_replace(std::string jsonString)
{
    std::string result = jsonString;
    result = str_replace(result, "\"", "");
    result = str_replace(result, "'",  "");
    result = str_replace(result, "\n", "");
    result = str_replace(result, "\r", "");
    result = str_replace(result, "\t", "");
    result = str_replace(result, "\\", "");

    return result;
}

std::string getnodevalue(std::string info, std::string nodename)
{
    info.erase(std::remove(info.begin(), info.end(), '\n'), info.end());
    std::string result = "";
    std::string start = "", end = "";
    char buff[256] = { 0 };
    snprintf(buff, 256, "<%s>", nodename.c_str()); start = buff;
    snprintf(buff, 256, "</%s>", nodename.c_str()); end = buff;

    int ns = info.find(start);
    int ne = info.find(end);
    if (ns != std::string::npos && ne != std::string::npos && ne > ns)
    {
        int offset = nodename.length();
        result = info.substr(ns + offset + 2, ne - ns - offset - 2);
    }

    return result;
}

std::string getDispositionValue(const std::string source, int pos, const std::string name)
{
    //头部内容：Content-Disposition: form-data; name="projectName"
    //构建模式串
    std::string pattern = " " + name + "=";
    int i = source.find(pattern, pos);
    //更换格式继续查找位置
    if (i < 0) {
        pattern = ";" + name + "=";
        i = source.find(pattern, pos);
    }
    if (i < 0) {
        pattern = name + "=";
        i = source.find(pattern, pos);
    }
    //尝试了可能的字符串，还没有找到，返回空字符串        
    if (i < 0) { return std::string(); }

    i += pattern.size();
    if (source[i] == '\"') {
        ++i;
        int j = source.find('\"', i);
        if (j < 0 || i == j) { return std::string(); }
        return source.substr(i, j - i);
    }
    else {
        int j = source.find(";", i);
        if (j < 0) { j = source.size(); }
        auto value = source.substr(i, j - i);
        //去掉前后的空白字符
        return trim(value);
    }
}

std::string gettimetext_now()
{
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    char buf[100] = { 0 };
    //std::strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S", std::localtime(&now));//12
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));//24
    return buf;
}
long long gettimecount_now()
{
    long long set = 0;
    time_t timep;
    set = time(&timep);

    return set;
}
bool gettimecount_interval(std::string timetext_start, std::string timetext_end, long long& interval)//用于计算两个时间的差值,单位为秒
{
    std::string timestart = "", timeend = "";
    timestart = str_replace(timetext_start, " ", "-"); timestart = str_replace(timestart, ":", "-");
    timeend = str_replace(timetext_end, " ", "-"); timeend = str_replace(timeend, ":", "-");

    std::vector<std::string> vecStart,vecEnd;
    globalSpliteString(timestart, vecStart, std::string("-"));
    globalSpliteString(timeend, vecEnd, std::string("-"));
    if (vecStart.size() != 6 || vecEnd.size() != 6)
        return false;

    long long year = max(0,atoi(vecEnd[0].c_str()) - atoi(vecStart[0].c_str()));
    long long month = max(0,atoi(vecEnd[1].c_str()) - atoi(vecStart[1].c_str()));
    long long day = max(0,atoi(vecEnd[2].c_str()) - atoi(vecStart[2].c_str()));
    long long H = max(0,atoi(vecEnd[3].c_str()) - atoi(vecStart[3].c_str()));
    long long M = max(0,atoi(vecEnd[4].c_str()) - atoi(vecStart[4].c_str()));
    long long S = max(0,atoi(vecEnd[5].c_str()) - atoi(vecStart[5].c_str()));

    interval = 0;
    interval += (year * (long long)946080000);
    interval += (month * (long long)2592000);
    interval += (day * (long long)86400);
    interval += (H * (long long)3600);
    interval += (M * (long long)60);
    interval += (S);

    return true;
}

std::string gettime_custom()//用于判断某时段,进行定时操作
{
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    char buf[100] = { 0 };
    std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", std::localtime(&now));//24
    return buf;
}

std::string gettimetext_millisecond(int millisecond)
{
    std::string result = "00:00:00";
    if (millisecond <= 0)
        return result;

    int H = (millisecond / (3600 * 1000));
    int M = (millisecond % (3600 * 1000)) / (60 * 1000);
    int S = (millisecond % (60 * 1000)) / 1000;

    char buff[256] = { 0 };
    snprintf(buff, 256, "%02d:%02d:%02d", H, M, S); result = buff;

    return result;
}
std::string gettimetext_framecount(int framecount, double dfps)
{
    int fps = (int)dfps;
    std::string result = "00:00:00";
    if (framecount <= 0 || fps <= 0)
        return result;

    int H = (framecount / (3600 * fps));
    int M = (framecount % (3600 * fps)) / (60 * fps);
    int S = (framecount % (60 * fps)) / fps;

    char buff[256] = { 0 };
    snprintf(buff, 256, "%02d:%02d:%02d", H, M, S); result = buff;

    return result;
}

std::string gettimeshow_second(long long second)
{
    std::string result = "0天00时00分00秒";
    if (second <= 0)
        return result;

    int D = (second / 86400);
    int H = (second % 86400) / 3600;
    int M = (second % 3600) / 60;
    int S = (second % 60);

    char buff[256] = { 0 };
    snprintf(buff, 256, "%d天%02d时%02d分%02d秒", D, H, M, S); result = buff;

    return result;
}
std::string gettimeshow_day(long long second)
{
    std::string result = "0天";
    if (second <= 0)
        return result;

    int D = (second / 86400);

    char buff[256] = { 0 };
    snprintf(buff, 256, "%d天", D); result = buff;

    return result;
}


bool is_existfile(const char* filename)
{
#if defined WIN32
    return (_taccess(filename, 0) == 0);
#else
    return (access(filename, 0) == 0);
#endif
}

bool is_imagefile(const char* filename)
{
    std::string str_filename = filename;
    std::transform(str_filename.begin(), str_filename.end(), str_filename.begin(), ::tolower);
    if (str_filename.find(".jpg") != std::string::npos)
        return true;
    if (str_filename.find(".jpeg") != std::string::npos)
        return true;
    if (str_filename.find(".png") != std::string::npos)
        return true;
    if (str_filename.find(".bmp") != std::string::npos)
        return true;

    return false;
    //不使用以下方法判断文件格式
    FILE* fp = fopen(filename, "rb");
    if (!fp) 
        return false;
  
    unsigned char buf[8];
    fread(buf, 1, 8, fp);  // read header
    fclose(fp);

    if (memcmp(buf, "\xFF\xD8\xFF", 3) == 0) {  
        // JPEG
        return true;
    }
    if (memcmp(buf, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0) {  
        // PNG
        return true;
    }
    if (memcmp(buf, "\x47\x49\x46\x38\x37\x61", 6) == 0 || memcmp(buf, "\x47\x49\x46\x38\x39\x61", 6) == 0) {  
        // GIF
        return true;
    }
    if (memcmp(buf, "\x42\x4D", 2) == 0) {  
        // BMP
        return true;
    }

    return false;
}

bool is_videofile(const char* filename)
{
    std::string str_filename = filename;
    std::transform(str_filename.begin(), str_filename.end(), str_filename.begin(), ::tolower);
    if (str_filename.find(".mp4") != std::string::npos)
        return true;
    if (str_filename.find(".avi") != std::string::npos)
        return true;

    return false;
    //不使用以下方法判断文件格式
    FILE* fp = fopen(filename, "rb");
    if (!fp)
        return false;
    
    unsigned char buf[4];
    fread(buf, 1, 4, fp); // read header
    fclose(fp);

    if (buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x01 && buf[3] == 0xba) {
        return true; // MPEG
    }
    if (buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x01 && buf[3] == 0xb3) {
        return true; // MPEG-2
    }
    if (buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x01 && buf[3] == 0xb6) {
        return true; // MPEG-2
    }
    if (buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x01 && buf[3] == 0xe0) {
        return true; // MPEG-1
    }
    if (buf[0] == 0x52 && buf[1] == 0x49 && buf[2] == 0x46 && buf[3] == 0x46) {
        return true; // AVI
    }
    if (buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x00 && buf[3] == 0x1c) {
        return true; // QuickTime
    }
    if (buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x00 && buf[3] == 0x20) {
        return true; // MP4
    }

    return false;
}

bool is_audiofile(const char* filename)
{
    std::string str_filename = filename;
    std::transform(str_filename.begin(), str_filename.end(), str_filename.begin(), ::tolower);
    if (str_filename.find(".mp3") != std::string::npos)
        return true;
    if (str_filename.find(".wav") != std::string::npos)
        return true;

    return false;
    //不使用以下方法判断文件格式
    FILE* fp = fopen(filename, "rb");
    if (!fp)
        return false;

    unsigned char buf[4];
    fread(buf, 1, 4, fp); // read header
    fclose(fp);

    if (buf[0] == 'R' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == 'F') {
        // WAV audio file
        return true;
    }
    else if (buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3') {
        // MP3 audio file
        return true;
    }
    else if (buf[0] == 'O' && buf[1] == 'g' && buf[2] == 'g' && buf[3] == 'S') {
        // OGG audio file
        return true;
    }
    else {
        // Not an audio file
        return false;
    }

    return false;
}


std::string get_path_folder(std::string filepath)
{
    std::string folder = "";
    //从最后一个反斜杠或正斜杠处分离目录名
    size_t pos = filepath.find_last_of("/\\");
    if (pos != std::string::npos) { 
        folder = filepath.substr(0, pos);
    }
    return folder;
}

std::string get_path_name(std::string filepath)
{
    std::string name = "";
    //从最后一个反斜杠或正斜杠处分离文件名
    size_t pos = filepath.find_last_of("\\/");
    if(pos != std::string::npos)
        name = filepath.substr(pos + 1);

    return name;
}

std::string get_path_extension(std::string filepath)
{
    std::string extension;
    //从最后一个点处分离文件名
    char* dot = (char*)strrchr(filepath.c_str(), '.');
    if (!dot || dot == filepath.c_str())
        return extension;
    extension = dot + 1;

    return extension;
}

bool read_file(const char* filename, char*& file_buff, long& file_size)
{
    if (filename == nullptr)
        return false;

    FILE* fp = nullptr;
    fp = fopen(filename, "rb");
    if (fp == nullptr) {
        return false;
    }

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);

    file_buff = (char*)malloc(file_size + 1);
    if (file_buff == nullptr) {
        return false;
    }

    fread(file_buff, file_size, 1, fp);
    fclose(fp);
    file_buff[file_size] = '\0';

    return true;
}

bool write_file(const char* filename, char* file_buff, long file_size)
{
    if (filename == nullptr)
        return false;
    if (file_buff == nullptr || file_size<= 0)
        return false;

    FILE* fp = nullptr;
    fp = fopen(filename, "wb");
    if (fp == nullptr) {
        return false;
    }

    fwrite(file_buff, sizeof(char), file_size, fp);
    fclose(fp);

    return true;
}




