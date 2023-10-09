#pragma once
#include <stdio.h>
#include <time.h>
#include <iostream>
#include <clocale>
#include <chrono>
#include <codecvt>
#include <cstdlib>
#include <random>
#include <sstream>
#include <iomanip>
#include <vector>
#include <list>
#include <random>
#include <algorithm>

#if defined WIN32
#include <io.h>
#include <windows.h>
#include <tchar.h>
#else
#include <unistd.h>
#endif

#include "loghelp.h"
#define _debug_to printf_tofile  
#define CHECK_CONFIG(name,value,error) {if(value.empty()){error=std::string(name)+" is empty,please check config... ";return false;}}

#define maxvalue(x, y)  ((x) > (y) ? (x) : (y))
#define minvalue(x, y)  ((x) < (y) ? (x) : (y))

#define LIBEVENT_USE_OPENSSL    1       

inline std::string getfolder_appdata() 
{
    const char* appDataPath = std::getenv("APPDATA");
    if (appDataPath != nullptr) 
    {
        std::string folderPath = appDataPath;
        return folderPath;
    }
    else 
    {
        return ""; // 如果无法获取到APPDATA环境变量，则返回空字符串
    }
}

inline std::string& trim(std::string& s)
{
    if (s.empty()) { return s; }
    s.erase(0, s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
    return s;
}

inline std::string getguidtext()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    for (int i = 0; i < 32; ++i) 
    {
        int randNum = dis(gen);
        ss << std::hex << randNum;
    }

    std::string guid = ss.str();
//  guid.insert(8, "-");
//  guid.insert(13, "-");
//  guid.insert(18, "-");
//  guid.insert(23, "-");

    return guid;
}

inline unsigned int getrandomnum(int min = 10000, int max = 100000000)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned int> dis(min, max);
    return dis(gen);
}

inline std::string url_encode(const std::string& input) 
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : input) 
    {
        // 保留字符：字母、数字、连字符、点、下划线和波浪线、英文冒号、左斜杠
        if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~' || c == ':' || c == '/')
        {
            escaped << c;
        }
        else 
        {
            // 转义其他字符为%xx的形式
            escaped << '%' << std::setw(2) << int((unsigned char)c);
        }
    }

    return escaped.str();
}

inline std::string url_decode(const std::string& encodedUrl)
{
    std::ostringstream decodedUrl;

    for (std::size_t i = 0; i < encodedUrl.length(); ++i) 
    {
        if (encodedUrl[i] == '%') 
        {
            std::string hexCode = encodedUrl.substr(i + 1, 2);
            int charCode;
            std::istringstream(hexCode) >> std::hex >> charCode;
            decodedUrl << static_cast<char>(charCode);
            i += 2;
        }
        else if (encodedUrl[i] == '+') 
        {
            decodedUrl << ' ';
        }
        else 
        {
            decodedUrl << encodedUrl[i];
        }
    }

    return decodedUrl.str();
}

//
std::string getexepath();
bool create_directories(const char* path);
bool delete_directories(const char* path);

//
void unicode_to_utf8(const wchar_t* in, size_t len, std::string& out);

void utf8_to_unicode(const char* in, size_t len, std::wstring& out);

void ansi_to_unicode(const char* in, size_t len, std::wstring& out);

void unicode_to_ansi(const wchar_t* in, size_t len, std::string& out);

void ansi_to_utf8(const char* in, size_t len, std::string& out);

void utf8_to_ansi(const char* in, size_t len, std::string& out);

//
int globalStrToIntDef(const LPTSTR valuechar, int& value, int defaultvalue = -1, int base = 10);

int globalStrToInt(const LPTSTR valuechar, int defaultvalue, int base);

void globalSpliteString(const std::string& str, std::vector<std::string>& strVector, std::string splitStr, int maxCount = 1024);

void globalCreateGUID(std::string& GuidStr);

//
unsigned short Checksum(const unsigned short* buf, int size);

bool str_existsubstr(std::string str, std::string substr);

bool str_prefixsame(const std::string str, const std::string prefix);

std::string str_replace(std::string str, std::string old, std::string now);

std::string jsonstr_replace(std::string jsonString);


std::string getnodevalue(std::string info, std::string nodename);

std::string getDispositionValue(const std::string source, int pos, const std::string name);

std::string gettimetext_now();
long long   gettimecount_now();
bool        gettimecount_interval(std::string timetext_start, std::string timetext_end, long long& interval);

std::string gettime_custom();

std::string gettimetext_millisecond(int millisecond);
std::string gettimetext_framecount(int framecount, int fps);

std::string gettimeshow_second(long long second);
std::string gettimeshow_day(long long second);

//
bool is_existfile(const char* filename);

bool is_imagefile(const char* filename);

bool is_videofile(const char* filename);

bool is_audiofile(const char* filename);


std::string get_path_folder(std::string filepath);

std::string get_path_name(std::string filepath);

std::string get_path_extension(std::string filepath);



bool read_file(const char* filename, char*& file_buff, long& file_size);

bool write_file(const char* filename, char* file_buff, long file_size);


//////////////////////////////////////////////////////////////////////////////////////////////////////////

//picture
#if 1
namespace picture
{

#define MAKEUS(a, b)	((unsigned short) ( ((unsigned short)(a))<<8 | ((unsigned short)(b)) ))
#define MAKEUI(a,b,c,d) ((unsigned int) ( ((unsigned int)(a)) << 24 | ((unsigned int)(b)) << 16 | ((unsigned int)(c)) << 8 | ((unsigned int)(d)) ))

#define M_DATA  0x00
#define M_SOF0  0xc0
#define M_DHT   0xc4
#define M_SOI	0xd8
#define M_EOI   0xd9
#define M_SOS   0xda
#define M_DQT	0xdb
#define M_DNL   0xdc
#define M_DRI   0xdd
#define M_APP0  0xe0
#define M_APPF	0xef
#define M_COM   0xfe

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

    static int GetBMPWidthHeight(const char* path, unsigned int* punWidth, unsigned int* punHeight) 
    {
        FILE* pfRead = fopen(path, "rb");
        if (!pfRead) {
            _debug_to(1, ("[GetBMPWidthHeight]:can't open file, %s\n"), path);
            return -1; // 打开文件失败
        }

        uint8_t header[54];
        fread(header, sizeof(uint8_t), 54, pfRead); // 读取bmp文件头

        // 获取宽度和高度
        *punWidth = *(int*)&header[18];
        *punHeight = *(int*)&header[22];

        fclose(pfRead);
        return 0;
    }

    static int GetPNGWidthHeight(const char* path, unsigned int* punWidth, unsigned int* punHeight)
    {
        int Finished = 0;
        unsigned char format[8];
        unsigned char width_height[4];
        FILE* pfRead;

        *punWidth = 0;
        *punHeight = 0;

        if (fopen_s(&pfRead, path, "rb") != 0)
        {
            _debug_to(1, ("[GetPNGWidthHeight]:can't open file, %s\n"), path);
            return -1;
        }

        for (int i = 0; i < 8; i++)
            fread(&format[i], sizeof(unsigned char), 1, pfRead);
        if ((MAKEUI(format[0], format[1], format[2], format[3]) == 0x89504e47) && (MAKEUI(format[4], format[5], format[6], format[7]) == 0x0d0a1a0a))
        {
            fseek(pfRead, 16, SEEK_SET);
            for (int i = 0; i < 4; i++)
                fread(&width_height[i], sizeof(unsigned char), 1, pfRead);
            *punWidth = MAKEUI(width_height[0], width_height[1], width_height[2], width_height[3]);
            for (int i = 0; i < 4; i++)
                fread(&width_height[i], sizeof(unsigned char), 1, pfRead);
            *punHeight = MAKEUI(width_height[0], width_height[1], width_height[2], width_height[3]);
        }

        fclose(pfRead);
        return 0;
    }

    static int GetJPGWidthHeight(const char* path, unsigned int* punWidth, unsigned int* punHeight)
    {
        int Finished = 0;
        unsigned char id, ucHigh, ucLow;
        FILE* pfRead;

        *punWidth = 0;
        *punHeight = 0;

        if (fopen_s(&pfRead, path, "rb") != 0)
        {
            _debug_to(1, ("[GetJPGWidthHeight]:can't open file:%s\n"), path);
            return -1;
        }

        while (!Finished)
        {
            if (!fread(&id, sizeof(char), 1, pfRead) || id != 0xff || !fread(&id, sizeof(char), 1, pfRead))
            {
                Finished = -2;
                break;
            }

            if (id >= M_APP0 && id <= M_APPF)
            {
                fread(&ucHigh, sizeof(char), 1, pfRead);
                fread(&ucLow, sizeof(char), 1, pfRead);
                fseek(pfRead, (long)(MAKEUS(ucHigh, ucLow) - 2), SEEK_CUR);
                continue;
            }

            switch (id)
            {
            case M_SOI:
                break;

            case M_COM:
            case M_DQT:
            case M_DHT:
            case M_DNL:
            case M_DRI:
                fread(&ucHigh, sizeof(char), 1, pfRead);
                fread(&ucLow, sizeof(char), 1, pfRead);
                fseek(pfRead, (long)(MAKEUS(ucHigh, ucLow) - 2), SEEK_CUR);
                break;

            case M_SOF0:
                fseek(pfRead, 3L, SEEK_CUR);
                fread(&ucHigh, sizeof(char), 1, pfRead);
                fread(&ucLow, sizeof(char), 1, pfRead);
                *punHeight = (unsigned int)MAKEUS(ucHigh, ucLow);
                fread(&ucHigh, sizeof(char), 1, pfRead);
                fread(&ucLow, sizeof(char), 1, pfRead);
                *punWidth = (unsigned int)MAKEUS(ucHigh, ucLow);
                Finished = 0;

            case M_SOS:
            case M_EOI:
            case M_DATA:
                Finished = -1;
                break;

            default:
                fread(&ucHigh, sizeof(char), 1, pfRead);
                fread(&ucLow, sizeof(char), 1, pfRead);
                _debug_to(1, ("[GetJPGWidthHeight]:unknown id: 0x%x ;  length=%hd\n"), id, MAKEUS(ucHigh, ucLow));
                if (fseek(pfRead, (long)(MAKEUS(ucHigh, ucLow) - 2), SEEK_CUR) != 0)
                    Finished = -2;
                break;
            }
        }

        if (Finished == -1)
            _debug_to(1, ("[GetJPGWidthHeight]:can't find SOF0!\n"));
        else if (Finished == -2)
            _debug_to(1, ("[GetJPGWidthHeight]:jpeg format error!\n"));

        fclose(pfRead);
        return Finished;
    }

    static int GetJPEGWidthHeight(const char* path, unsigned int* punWidth, unsigned int* punHeight)
    {
        int Finished = 0;
        unsigned char marker1, marker2, ucHigh, ucLow;
        FILE* pfRead;

        *punWidth = 0;
        *punHeight = 0;

        if (fopen_s(&pfRead, path, "rb") != 0)
        {
            _debug_to(1, ("[GetJPEGWidthHeight]:can't open file:%s\n"), path);
            return -1;
        }

        while (!Finished)
        {
            if (!fread(&marker1, sizeof(char), 1, pfRead) || !fread(&marker2, sizeof(char), 1, pfRead))
            {
                Finished = -2;
                break;
            }

            if (marker1 == 0xFF && marker2 == 0xD8) 
            {
                // 找到SOF0, 该标记后分别为图片的高度(2字节)、图片的宽度(2字节)
                unsigned int readlen = 0;
                unsigned short marker;
                while ((readlen<256) && (fread(&marker, sizeof(short), 1, pfRead)))
                {
                    readlen += 2;
                    if (marker == 0xC2FF)
                    {
                        fseek(pfRead, 3L, SEEK_CUR);

                        fread(&ucHigh, sizeof(char), 1, pfRead);
                        fread(&ucLow, sizeof(char), 1, pfRead);
                        *punHeight = (unsigned int)MAKEUS(ucHigh, ucLow);
                        fread(&ucHigh, sizeof(char), 1, pfRead);
                        fread(&ucLow, sizeof(char), 1, pfRead);
                        *punWidth = (unsigned int)MAKEUS(ucHigh, ucLow);
                        Finished = 0;
                        break;
                    }
                }
            }
            break;
        }
        fclose(pfRead);

        return Finished;
    }

    static void GetPicInfomation(const char* path, unsigned int* pWidth, unsigned int* pHeight, unsigned int* pBitCount, std::string& format)
    {
        int len = strlen(path);
        if (len <= 4)
        {
            *pWidth = 0; *pHeight = 0;
            return;
        }

        if (!strncmp(path + len - 3, "bmp", 3))
        {
            GetBMPWidthHeight(path, pWidth, pHeight);
            *pBitCount = 24;
            format = "bmp";
        }
        else if (!strncmp(path + len - 3, "png", 3))
        {
            GetPNGWidthHeight(path, pWidth, pHeight);
            *pBitCount = 32;
            format = "png";
        }
        else if ((!strncmp(path + len - 3, "jpg", 3)))
        {
            GetJPGWidthHeight(path, pWidth, pHeight);
            *pBitCount = 32;
            format = "jpg";
        }
        else if ((!strncmp(path + len - 4, "jpeg", 4)))
        {
            GetJPEGWidthHeight(path, pWidth, pHeight);
            *pBitCount = 32;
            format = "jpeg";
        }
        else
        {
            *pWidth = 0; *pHeight = 0;
            *pBitCount = 32;
            format = "";
            _debug_to(1, ("[GetPicWidthHeight]: not support picture type...[%s]\n"), path);
        }
    }
}
#endif

//md5
#if 1
namespace md5
{
    typedef struct
    {
        unsigned int count[2];
        unsigned int state[4];
        unsigned char buffer[64];
    }MD5_CTX;

#define MD5_F(x, y, z)  ((x & y) | (~x & z))
#define MD5_G(x, y, z)  ((x & z) | (y & ~z))
#define MD5_H(x, y, z)  (x ^ y ^ z)
#define MD5_I(x, y, z)  (y ^ (x | ~z))
#define MD5_ROTATE_LEFT(x, n)   ((x << n) | (x >> (32 - n)))
#define MD5_FF(a, b, c, d, x, s, ac) \
        { \
        a += MD5_F(b, c, d) + x + ac; \
        a = MD5_ROTATE_LEFT(a, s); \
        a += b; \
        }
#define MD5_GG(a, b, c, d, x, s, ac) \
        { \
        a += MD5_G(b, c, d) + x + ac; \
        a = MD5_ROTATE_LEFT(a, s); \
        a += b; \
        }
#define MD5_HH(a, b, c, d, x, s, ac) \
        { \
        a += MD5_H(b, c, d) + x + ac; \
        a = MD5_ROTATE_LEFT(a, s); \
        a += b; \
        }
#define MD5_II(a, b, c, d, x, s, ac) \
        { \
        a += MD5_I(b, c, d) + x + ac; \
        a = MD5_ROTATE_LEFT(a, s); \
        a += b; \
        }

    static unsigned char PADDING[] =
    {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    static void MD5Init(MD5_CTX* context)
    {
        context->count[0] = 0;
        context->count[1] = 0;
        context->state[0] = 0x67452301;
        context->state[1] = 0xEFCDAB89;
        context->state[2] = 0x98BADCFE;
        context->state[3] = 0x10325476;
    }

    static void MD5Decode(unsigned int* output, unsigned char* input, unsigned int len)
    {
        unsigned int i = 0, j = 0;
        while (j < len) {
            output[i] = (input[j]) |
                (input[j + 1] << 8) |
                (input[j + 2] << 16) |
                (input[j + 3] << 24);
            i++;
            j += 4;
        }
    }

    static void MD5Encode(unsigned char* output, unsigned int* input, unsigned int len)
    {
        unsigned int i = 0, j = 0;
        while (j < len) {
            output[j] = input[i] & 0xFF;
            output[j + 1] = (input[i] >> 8) & 0xFF;
            output[j + 2] = (input[i] >> 16) & 0xFF;
            output[j + 3] = (input[i] >> 24) & 0xFF;
            i++;
            j += 4;
        }
    }

    static void MD5Transform(unsigned int state[4], unsigned char block[64])
    {
        unsigned int a = state[0];
        unsigned int b = state[1];
        unsigned int c = state[2];
        unsigned int d = state[3];
        unsigned int x[64];
        MD5Decode(x, block, 64);
        MD5_FF(a, b, c, d, x[0], 7, 0xd76aa478);    // 1
        MD5_FF(d, a, b, c, x[1], 12, 0xe8c7b756);   // 2
        MD5_FF(c, d, a, b, x[2], 17, 0x242070db);   // 3
        MD5_FF(b, c, d, a, x[3], 22, 0xc1bdceee);   // 4
        MD5_FF(a, b, c, d, x[4], 7, 0xf57c0faf);    // 5
        MD5_FF(d, a, b, c, x[5], 12, 0x4787c62a);   // 6
        MD5_FF(c, d, a, b, x[6], 17, 0xa8304613);   // 7
        MD5_FF(b, c, d, a, x[7], 22, 0xfd469501);   // 8
        MD5_FF(a, b, c, d, x[8], 7, 0x698098d8);    // 9
        MD5_FF(d, a, b, c, x[9], 12, 0x8b44f7af);   // 10
        MD5_FF(c, d, a, b, x[10], 17, 0xffff5bb1);  // 11
        MD5_FF(b, c, d, a, x[11], 22, 0x895cd7be);  // 12
        MD5_FF(a, b, c, d, x[12], 7, 0x6b901122);   // 13
        MD5_FF(d, a, b, c, x[13], 12, 0xfd987193);  // 14
        MD5_FF(c, d, a, b, x[14], 17, 0xa679438e);  // 15
        MD5_FF(b, c, d, a, x[15], 22, 0x49b40821);  // 16

        MD5_GG(a, b, c, d, x[1], 5, 0xf61e2562);    // 17
        MD5_GG(d, a, b, c, x[6], 9, 0xc040b340);    // 18
        MD5_GG(c, d, a, b, x[11], 14, 0x265e5a51);  // 19
        MD5_GG(b, c, d, a, x[0], 20, 0xe9b6c7aa);   // 20
        MD5_GG(a, b, c, d, x[5], 5, 0xd62f105d);    // 21
        MD5_GG(d, a, b, c, x[10], 9, 0x2441453);    // 22
        MD5_GG(c, d, a, b, x[15], 14, 0xd8a1e681);  // 23
        MD5_GG(b, c, d, a, x[4], 20, 0xe7d3fbc8);   // 24
        MD5_GG(a, b, c, d, x[9], 5, 0x21e1cde6);    // 25
        MD5_GG(d, a, b, c, x[14], 9, 0xc33707d6);   // 26
        MD5_GG(c, d, a, b, x[3], 14, 0xf4d50d87);   // 27
        MD5_GG(b, c, d, a, x[8], 20, 0x455a14ed);   // 28
        MD5_GG(a, b, c, d, x[13], 5, 0xa9e3e905);   // 29
        MD5_GG(d, a, b, c, x[2], 9, 0xfcefa3f8);    // 30
        MD5_GG(c, d, a, b, x[7], 14, 0x676f02d9);   // 31
        MD5_GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);  // 32

        MD5_HH(a, b, c, d, x[5], 4, 0xfffa3942);    // 33
        MD5_HH(d, a, b, c, x[8], 11, 0x8771f681);   // 34
        MD5_HH(c, d, a, b, x[11], 16, 0x6d9d6122);  // 35
        MD5_HH(b, c, d, a, x[14], 23, 0xfde5380c);  // 36
        MD5_HH(a, b, c, d, x[1], 4, 0xa4beea44);    // 37
        MD5_HH(d, a, b, c, x[4], 11, 0x4bdecfa9);   // 38
        MD5_HH(c, d, a, b, x[7], 16, 0xf6bb4b60);   // 39
        MD5_HH(b, c, d, a, x[10], 23, 0xbebfbc70);  // 40
        MD5_HH(a, b, c, d, x[13], 4, 0x289b7ec6);   // 41
        MD5_HH(d, a, b, c, x[0], 11, 0xeaa127fa);   // 42
        MD5_HH(c, d, a, b, x[3], 16, 0xd4ef3085);   // 43
        MD5_HH(b, c, d, a, x[6], 23, 0x4881d05);    // 44
        MD5_HH(a, b, c, d, x[9], 4, 0xd9d4d039);    // 45
        MD5_HH(d, a, b, c, x[12], 11, 0xe6db99e5);  // 46
        MD5_HH(c, d, a, b, x[15], 16, 0x1fa27cf8);  // 47
        MD5_HH(b, c, d, a, x[2], 23, 0xc4ac5665);   // 48

        MD5_II(a, b, c, d, x[0], 6, 0xf4292244);    // 49
        MD5_II(d, a, b, c, x[7], 10, 0x432aff97);   // 50
        MD5_II(c, d, a, b, x[14], 15, 0xab9423a7);  // 51
        MD5_II(b, c, d, a, x[5], 21, 0xfc93a039);   // 52
        MD5_II(a, b, c, d, x[12], 6, 0x655b59c3);   // 53
        MD5_II(d, a, b, c, x[3], 10, 0x8f0ccc92);   // 54
        MD5_II(c, d, a, b, x[10], 15, 0xffeff47d);  // 55
        MD5_II(b, c, d, a, x[1], 21, 0x85845dd1);   // 56
        MD5_II(a, b, c, d, x[8], 6, 0x6fa87e4f);    // 57
        MD5_II(d, a, b, c, x[15], 10, 0xfe2ce6e0);  // 58
        MD5_II(c, d, a, b, x[6], 15, 0xa3014314);   // 59
        MD5_II(b, c, d, a, x[13], 21, 0x4e0811a1);  // 60
        MD5_II(a, b, c, d, x[4], 6, 0xf7537e82);    // 61
        MD5_II(d, a, b, c, x[11], 10, 0xbd3af235);  // 62
        MD5_II(c, d, a, b, x[2], 15, 0x2ad7d2bb);   // 63
        MD5_II(b, c, d, a, x[9], 21, 0xeb86d391);   // 64

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
    }

    static void MD5Update(MD5_CTX* context, unsigned char* input, unsigned int inputlen)
    {
        unsigned int i = 0, index = 0, partlen = 0;
        index = (context->count[0] >> 3) & 0x3F;
        partlen = 64 - index;
        context->count[0] += inputlen << 3;
        if (context->count[0] < (inputlen << 3)) {
            context->count[1]++;
        }
        context->count[1] += inputlen >> 29;
        if (inputlen >= partlen) {
            memcpy(&context->buffer[index], input, partlen);
            MD5Transform(context->state, context->buffer);
            for (i = partlen; i + 64 <= inputlen; i += 64) {
                MD5Transform(context->state, &input[i]);
            }
            index = 0;
        }
        else {
            i = 0;
        }
        memcpy(&context->buffer[index], &input[i], inputlen - i);
    }

    static void MD5Final(MD5_CTX* context, unsigned char digest[16])
    {
        unsigned int index = 0, padlen = 0;
        unsigned char bits[8];
        index = (context->count[0] >> 3) & 0x3F;
        padlen = (index < 56) ? (56 - index) : (120 - index);
        MD5Encode(bits, context->count, 8);
        MD5Update(context, PADDING, padlen);
        MD5Update(context, bits, 8);
        MD5Encode(digest, context->state, 16);
    }

    //string md5
    static std::string getStringMD5(std::string encrypt)
    {
        //encrypt = "admin";//21232f297a57a5a743894a0e4a801fc3
        unsigned char decrypt[16] = { 0 };

        MD5_CTX md5;
        MD5Init(&md5);
        MD5Update(&md5, (unsigned char*)encrypt.c_str(), (unsigned int)encrypt.length());
        MD5Final(&md5, (unsigned char*)decrypt);

        std::string result = "";
        for (int i = 0; i < 16; i++)
        {
            std::string ch = "";
            char buff[3] = { 0 };//xx\0

            snprintf(buff, 3, "%02x", decrypt[i]);
            ch = buff; result += ch;
        }

        return result;
    }
  
    //file md5
    static std::string getFileMD5(std::string sfilepath)
    {
        unsigned char encrypt[1024] = { 0 };
        unsigned char decrypt[16] = { 0 };

        FILE* fp = nullptr;
        fp = fopen(sfilepath.c_str(), "rb");
        if (fp != nullptr)
        {
            MD5_CTX md5;
            MD5Init(&md5);
            while (1)
            {
                //memset(encrypt, 0, sizeof(encrypt));
                int ret = fread(encrypt, 1, sizeof(encrypt), fp);
                if (ret > 0)
                {
                    MD5Update(&md5, encrypt, ret);
                }
                if (ret < sizeof(encrypt))
                {
                    break;
                }
            }
            MD5Final(&md5, decrypt);
            fclose(fp);
        }


        std::string result = "";
        for (int i = 0; i < 16; i++)
        {
            std::string ch = "";
            char buff[3] = { 0 };//xx\0

            snprintf(buff, 3, "%02x", decrypt[i]);
            ch = buff; result += ch;
        }

        return result;
    }

}
#endif

//base64
#if 1
namespace base64
{
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    static inline bool is_base64(unsigned char c)
    {
        return (isalnum(c) || (c == '+') || (c == '/'));
    }

    static long fsize(FILE* fp)
    {
        fpos_t fpos;
        fgetpos(fp, &fpos);

        long len = 0;
        fseek(fp, 0, SEEK_END);
        len = ftell(fp);

        fsetpos(fp, &fpos);

        return len;
    }

    static std::string base64_encode(char const* bytes_to_encode, int in_len)
    {
        std::string ret;
        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];

        while (in_len--) {
            char_array_3[i++] = *(bytes_to_encode++);
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for (i = 0; (i < 4); i++)
                    ret += base64_chars[char_array_4[i]];
                i = 0;
            }
        }

        if (i)
        {
            for (j = i; j < 3; j++)
                char_array_3[j] = '\0';

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (j = 0; (j < i + 1); j++)
                ret += base64_chars[char_array_4[j]];

            while ((i++ < 3))
                ret += '=';
        }

        return ret;
    }

    static std::string base64_decode(std::string& encoded_string)
    {
        int in_len = encoded_string.size();
        int i = 0;
        int j = 0;
        int in_ = 0;
        unsigned char char_array_4[4], char_array_3[3];
        std::string ret;

        while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
            char_array_4[i++] = encoded_string[in_]; in_++;
            if (i == 4) {
                for (i = 0; i < 4; i++)
                    char_array_4[i] = base64_chars.find(char_array_4[i]);

                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; (i < 3); i++)
                    ret += char_array_3[i];
                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 4; j++)
                char_array_4[j] = 0;

            for (j = 0; j < 4; j++)
                char_array_4[j] = base64_chars.find(char_array_4[j]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
        }

        return ret;
    }

    //custom
    static std::string base64_encode_file(std::string& sfilepath)
    {
        std::string encoded = "";
        if (!is_existfile(sfilepath.c_str())) return encoded;

        long length = 0;
        char* imgBuffer = nullptr;
        FILE* fp = nullptr;
        fp = fopen(sfilepath.c_str(), "rb");
        if (fp != nullptr)
        {
            fseek(fp, 0, SEEK_END);
            length = ftell(fp);
            rewind(fp);

            imgBuffer = (char*)malloc(length * sizeof(char));
            if (imgBuffer != nullptr)
                fread(imgBuffer, length, 1, fp);
            fclose(fp);
        }

        if (length && imgBuffer)
        {
            encoded = base64_encode((const char*)(imgBuffer), length);
        }

        return encoded;
    }
}
#endif

//jwt
#if 1

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

    static void remove_n(std::string& input)
    {
        //去掉加密字串中的 \n
        input.erase(std::remove(input.begin(), input.end(), '\n'), input.end());
    }
    static std::vector<std::string> split(const std::string& s, char delimiter)
    {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }
    static std::string hmac_sha256(const std::string& key, const std::string& msg)
    {
        unsigned char hmac[EVP_MAX_MD_SIZE];
        unsigned int hmac_len = 0;
        HMAC(EVP_sha256(), key.c_str(), key.length(), reinterpret_cast<const unsigned char*>(msg.c_str()), msg.length(), hmac, &hmac_len);
        std::stringstream ss;
        for (unsigned int i = 0; i < hmac_len; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hmac[i]);
        }

        return ss.str();
    }

    static std::string jwt_base64_encode(const std::string& input) 
    {
        BIO* bio, * b64;
        BUF_MEM* bufferPtr;

        b64 = BIO_new(BIO_f_base64());
        bio = BIO_new(BIO_s_mem());
        bio = BIO_push(b64, bio);

        BIO_write(bio, input.c_str(), input.length());
        BIO_flush(bio);

        BIO_get_mem_ptr(bio, &bufferPtr);
        std::string output(bufferPtr->data, bufferPtr->length);
        BIO_free_all(bio);

        remove_n(output);
        return output;
    }
    static std::string jwt_base64_decode(const std::string& input)
    {
        BIO* bio, * b64;
        char* buffer = new char[input.size()+1];
        memset(buffer, 0, input.size());

        bio = BIO_new_mem_buf(input.c_str(), -1);
        b64 = BIO_new(BIO_f_base64());
        bio = BIO_push(b64, bio);
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
        int len = BIO_read(bio, buffer, input.size());
        BIO_free_all(bio);

        std::string str_ret; str_ret = buffer;
        return str_ret;
    }

    static std::string jwt_encode(std::string& header, std::string& payload, std::string& key)
    {
        std::string header_b64 = jwt_base64_encode(header);     
        std::string payload_b64 = jwt_base64_encode(payload);   

        std::string signature_hac_before = header_b64 + "." + payload_b64;
        std::string signature_hac_after = hmac_sha256(key, signature_hac_before);
        std::string signature_b64 = jwt_base64_encode(signature_hac_after);  

        return header_b64 + "." + payload_b64 + "." + signature_b64;
    }
    static bool jwt_decode(std::string& token, std::string& key, std::string& header, std::string& payload)
    {
        std::vector<std::string> parts = split(token, '.');
        if (parts.size() != 3)
            return false;

        header = jwt_base64_decode(parts[0]);

        payload = jwt_base64_decode(parts[1]);

        std::string signature_hac_before = parts[0] + "." + parts[1];
        std::string signature_hac_after = hmac_sha256(key, signature_hac_before);
        std::string signature = jwt_base64_decode(parts[2]);
        if (signature != signature_hac_after) 
            return false;
        
        return true;
    }

    static bool string_to_token(std::string header, std::string payload, std::string key, std::string& token)
    {
        bool result = true;
        token = jwt_encode(header, payload, key);
        if (token.empty())
            result = false;

        return result;
    }
    static bool token_to_string(std::string& header, std::string& payload, std::string& key, std::string token)
    {
        return jwt_decode(token, key, header, payload);
    }
#endif


