// HttpServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <set>

#include "event.h"
#include "event2/event.h"
#include "event2/buffer.h"
#include "event2/bufferevent.h"
#include "event2/bufferevent_ssl.h"
#include "event2/buffer_compat.h"
#include "event2/bufferevent_compat.h"
#include "evhttp.h"
#include "event2/http.h"
#include "event2/http_struct.h"
#include "event2/keyvalq_struct.h"
#include "event2/util.h"
#include "event2/listener.h"
#include "event2/thread.h"
#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>

//
#include "public.h"
#include "json.h"
#include "httpkit.h"
#include "awsUpload.h"
#include "mmRabbitmq.h"
#include "ssoDataFrom.h"
#include "videoOperate.h"
#include "digitalmysql.h"
#include "digitalEntityJson.h"
#include "httpConcurrencyServer.h"

#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:2362)		//忽略goto报错


#define CHECK_REQUEST_STR(name,str,msg,result)  {if(str.empty()){msg=std::string(name)+" is error,please check request body";result&=false;}}
#define CHECK_REQUEST_NUM(name,num,msg,result)  {if(num<0){msg=std::string(name)+" is error,please check request body";result&=false;}}
#define ADD_FILTER_INFO(vecitem,iteminfo,field,value) {if(!value.empty()){iteminfo.filterfield = field; iteminfo.filtervalue = value; vecitem.push_back(iteminfo);}}
#define BUFF_SZ 1024*16  //system max stack size
#define PREFIX_OSS		("OSS:")			//自定义云盘路径前缀

#if defined WIN32
#define COMMON_STRCPY(x, y, z) strcpy_s(x, z, y)
#else
#define COMMON_STRCPY(x, y, z) strncpy(x, y, z);
#endif

//sleep function
#if defined WIN32
    #include <windows.h>

    void sleep(unsigned milliseconds)
    {
        Sleep(milliseconds);
    }
#else
    #include <unistd.h>

    void sleep(unsigned milliseconds)
    {
        usleep(milliseconds * 1000); // takes microseconds
    }
#endif

//dump file
#if defined WIN32
#include <dbghelp.h>
#pragma comment( lib, "dbghelp")

	LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* exceptionPointers)
	{
		std::string dumpFilePath = getexepath() + std::string("\\") + gettime_custom() + std::string("_HttpServer.dmp");

		// 创建Dump文件
		HANDLE dumpFile = CreateFileA(dumpFilePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (dumpFile != INVALID_HANDLE_VALUE)
		{
			// 写入Dump文件头
			MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
			exceptionInfo.ThreadId = GetCurrentThreadId();
			exceptionInfo.ExceptionPointers = exceptionPointers;
			exceptionInfo.ClientPointers = FALSE;

			// 生成Dump文件
			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile, MiniDumpNormal, &exceptionInfo, NULL, NULL);

			// 关闭Dump文件
			CloseHandle(dumpFile);
		}

		// 返回处理异常的结果
		return EXCEPTION_EXECUTE_HANDLER;
	};
#else
	LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* exceptionPointers)
	{
		return EXCEPTION_EXECUTE_HANDLER;
	}
#endif


//human unique id from
std::string gethuman_uniqueid(int belongid, std::string humanname)
{
	std::string str_uniqueid = "";
	if (belongid < 0 || humanname.empty())
		return str_uniqueid;

	char temp[256] = { 0 };
	snprintf(temp, 256, "%d:%s", belongid, humanname.c_str()); str_uniqueid = temp;
	str_uniqueid = md5::getStringMD5(str_uniqueid);

	return str_uniqueid;
}


#if 1 //环境参数

//Actorinfo 结构体
typedef struct _actorinfo
{
	int			actortype;//0=TTS+W2L,1=TTS
	std::string ip;
	short		port;

	_actorinfo()
	{
		actortype = 0;
		ip = "";
		port = 0;
	}

	void copydata(const _actorinfo& item)
	{
		actortype = item.actortype;
		ip = item.ip;
		port = item.port;
	}
} actorinfo, *pactorinfo;
typedef std::map<std::string, actorinfo> ACTORINFO_MAP;
//TTS+W2L
ACTORINFO_MAP Container_TTSW2LActor;
bool getconfig_actornode(std::string configfilepath, std::string& error)
{
	long length = 0;
	char* configbuffer = nullptr;
	FILE* fp = nullptr;
	fp = fopen(configfilepath.c_str(), "r");
	if (fp != nullptr)
	{
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		rewind(fp);

		configbuffer = (char*)malloc(length * sizeof(char));
		if (configbuffer == nullptr) return false;

		fread(configbuffer, length, 1, fp);
		fclose(fp);
	}

	if (length && configbuffer)
	{
		std::string value = "";
		std::string config = configbuffer;
		free(configbuffer);

		value = getnodevalue(config, "actor_count"); CHECK_CONFIG("actor_count", value, error);
		int count = atoi(value.c_str());
		_debug_to(1, ("CONFIG actornode count = %d\n"), count);

		Container_TTSW2LActor.clear();
		for (int i = 0; i < count; i++)
		{
			std::string ip = ""; short port = 0;

			std::string actor_pro = ""; char temp[256] = { 0 };
			snprintf(temp, 256, "actor%d_", i); actor_pro = temp;

			std::string actor_ip = actor_pro + "ip";
			value = getnodevalue(config, actor_ip);
			if (value.empty()) continue;
			ip = value;
			_debug_to(1, ("CONFIG actornode actor%d_ip = %s\n"), i, ip.c_str());

			std::string actor_port = actor_pro + "port";
			value = getnodevalue(config, actor_port);
			if (value.empty()) continue;
			port = atoi(value.c_str());
			_debug_to(1, ("CONFIG actornode actor%d_port = %d\n"), i, port);

			if (!ip.empty() && port > 0)
			{
				actorinfo actoritem;
				actoritem.actortype = 0;
				actoritem.ip = ip;
				actoritem.port = port;
				Container_TTSW2LActor.insert(std::make_pair(ip, actoritem));
			}	
		}

		return true;
	}

	return false;
}
//TTS
ACTORINFO_MAP Container_TTSActor;
bool getconfig_actornode_tts(std::string configfilepath, std::string& error)
{
	long length = 0;
	char* configbuffer = nullptr;
	FILE* fp = nullptr;
	fp = fopen(configfilepath.c_str(), "r");
	if (fp != nullptr)
	{
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		rewind(fp);

		configbuffer = (char*)malloc(length * sizeof(char));
		if (configbuffer == nullptr) return false;

		fread(configbuffer, length, 1, fp);
		fclose(fp);
	}

	if (length && configbuffer)
	{
		std::string value = "";
		std::string config = configbuffer;
		free(configbuffer);

		value = getnodevalue(config, "ttsactor_count");
		int count = atoi(value.c_str());
		_debug_to(1, ("CONFIG ttsactor_count count = %d\n"), count);

		Container_TTSActor.clear();
		for (int i = 0; i < count; i++)
		{
			std::string ip = ""; short port = 0;

			std::string actor_pro = ""; char temp[256] = { 0 };
			snprintf(temp, 256, "ttsactor%d_", i); actor_pro = temp;

			std::string actor_ip = actor_pro + "ip";
			value = getnodevalue(config, actor_ip);
			if (value.empty()) continue;
			ip = value;
			_debug_to(1, ("CONFIG actornode ttsactor%d_ip = %s\n"), i, ip.c_str());

			std::string actor_port = actor_pro + "port";
			value = getnodevalue(config, actor_port);
			if (value.empty()) continue;
			port = atoi(value.c_str());
			_debug_to(1, ("CONFIG actornode ttsactor%d_port = %d\n"), i, port);

			if (!ip.empty() && port > 0)
			{
				actorinfo actoritem;
				actoritem.actortype = 1;
				actoritem.ip = ip;
				actoritem.port = port;
				Container_TTSActor.insert(std::make_pair(ip, actoritem));
			}
		}

		//兼容TTSActor未配置的情况:使用Actor进行合成音频任务
		if (Container_TTSActor.empty())
		{
			Container_TTSActor = Container_TTSW2LActor;
			
			size_t copy_count = Container_TTSActor.size();
			_debug_to(1, ("CONFIG ttsactor copy_count = %d\n"), copy_count);
		}

		return true;
	}

	return false;
}

//AWS上传云盘
bool aws_enable = false;
std::string aws_rootfolder = "default";
std::string aws_webprefix = "https://";
std::string aws_endpoint = "";
std::string aws_url = "";
std::string aws_ak = "";
std::string aws_sk = "";
std::string aws_bucket = "";
bool getconfig_aws(std::string configfilepath, std::string& error)
{
	long length = 0;
	char* configbuffer = nullptr;
	FILE* fp = nullptr;
	fp = fopen(configfilepath.c_str(), "r");
	if (fp != nullptr)
	{
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		rewind(fp);

		configbuffer = (char*)malloc(length * sizeof(char));
		if (configbuffer == nullptr) return false;

		fread(configbuffer, length, 1, fp);
		fclose(fp);
	}

	if (length && configbuffer)
	{
		std::string value = "";
		std::string config = configbuffer;
		free(configbuffer);

		value = getnodevalue(config, "aws_enable");
		aws_enable = (atoi(value.c_str()) > 0);
		_debug_to(1, ("AWS Enable = %s\n"), (aws_enable ? ("TRUE") : ("FALSE")));

		if (aws_enable)
		{
			value = getnodevalue(config, "aws_rootfolder"); CHECK_CONFIG("aws_rootfolder", value, error);
			aws_rootfolder = value.c_str();
			_debug_to(1, ("CONFIG aws_rootfolder = %s\n"), aws_rootfolder.c_str());

			value = getnodevalue(config, "aws_webprefix"); CHECK_CONFIG("aws_webprefix", value, error);
			aws_webprefix = value.c_str();
			_debug_to(1, ("CONFIG aws_webprefix = %s\n"), aws_webprefix.c_str());

			value = getnodevalue(config, "aws_endpoint"); CHECK_CONFIG("aws_endpoint", value, error);
			aws_endpoint = value.c_str();
			_debug_to(1, ("CONFIG aws_endpoint = %s\n"), aws_endpoint.c_str());

			value = getnodevalue(config, "aws_url"); CHECK_CONFIG("aws_url", value, error);
			aws_url = value.c_str();
			_debug_to(1, ("CONFIG aws_url = %s\n"), aws_url.c_str());

			value = getnodevalue(config, "aws_ak"); CHECK_CONFIG("aws_ak", value, error);
			aws_ak = value.c_str();
			_debug_to(1, ("CONFIG aws_ak = %s\n"), aws_ak.c_str());

			value = getnodevalue(config, "aws_sk"); CHECK_CONFIG("aws_sk", value, error);
			aws_sk = value.c_str();
			_debug_to(1, ("CONFIG aws_sk = %s\n"), aws_sk.c_str());

			value = getnodevalue(config, "aws_bucket"); CHECK_CONFIG("aws_bucket", value, error);
			aws_bucket = value.c_str();
			_debug_to(1, ("CONFIG aws_bucket = %s\n"), aws_bucket.c_str());
		}

		return true;
	}

	return false;
}

//SSO登录验证
bool sso_enable = false;
std::string url_getcode  = "https://auth.sobeylingyun.com/oauth/authorize";
std::string url_gettoken = "https://auth.sobeylingyun.com/oauth/token";
std::string url_getuser  = "https://auth.sobeylingyun.com/v1/kernel/configs/user/current";
std::string url_homepage = "https://virbot.sobeylingyun.com";
std::string url_redirect = "https://virbot.sobeylingyun.com/login/callback";
std::string client_id = "VIRBOT";
std::string client_secret = "dPD1RDT1j1bk4DiBCIKZaINAzbrDyER5";
std::string client_authorization = "";
bool getconfig_sso(std::string configfilepath, std::string& error)
{
	long length = 0;
	char* configbuffer = nullptr;
	FILE* fp = nullptr;
	fp = fopen(configfilepath.c_str(), "r");
	if (fp != nullptr)
	{
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		rewind(fp);

		configbuffer = (char*)malloc(length * sizeof(char));
		if (configbuffer == nullptr) return false;

		fread(configbuffer, length, 1, fp);
		fclose(fp);
	}

	if (length && configbuffer)
	{
		std::string value = "";
		std::string config = configbuffer;
		free(configbuffer);

		value = getnodevalue(config, "sso_enable");
		sso_enable = (atoi(value.c_str()) > 0);
		_debug_to(1, ("SSO Enable = %s\n"), (sso_enable ? ("TRUE") : ("FALSE")));

		if (sso_enable)
		{
			value = getnodevalue(config, "url_getcode"); CHECK_CONFIG("url_getcode", value, error);
			url_getcode = value.c_str();
			_debug_to(1, ("CONFIG url_getcode = %s\n"), url_getcode.c_str());

			value = getnodevalue(config, "url_gettoken"); CHECK_CONFIG("url_gettoken", value, error);
			url_gettoken = value.c_str();
			_debug_to(1, ("CONFIG url_gettoken = %s\n"), url_gettoken.c_str());

			value = getnodevalue(config, "url_getuser"); CHECK_CONFIG("url_getuser", value, error);
			url_getuser = value.c_str();
			_debug_to(1, ("CONFIG url_getuser = %s\n"), url_getuser.c_str());

			value = getnodevalue(config, "url_redirect"); CHECK_CONFIG("url_redirect", value, error);
			url_redirect = value.c_str();
			_debug_to(1, ("CONFIG url_redirect = %s\n"), url_redirect.c_str());

			value = getnodevalue(config, "client_id"); CHECK_CONFIG("client_id", value, error);
			client_id = value.c_str();
			_debug_to(1, ("CONFIG client_id = %s\n"), client_id.c_str());

			value = getnodevalue(config, "client_secret"); CHECK_CONFIG("client_secret", value, error);
			client_secret = value.c_str();
			_debug_to(1, ("CONFIG client_secret = %s\n"), client_secret.c_str());

			//calc
			std::string str = client_id + std::string(":") + client_secret;
			std::string str_base64 = base64::base64_encode(str.c_str(), str.length());
			client_authorization = std::string("Basic ") + str_base64;
		}

		return true;
	}

	return false;
}

//前后端是否使用证书
bool cert_enable = false;
std::string  key_certificate = "";
std::string  key_private = "";
bool getconfig_cert(std::string configfilepath, std::string& error)
{
	long length = 0;
	char* configbuffer = nullptr;
	FILE* fp = nullptr;
	fp = fopen(configfilepath.c_str(), "r");
	if (fp != nullptr)
	{
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		rewind(fp);

		configbuffer = (char*)malloc(length * sizeof(char));
		if (configbuffer == nullptr) return false;

		fread(configbuffer, length, 1, fp);
		fclose(fp);
	}

	if (length && configbuffer)
	{
		std::string value = "";
		std::string config = configbuffer;
		free(configbuffer);

		value = getnodevalue(config, "cert_enable");
		cert_enable = (atoi(value.c_str()) > 0);
		_debug_to(1, ("Cert Enable = %s\n"), (cert_enable ? ("TRUE") : ("FALSE")));

		if (cert_enable)
		{
			value = getnodevalue(config, "key_certificate");
			key_certificate = value.c_str();
			_debug_to(1, ("CONFIG key_certificate = %s\n"), key_certificate.c_str());

			value = getnodevalue(config, "key_private");
			key_private = value.c_str();
			_debug_to(1, ("CONFIG key_private = %s\n"), key_private.c_str());
		}

		return true;
	}

	return false;
}

//全局配置
std::string  delay_beforetext = "[p500]";
std::string  delay_aftertext = "[p300]";
std::string  folder_digitalmodel = "";	//本地路径：数字人资源包相关文件【模型文件+原始视频文件等】
std::string  folder_digitalhtml = "";	//WEB服务器路径：数字人相关【<task>+<keyframe>+<source>】
bool getconfig_global(std::string configfilepath, std::string& error)
{
	long length = 0;
	char* configbuffer = nullptr;
	FILE* fp = nullptr;
	fp = fopen(configfilepath.c_str(), "r");
	if (fp != nullptr)
	{
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		rewind(fp);

		configbuffer = (char*)malloc(length * sizeof(char));
		if (configbuffer == nullptr) return false;

		fread(configbuffer, length, 1, fp);
		fclose(fp);
	}

	if (length && configbuffer)
	{
		std::string value = "";
		std::string config = configbuffer;
		free(configbuffer);

		char temp[256] = { 0 };
		value = getnodevalue(config, "delay_beforetext");
		if (value.empty()) value = "500";
		snprintf(temp, 256, "[p%s]", value.c_str());
		delay_beforetext = temp;
		_debug_to(1, ("CONFIG delay_beforetext = %s\n"), delay_beforetext.c_str());

		value = getnodevalue(config, "delay_aftertext");
		if (value.empty()) value = "300";
		snprintf(temp, 256, "[p%s]", value.c_str());
		delay_aftertext = temp;
		_debug_to(1, ("CONFIG delay_aftertext = %s\n"), delay_aftertext.c_str());

		//
		value = getnodevalue(config, "folder_digitalmodel"); CHECK_CONFIG("folder_digitalmodel", value, error);
		folder_digitalmodel = value.c_str();
		_debug_to(1, ("CONFIG folder_digitalmodel = %s\n"), folder_digitalmodel.c_str());

		value = getnodevalue(config, "folder_digitalhtml"); CHECK_CONFIG("folder_digitalhtml", value, error);
		folder_digitalhtml = value.c_str();
		_debug_to(1, ("CONFIG folder_digitalhtml = %s\n"), folder_digitalhtml.c_str());

		return true;
	}

	return false;
}

#endif

#if 1//AWS上传+下载

//其他文件(临时/公共)
bool uploadfile_public(std::string sourcepath_local, std::string& sourcepath_http)
{
	if (sourcepath_local.empty())
		return false;
	sourcepath_local = str_replace(sourcepath_local, std::string("//"), std::string("\\\\")); //兼容共享路径
	ansi_to_utf8(sourcepath_local.c_str(), sourcepath_local.length(), sourcepath_local);//awsUpload需UTF8编码参数

	awsUpload uploadObj;
	uploadObj.SetAWSConfig(aws_webprefix, aws_endpoint, aws_url, aws_ak, aws_sk, aws_bucket);
	std::string object_folder = aws_rootfolder + std::string("/Public");

	//上传 
	bool result = true;
	if (!uploadObj.UploadFile(object_folder, sourcepath_local, sourcepath_http, true))
	{
		result = false;
		_debug_to(0, ("upload public file failed: %s\n"), sourcepath_local.c_str());
	}
	return result;
}
//背景资源上传
bool uploadfile_backsource(std::string sourcepath_local, std::string& sourcepath_http)
{
	if (sourcepath_local.empty())
		return false;
	sourcepath_local = str_replace(sourcepath_local, std::string("//"), std::string("\\\\")); //兼容共享路径
	ansi_to_utf8(sourcepath_local.c_str(), sourcepath_local.length(), sourcepath_local);//awsUpload需UTF8编码参数

	awsUpload uploadObj;
	uploadObj.SetAWSConfig(aws_webprefix, aws_endpoint, aws_url, aws_ak, aws_sk, aws_bucket);
	std::string object_folder = aws_rootfolder + std::string("/BackSource");

	//上传 
	bool result = true;
	if (!uploadObj.UploadFile(object_folder, sourcepath_local, sourcepath_http, true))
	{
		result = false;
		_debug_to(0, ("upload backsource file failed: %s\n"), sourcepath_local.c_str());
	}
	return result;
}
//原始视频上传
bool uploadfile_originalvdo(std::string humanid, std::string sourcepath_local, std::string& sourcepath_http)
{
	if (sourcepath_local.empty() || humanid.empty())
		return false;
	sourcepath_local = str_replace(sourcepath_local, std::string("//"), std::string("\\\\")); //兼容共享路径
	ansi_to_utf8(sourcepath_local.c_str(), sourcepath_local.length(), sourcepath_local);//awsUpload需UTF8编码参数

	awsUpload uploadObj;
	uploadObj.SetAWSConfig(aws_webprefix, aws_endpoint, aws_url, aws_ak, aws_sk, aws_bucket);
	std::string object_folder = aws_rootfolder + std::string("/ModelFile/") + humanid + std::string("/originalvdo");

	//上传 
	bool result = true;
	if (!uploadObj.UploadFile(object_folder, sourcepath_local, sourcepath_http, true))
	{
		result = false;
		_debug_to(0, ("upload originalvideo file failed: %s\n"), sourcepath_local.c_str());
	}
	return result;
}
//[OSS->https / 本地地址->本地地址]
std::string webpath_from_osspath(std::string objectfile_path)
{
	std::string result = objectfile_path;
	if (str_prefixsame(objectfile_path, std::string(PREFIX_OSS)))//认为是OSS路径
	{
		//不使用GetHttpFilePath避免S3访问冲突
		objectfile_path = str_replace(objectfile_path, std::string(PREFIX_OSS), std::string(""));
		std::string webfile_path = aws_webprefix + aws_url + std::string("/") + objectfile_path;
		result = url_encode(webfile_path); //加密
	}

	return result;
}
//[https->OSS / 本地地址->本地地址]
std::string osspath_from_webpath(std::string webfile_path)
{
	std::string result = webfile_path;
	if (str_prefixsame(webfile_path, aws_webprefix))//认为是web路径
	{
		//不使用GetObjectFilePath避免S3访问冲突
		webfile_path = url_decode(webfile_path);//解密
		std::string url_prefix = aws_webprefix + aws_url + std::string("/");
		result = str_replace(webfile_path, url_prefix, std::string(PREFIX_OSS));
	}

	return result;
}

//执行AWS上传任务线程
typedef struct _fileupload
{
	std::string filepath_local;
	std::string filefolder_object;
	bool		removelocal;

	_fileupload()
	{
		filepath_local = "";
		filefolder_object = "";
		removelocal = true;
	}

	void copydata(const _fileupload& item)
	{
		filepath_local = item.filepath_local;
		filefolder_object = item.filefolder_object;
		removelocal = item.removelocal;
	}

} fileupload, *pfileupload;
void* pthread_awsupload_thread(void* arg)
{
	_debug_to(0, ("pthread_awsupload_thread start...\n"));
	pfileupload pfileitem = (pfileupload)arg;
	std::string filepath_local = pfileitem->filepath_local;
	std::string filefolder_object = pfileitem->filefolder_object;
	bool        removelocal = pfileitem->removelocal;
	if (filepath_local.empty() || filefolder_object.empty())
	{
		_debug_to(0, ("[%s]->[%s], parmeter error...\n"), filepath_local.c_str(), filefolder_object.c_str());
		delete pfileitem;
		return nullptr;
	}

	filepath_local = str_replace(filepath_local, std::string("//"), std::string("\\\\")); //兼容共享路径
	ansi_to_utf8(filepath_local.c_str(), filepath_local.length(), filepath_local);//awsUpload需UTF8编码参数

	awsUpload uploadObj;
	uploadObj.SetAWSConfig(aws_webprefix, aws_endpoint, aws_url, aws_ak, aws_sk, aws_bucket);

	//上传 
	bool result = true;
	std::string filepath_http = "";
	if (uploadObj.UploadFile(filefolder_object, filepath_local, filepath_http, true))
	{
		if (removelocal) remove(filepath_local.c_str());
		_debug_to(0, ("[%s]->[%s], upload success\n"), filepath_local.c_str(), filefolder_object.c_str());
	}
	else
	{
		result = false;
		_debug_to(0, ("[%s]->[%s], upload failed...\n"), filepath_local.c_str(), filefolder_object.c_str());
	}

	_debug_to(0, ("pthread_awsupload_thread exit...\n"));
	delete pfileitem;
	return nullptr;
}

#endif

#if 1 //rabbitmq消息

//连接到rabbitmq服务
static std::string rabbitmq_ip = "";
static short	   rabbitmq_port = 5672;
static std::string rabbitmq_user = "";
static std::string rabbitmq_passwd = "";
//发送者指定+接受者使用
static std::string rabbitmq_exchange = "";  //消息属性1
static std::string rabbitmq_routekey = "";  //消息熟悉2
//接收者指定+接受者使用
static std::string rabbitmq_queuename = ""; //消息队列名
bool getconfig_rabbitmq(std::string configfilepath, std::string& error)
{
	long length = 0;
	char* configbuffer = nullptr;
	FILE* fp = nullptr;
	fp = fopen(configfilepath.c_str(), "r");
	if (fp != nullptr)
	{
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		rewind(fp);

		configbuffer = (char*)malloc(length * sizeof(char));
		if (configbuffer == nullptr) return false;

		fread(configbuffer, length, 1, fp);
		fclose(fp);
	}

	if (length && configbuffer)
	{
		std::string value = "";
		std::string config = configbuffer;
		free(configbuffer);

		value = getnodevalue(config, "rabbitmq_ip"); CHECK_CONFIG("rabbitmq_ip", value, error);
		rabbitmq_ip = value;
		_debug_to(1, ("CONFIG rabbitmq_ip = %s\n"), rabbitmq_ip.c_str());

		value = getnodevalue(config, "rabbitmq_port"); CHECK_CONFIG("rabbitmq_port", value, error);
		rabbitmq_port = atoi(value.c_str());
		_debug_to(1, ("CONFIG rabbitmq_port = %d\n"), rabbitmq_port);

		value = getnodevalue(config, "rabbitmq_user"); CHECK_CONFIG("rabbitmq_user", value, error);
		rabbitmq_user = value;
		_debug_to(1, ("CONFIG rabbitmq_user = %s\n"), rabbitmq_user.c_str());

		value = getnodevalue(config, "rabbitmq_passwd"); CHECK_CONFIG("rabbitmq_passwd", value, error);
		rabbitmq_passwd = value;
		_debug_to(1, ("CONFIG rabbitmq_passwd = %s\n"), rabbitmq_passwd.c_str());

		//
		value = getnodevalue(config, "rabbitmq_exchange"); CHECK_CONFIG("rabbitmq_exchange", value, error);
		rabbitmq_exchange = value;
		_debug_to(1, ("CONFIG rabbitmq_exchange = %s\n"), rabbitmq_exchange.c_str());

		value = getnodevalue(config, "rabbitmq_routekey"); CHECK_CONFIG("rabbitmq_routekey", value, error);
		rabbitmq_routekey = value;
		_debug_to(1, ("CONFIG rabbitmq_routekey = %s\n"), rabbitmq_routekey.c_str());

		return true;
	}

	return false;
}

std::string getNotifyMsg_ToHtml(int taskid)
{
	std::string result_str = "";
	digitalmysql::taskinfo newstateitem;
	digitalmysql::gettaskinfo(taskid, newstateitem);

	//message data
	int task_id = newstateitem.taskid;
	int task_type = newstateitem.tasktype;
	int task_state = newstateitem.taskstate;
	int task_progress = newstateitem.taskprogress;
	std::string task_videopath = webpath_from_osspath(newstateitem.video_path);
	std::string task_videokeyframe = webpath_from_osspath(newstateitem.video_keyframe);
	std::string task_videoduration = gettimetext_framecount(newstateitem.video_length, newstateitem.video_fps);
	std::string task_audioduration = gettimetext_millisecond(newstateitem.audio_length);
	if (task_videopath.empty() || task_videokeyframe.empty())
	{
		std::string taskhumanid = newstateitem.humanid;
		digitalmysql::humaninfo taskhumanitem;
		if (digitalmysql::gethumaninfo(taskhumanid, taskhumanitem))
			task_videokeyframe = webpath_from_osspath(taskhumanitem.keyframe);
	}

	int notify_taskid = task_id;
	int notify_state = task_state;
	int notify_progress = task_progress;
	std::string notify_videopath = task_videopath;
	std::string notify_videokeyframe = task_videokeyframe;
	std::string notify_duration = (task_type==0)?(task_audioduration):(task_videoduration);

	//message json
	char tempbuff[1024] = { 0 };
	snprintf(tempbuff, 1024, "{\"TaskID\":%d, \"State\":%d, \"Progerss\":%d, \"VedioFile\":\"%s\", \"FilePath\":\"%s\",\"Duration\":\"%s\" }", 
		notify_taskid, notify_state, notify_progress, notify_videopath.c_str(), notify_videokeyframe.c_str(), notify_duration.c_str());
	result_str = tempbuff;

	return result_str;
}

//全局对象
nsRabbitmq::cwRabbitmqPublish* g_RabbitmqSender = nullptr;
bool sendRabbitmqMsg(std::string mqmessage)
{
	if (g_RabbitmqSender == nullptr)
	{
		_debug_to(1, ("Rabbitmq object is null,please restart...\n"));
		return false;
	}

	std::vector<std::string>   vecMessage;
	vecMessage.push_back(mqmessage);

	nsRabbitmq::mmRabbitmqData Rabbitmq_data;
	Rabbitmq_data.index = 0;
	Rabbitmq_data.moreStr = "";
	Rabbitmq_data.moreInt = 0;
	Rabbitmq_data.exchange = rabbitmq_exchange;
	Rabbitmq_data.routekey = rabbitmq_routekey;
	Rabbitmq_data.commandVector.assign(vecMessage.begin(), vecMessage.end());
	g_RabbitmqSender->send(Rabbitmq_data);

	return true;
}

#endif

#if 1 //TCP消息

#define PNP_SYMBOL					0x1111
#define RECVBUFF_MAXSIZE			40960

//-----------------向合成服务发消息[仅音频任务使用]-----------------
typedef struct tagDGHDR
{
	u_int		type;		// type of packet
	u_int		len;		// total length of packet
	u_short     symbol;
	u_long      msg;
	u_short     checksum;	// checksum of header
	u_short     off;		// data offset in the packet
} DG_HDR, *LPDG_HDR;
bool IsValidPacket_DGHDR(const char* buf, int len)
{
	LPDG_HDR pHdr = (LPDG_HDR)buf;
	if (pHdr == NULL) return false;

	if (len < sizeof(DG_HDR))		return false;
	if (pHdr->symbol != PNP_SYMBOL)	return false;
	if (pHdr->off < sizeof(DG_HDR))	return false;
	if (pHdr->off > pHdr->len)		return false;
	if (pHdr->len > (u_int)len)		return false;

	if (Checksum((u_short*)pHdr, pHdr->off) != 0)
		return false;

	return true;
}
bool SendTcpMsg_DGHDR(std::string ip, short port, std::string sendmsg, std::string& recvmsg, int& sockfd, long recv_timeout = 3)
{
	std::wstring uni_msg;
	ansi_to_unicode(sendmsg.c_str(), sendmsg.length(), uni_msg);

	int sfd = -1;
	struct sockaddr_in serveraddr;

	bool bRet = true;
	if (bRet)//建立套接字
	{
		sfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sfd == -1)
		{
			perror(("socket failed\n"));
			bRet = false;
		}
	}

	if (bRet)//发起连接请求
	{
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_port = htons(port);
		serveraddr.sin_addr.s_addr = inet_addr(ip.c_str());
		if (connect(sfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1)
		{
			_debug_to(0, ("addr: %s:%u ,connect failed\n"), inet_ntoa(serveraddr.sin_addr), ntohs(serveraddr.sin_port));
			bRet = false;
		}
	}

	if (bRet)//发送buf中的内容
	{
		char* data = (char*)uni_msg.c_str();
		int   datalen = uni_msg.length() * 2;

		DG_HDR hdr;
		hdr.type = 100;
		hdr.len = sizeof(hdr) + datalen;
		hdr.symbol = PNP_SYMBOL;
		hdr.msg = 0;//default
		hdr.checksum = 0;
		hdr.off = sizeof(hdr);
		hdr.checksum = Checksum((u_short*)&hdr, hdr.off);

		int bufferlen = sizeof(hdr) + datalen;
		char* pbuffer = new char[bufferlen];
		memcpy(pbuffer, &hdr, sizeof(hdr));
		memcpy(pbuffer + sizeof(hdr), data, datalen);

		int ret = send(sfd, pbuffer, bufferlen, 0);
		if (ret <= 0)
		{
			std::string sendmsg_utf8; ansi_to_utf8(sendmsg.c_str(), sendmsg.length(), sendmsg_utf8);
			_debug_to(0, ("addr: %s:%u ,[type1] send message failed: %s\n"), inet_ntoa(serveraddr.sin_addr), ntohs(serveraddr.sin_port), sendmsg_utf8.c_str());
			bRet = false;
		}
		else
		{
			std::string sendmsg_utf8; ansi_to_utf8(sendmsg.c_str(), sendmsg.length(), sendmsg_utf8);
			_debug_to(0, ("addr: %s:%u ,[type1]send message success: %s\n"), inet_ntoa(serveraddr.sin_addr), ntohs(serveraddr.sin_port), sendmsg_utf8.c_str());
		}
		delete[] pbuffer;
	}

	if (bRet)
	{
		//----------------------------------------------------//
		//timeout recv message
		struct timeval tv;
		tv.tv_sec = recv_timeout;//s
		tv.tv_usec = 0;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sfd, &readfds);
		select(sfd + 1, &readfds, NULL, NULL, &tv);

		int recvsize = 0;
		char recvbuff[RECVBUFF_MAXSIZE] = { 0 };
		if (FD_ISSET(sfd, &readfds))
			recvsize = recv(sfd, recvbuff, sizeof(recvbuff), 0);//接收消息
		if (recvsize > 0 && recvsize < RECVBUFF_MAXSIZE)
		{
			if (IsValidPacket_DGHDR(recvbuff, recvsize))
			{
				LPDG_HDR pHdr = (LPDG_HDR)recvbuff;
				if (pHdr->off == 0) pHdr->off = sizeof(DG_HDR);//fix error
				recvsize = recvsize - pHdr->off;

				char tempbuff[RECVBUFF_MAXSIZE] = { 0 };
				memcpy(tempbuff, &(recvbuff[pHdr->off]), sizeof(char) * recvsize);
				memset(recvbuff, 0, sizeof(char) * RECVBUFF_MAXSIZE);
				memcpy(recvbuff, tempbuff, sizeof(char) * RECVBUFF_MAXSIZE);
			}

			std::wstring uni_recvmsg;
			uni_recvmsg = (wchar_t*)recvbuff;
			unicode_to_ansi(uni_recvmsg.c_str(), uni_recvmsg.length(), recvmsg);
		}
		//----------------------------------------------------//
	}

exitsend:
	sockfd = sfd;
	return bRet;
}
bool RecvTcpMsg_DGHDR(int sfd, std::string& recvmsg, long recv_timeout = 3)
{
	bool bRet = false;
	if (sfd != -1)
	{
		//----------------------------------------------------//
		//timeout recv message
		struct timeval tv;
		tv.tv_sec = recv_timeout;//s
		tv.tv_usec = 0;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sfd, &readfds);
		select(sfd + 1, &readfds, NULL, NULL, &tv);

		int recvsize = 0;
		char recvbuff[RECVBUFF_MAXSIZE] = { 0 };
		if (FD_ISSET(sfd, &readfds))
			recvsize = recv(sfd, recvbuff, sizeof(recvbuff), 0);//接收消息
		if (recvsize > 0 && recvsize < RECVBUFF_MAXSIZE)
		{
			if (IsValidPacket_DGHDR(recvbuff, recvsize))
			{
				LPDG_HDR pHdr = (LPDG_HDR)recvbuff;
				if (pHdr->off == 0) pHdr->off = sizeof(DG_HDR);//fix error
				recvsize = recvsize - pHdr->off;

				char tempbuff[RECVBUFF_MAXSIZE] = { 0 };
				memcpy(tempbuff, &(recvbuff[pHdr->off]), sizeof(char) * recvsize);
				memset(recvbuff, 0, sizeof(char) * RECVBUFF_MAXSIZE);
				memcpy(recvbuff, tempbuff, sizeof(char) * RECVBUFF_MAXSIZE);
			}

			bRet = true;
			std::wstring uni_recvmsg;
			uni_recvmsg = (wchar_t*)recvbuff;
			unicode_to_ansi(uni_recvmsg.c_str(), uni_recvmsg.length(), recvmsg);
		}
		//----------------------------------------------------//
	}

	return bRet;
}
//-----------------向录制服务发消息-----------------
typedef struct tagPLAYOUTNODE
{
	u_short		channel;
	u_short		studio;
	u_long		type;
} PLAYOUTNODE, *LPPLAYOUTNODE;
typedef struct tagPNPHDR
{
	u_char			ver;		// version
	u_char			type;		// type of packet
	u_short			symbol;		// symbol of RPC packet, it must equal 0x1111
	u_int			len;		// total length of packet
	PLAYOUTNODE		src;
	u_long			dst;
	u_long			msg;
	u_short			checksum;	// checksum of header
	u_short			off;		// data offset in the packet
} PNP_HDR, *LPPNP_HDR;
bool IsValidPacket_PNPHDR(const char* buf, int len)
{
	LPPNP_HDR pHdr = (LPPNP_HDR)buf;

	if (len < sizeof(PNP_HDR))       return false;
	if (pHdr->ver > 1)              return false;
	if (pHdr->symbol != PNP_SYMBOL) return false;
	if (pHdr->off < sizeof(PNP_HDR)) return false;
	if (pHdr->off > pHdr->len)      return false;
	if (pHdr->len > (u_int)len)     return false;

	if (Checksum((u_short*)pHdr, pHdr->off) != 0)
		return false;

	return true;
}
//
bool SendTcpMsg_PNPHDR(std::string ip, short port, std::string sendmsg, std::string& recvmsg, int& sockfd, long recv_timeout = 3)
{
	std::wstring uni_msg;
	ansi_to_unicode(sendmsg.c_str(), sendmsg.length(), uni_msg);

	int sfd = -1;
	struct sockaddr_in serveraddr;

	bool bRet = true;
	if (bRet)//建立套接字
	{
		sfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sfd == -1)
		{
			perror(("socket failed\n"));
			bRet = false;
		}
	}

	if (bRet)//发起连接请求
	{
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_port = htons(port);
		serveraddr.sin_addr.s_addr = inet_addr(ip.c_str());
		if (connect(sfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1)
		{
			_debug_to(0, ("addr: %s:%u ,connect failed\n"), inet_ntoa(serveraddr.sin_addr), ntohs(serveraddr.sin_port));
			bRet = false;
		}
	}

	if (bRet)//发送buf中的内容
	{
		char* data = (char*)uni_msg.c_str();
		int   datalen = uni_msg.length() * 2;

		PNP_HDR hdr;
		hdr.ver = 1;
		hdr.type = 100;
		hdr.symbol = PNP_SYMBOL;
		hdr.len = sizeof(hdr) + datalen;
		hdr.src.channel = 0;
		hdr.src.studio = 0;
		hdr.src.type = 0;
		hdr.dst = 0;
		hdr.msg = 0;
		hdr.checksum = 0;
		hdr.off = sizeof(hdr);
		hdr.checksum = Checksum((u_short*)&hdr, hdr.off);

		int bufferlen = sizeof(hdr) + datalen;
		char* pbuffer = new char[bufferlen];
		memcpy(pbuffer, &hdr, sizeof(hdr));
		memcpy(pbuffer + sizeof(hdr), data, datalen);

		int ret = send(sfd, pbuffer, bufferlen, 0);
		if (ret <= 0)
		{
			std::string sendmsg_utf8; ansi_to_utf8(sendmsg.c_str(), sendmsg.length(), sendmsg_utf8);
			_debug_to(0, ("addr: %s:%u ,[type3]send message failed: %s\n"), inet_ntoa(serveraddr.sin_addr), ntohs(serveraddr.sin_port), sendmsg_utf8.c_str());
			bRet = false;
		}
		else
		{
			std::string sendmsg_utf8; ansi_to_utf8(sendmsg.c_str(), sendmsg.length(), sendmsg_utf8);
			_debug_to(0, ("addr: %s:%u ,[type3]send message success: %s\n"), inet_ntoa(serveraddr.sin_addr), ntohs(serveraddr.sin_port), sendmsg_utf8.c_str());
		}
		delete[] pbuffer;
	}

	if (bRet)
	{
		//----------------------------------------------------//
		//timeout recv message
		struct timeval tv;
		tv.tv_sec = recv_timeout;//s
		tv.tv_usec = 0;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sfd, &readfds);
		select(sfd + 1, &readfds, NULL, NULL, &tv);

		int recvsize = 0;
		char recvbuff[RECVBUFF_MAXSIZE] = { 0 };
		if (FD_ISSET(sfd, &readfds))
			recvsize = recv(sfd, recvbuff, sizeof(recvbuff), 0);//接收消息
		if (recvsize > 0 && recvsize < RECVBUFF_MAXSIZE)
		{
			if (IsValidPacket_PNPHDR(recvbuff, recvsize))
			{
				LPPNP_HDR pHdr = (LPPNP_HDR)recvbuff;
				if (pHdr->off == 0) pHdr->off = sizeof(PNP_HDR);//fix error
				recvsize = recvsize - pHdr->off;

				char tempbuff[RECVBUFF_MAXSIZE] = { 0 };
				memcpy(tempbuff, &(recvbuff[pHdr->off]), sizeof(char) * recvsize);
				memset(recvbuff, 0, sizeof(char) * RECVBUFF_MAXSIZE);
				memcpy(recvbuff, tempbuff, sizeof(char) * RECVBUFF_MAXSIZE);
			}

			bRet = true;
			std::wstring uni_recvmsg;
			uni_recvmsg = (wchar_t*)recvbuff;
			unicode_to_ansi(uni_recvmsg.c_str(), uni_recvmsg.length(), recvmsg);
		}
		//----------------------------------------------------//
	}

exitsend:
	sockfd = sfd;
	return bRet;
};
bool RecvTcpMsg_PNPHDR(int sfd, std::string& recvmsg, long recv_timeout = 3)
{
	bool bRet = false;
	if (sfd != -1)
	{
		//----------------------------------------------------//
		//timeout recv message
		struct timeval tv;
		tv.tv_sec = recv_timeout;//s
		tv.tv_usec = 0;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sfd, &readfds);
		select(sfd + 1, &readfds, NULL, NULL, &tv);

		int recvsize = 0;
		char recvbuff[RECVBUFF_MAXSIZE] = { 0 };
		if (FD_ISSET(sfd, &readfds))
			recvsize = recv(sfd, recvbuff, sizeof(recvbuff), 0);//接收消息
		if (recvsize > 0 && recvsize < RECVBUFF_MAXSIZE)
		{
			if (IsValidPacket_PNPHDR(recvbuff, recvsize))
			{
				LPPNP_HDR pHdr = (LPPNP_HDR)recvbuff;
				if (pHdr->off == 0) pHdr->off = sizeof(PNP_HDR);//fix error
				recvsize = recvsize - pHdr->off;

				char tempbuff[RECVBUFF_MAXSIZE] = { 0 };
				memcpy(tempbuff, &(recvbuff[pHdr->off]), sizeof(char) * recvsize);
				memset(recvbuff, 0, sizeof(char) * RECVBUFF_MAXSIZE);
				memcpy(recvbuff, tempbuff, sizeof(char) * RECVBUFF_MAXSIZE);
			}

			bRet = true;
			std::wstring uni_recvmsg;
			uni_recvmsg = (wchar_t*)recvbuff;
			unicode_to_ansi(uni_recvmsg.c_str(), uni_recvmsg.length(), recvmsg);
		}
		//----------------------------------------------------//
	}

	return bRet;
}

#endif

#if 1 //会话信息
#define SESSION_VALID_TIME			7000			//会话7000秒内有效
#define SESSION_REFRESH_TIME		3600			//会话3600秒刷新一次【有效时间延长7000秒】
#define SESSION_CLEAR_TIME			600				//定时删除过期会话(秒)
typedef struct _sessioninfo
{
	std::string sessionid;
	std::string refresh_token;
	long long   timeout_time;
	_sessioninfo()
	{
		sessionid = "";
		refresh_token = "";
		timeout_time = 0;
	}
	void copydata(const _sessioninfo& item)
	{
		sessionid = item.sessionid;
		refresh_token = item.refresh_token;
		timeout_time = item.timeout_time;
	}
}sessioninfo;
typedef std::map<std::string, sessioninfo> SESSIONINFO_MAP;
SESSIONINFO_MAP Container_sessioninfo;
pthread_mutex_t mutex_sessioninfo;// sessioninfo互斥量

bool addsessioninfo(sessioninfo sessionitem)
{
	pthread_mutex_lock(&mutex_sessioninfo);
	Container_sessioninfo.insert(std::make_pair(sessionitem.sessionid, sessionitem));
	pthread_mutex_unlock(&mutex_sessioninfo);
	return true;
}
bool getsessioninfo(std::string sessionid, sessioninfo& sessionitem)
{
	bool result = false;
	SESSIONINFO_MAP::iterator itFind = Container_sessioninfo.find(sessionid);
	if (itFind != Container_sessioninfo.end())
	{
		pthread_mutex_lock(&mutex_sessioninfo);
		sessionitem.copydata(itFind->second);
		pthread_mutex_unlock(&mutex_sessioninfo);
		result = true;
	}
	return result;
}
bool checksession(std::string sessionid)
{
	bool result = false;
	SESSIONINFO_MAP::iterator itFind = Container_sessioninfo.find(sessionid);
	if (itFind != Container_sessioninfo.end())
	{
		long long time_session = itFind->second.timeout_time;
		long long time_now = gettimecount_now();
		if (time_session > time_now)//未过期
			result = true;
	}
	return result;
}
bool clearsessioninfo(std::string sessionid)
{
	SESSIONINFO_MAP::iterator itFind = Container_sessioninfo.find(sessionid);
	if (itFind != Container_sessioninfo.end())
	{
		pthread_mutex_lock(&mutex_sessioninfo);
		Container_sessioninfo.erase(itFind);
		pthread_mutex_unlock(&mutex_sessioninfo);
	}

	return true;
}

pthread_t threadid_clearsession_thread;
void* pthread_clearsession_thread(void* arg)
{
	while (true)
	{
		for (SESSIONINFO_MAP::iterator it = Container_sessioninfo.begin(); it != Container_sessioninfo.end();)
		{
			long long time_now = gettimecount_now();
			if (it->second.timeout_time < time_now)//会话已过期
			{
				_debug_to(0, ("delete session: sessionid=%s\n"), it->second.sessionid);
				pthread_mutex_lock(&mutex_sessioninfo);
				Container_sessioninfo.erase(it++);//it = Container_sessioninfo.erase(it);//在C++11有效
				pthread_mutex_unlock(&mutex_sessioninfo);
			}
			else
			{
				++it;
			}
		}

		sleep(SESSION_CLEAR_TIME*1000);//600秒[检测会话过期]
	}
}

#endif

#if 1 //Token加解密

typedef struct _authorizationinfo
{
	std::string session;
	int			userid;
	int			usertype;
	std::string usercode;
	std::string username;
	std::string password;
	std::string refresh_token;
	long long   accesstime;

	_authorizationinfo()
	{
		session = "";
		userid = -1;
		usertype = -1;
		usercode = "";
		username = "";
		password = "";
		refresh_token = "";
		accesstime = 0;
	}

	void copydata(const _authorizationinfo& item)
	{
		session = item.session;
		userid = item.userid;
		usertype = item.usertype;
		usercode = item.usercode;
		username = item.username;
		password = item.password;
		refresh_token = item.refresh_token;
		accesstime = item.accesstime;
	}
}authorizationinfo;

#define SESSION_AUTHOTINFO_COUNT  6//解析时会检验,需与下方字符串中数据个数一致
std::string getstring_authorinfo(authorizationinfo authorizationitem)
{
	std::string result = "";
	char buff[BUFF_SZ] = { 0 };
	snprintf(buff, BUFF_SZ, "%s|%d|%d|%s|%s|%s", authorizationitem.session.c_str(), authorizationitem.userid, authorizationitem.usertype, authorizationitem.usercode.c_str(), authorizationitem.username.c_str(), authorizationitem.password.c_str());
	result = buff;

	return result;
}
bool token_encode(authorizationinfo authorizationitem, std::string& token)
{
	//1.自定义加密用户信息
	std::string b64_sessioninfo = jwt_base64_encode(getstring_authorinfo(authorizationitem));
	std::string sessioninfo = std::string("A") + b64_sessioninfo;//添加第一个字符【避免Base64被解密】

	//2.jwt加密【accesstime保证每次token不一样,不解析】
	char buff[BUFF_SZ] = { 0 };
	std::string key = "digit";
	std::string header = "{\"alg\":\"HS256\",\"type\":\"JWT\"}";
	std::string payload = "";
	snprintf(buff, BUFF_SZ, "{\"session\":\"%s\",\"authorinfo\":\"%s\",\"accesstime\":%lld}", authorizationitem.session.c_str(), sessioninfo.c_str(), gettimecount_now()); payload = buff;
	if (!string_to_token(header, payload, key, token))
		return false;

	return true;
}
bool token_decode(authorizationinfo& authorizationitem, std::string token)
{
	//1.jwt解密
	std::string key = "digit";
	std::string header = "";
	std::string payload = "";
	if (!token_to_string(header, payload, key, token))
		return false;
	if (key.empty() || header.empty() || payload.empty())
		return false;

	//2.自定义解密用户信息
	json::Value json_value = json::Deserialize((char*)payload.c_str());
	if (payload.empty() || json_value.GetType() == json::NULLVal)
		return false;

	json::Object json_obj = json_value.ToObject();
	if (json_obj.HasKey("session"))
		authorizationitem.session = json_obj["session"].ToString();
	if (json_obj.HasKey("accesstime"))
		authorizationitem.accesstime = json_obj["accesstime"].ToInt();
	if (json_obj.HasKey("authorinfo"))
	{
		std::vector<std::string> strVector;
		std::string sessioninfo = json_obj["authorinfo"].ToString();
		sessioninfo.erase(sessioninfo.begin());//删除第一个字符
		std::string b64_sessioninfo = jwt_base64_decode(sessioninfo);

		globalSpliteString(b64_sessioninfo, strVector, std::string("|"));
		if (strVector.size() != SESSION_AUTHOTINFO_COUNT)
			return false;

		size_t i = 0;
		if (authorizationitem.session != strVector[i++]) return false;
		authorizationitem.userid = atoi(strVector[i++].c_str());
		authorizationitem.usertype = atoi(strVector[i++].c_str());
		authorizationitem.usercode = strVector[i++];
		authorizationitem.username = strVector[i++];
		authorizationitem.password = strVector[i++];
	}

	return true;
}

#endif

#if 1 //数据库函数组合,快速调用

//用户剩余可用时间
bool getremaintime_user(int userid, long long& remaintime)
{
	//找到租户id
	int root_userid = userid;
	if (!digitalmysql::isrootuser_id(userid))
	{
		if (!digitalmysql::getuserid_parent(userid, root_userid))
			return false;
	}

	//获取租户usercode
	std::string root_usercode = "";
	if (!digitalmysql::getusercode_id(root_userid, root_usercode))
	{
		return false;
	}
		
	//获取租户可用时间
	if(!digitalmysql::getuserinfo_remaintime(root_usercode, remaintime))
	{
		return false;
	}

	return true;
}
bool setremaintime_user(int userid, long long remaintime)
{
	//找到租户id
	int root_userid = userid;
	if (!digitalmysql::isrootuser_id(userid))
	{
		if (!digitalmysql::getuserid_parent(userid, root_userid))
			return false;
	}

	//获取租户usercode
	std::string root_usercode = "";
	if (!digitalmysql::getusercode_id(root_userid, root_usercode))
	{
		return false;
	}

	//设置租户可用时间
	if (!digitalmysql::setuserinfo_remaintime(root_usercode, remaintime))
	{
		return false;
	}

	return true;
}
//数字人剩余可用时间
bool getremaintime_human(int userid, std::string humanid, long long& remaintime)
{
	//0-【查询id包括：账户本身、子账户/父账户】
	std::vector<int> vecbelongids;
	if (digitalmysql::isrootuser_id(userid))
	{
		//root类型查子账户
		digitalmysql::getuserid_childs(userid, vecbelongids);
	}
	else
	{
		//非root用户查父账户
		int parentid = -1;
		digitalmysql::getuserid_parent(userid, parentid);
		vecbelongids.push_back(parentid);
	}
	vecbelongids.push_back(userid);//添加账户本身

	if (!digitalmysql::gethumaninfo_remaintime(humanid, vecbelongids, remaintime))
		return false;

	return true;
}
bool setremaintime_human(int userid, std::string humanid, long long remaintime)
{
	//0-【查询id包括：账户本身、子账户/父账户】
	std::vector<int> vecbelongids;
	if (digitalmysql::isrootuser_id(userid))
	{
		//root类型查子账户
		digitalmysql::getuserid_childs(userid, vecbelongids);
	}
	else
	{
		//非root用户查父账户
		int parentid = -1;
		digitalmysql::getuserid_parent(userid, parentid);
		vecbelongids.push_back(parentid);
	}
	vecbelongids.push_back(userid);//添加账户本身

	if (!digitalmysql::sethumaninfo_remaintime(humanid, vecbelongids, remaintime))
		return false;

	return true;
}
//数字人标签
bool getlabelstring_human(int userid, std::string humanid, std::string& labelstring)
{
	//0-【查询id包括：账户本身、子账户/父账户】
	std::vector<int> vecbelongids;
	if (digitalmysql::isrootuser_id(userid))
	{
		//root类型查子账户
		digitalmysql::getuserid_childs(userid, vecbelongids);
	}
	else
	{
		//非root用户查父账户
		int parentid = -1;
		digitalmysql::getuserid_parent(userid, parentid);
		vecbelongids.push_back(parentid);
	}
	vecbelongids.push_back(userid);//添加账户本身

	if (!digitalmysql::gethumaninfo_label(humanid, vecbelongids, labelstring))
		return false;

	return true;
}
bool setlabelstring_human(int userid, std::string humanid, std::string labelstring)
{
	//0-【查询id包括：账户本身、子账户/父账户】
	std::vector<int> vecbelongids;
	if (digitalmysql::isrootuser_id(userid))
	{
		//root类型查子账户
		digitalmysql::getuserid_childs(userid, vecbelongids);
	}
	else
	{
		//非root用户查父账户
		int parentid = -1;
		digitalmysql::getuserid_parent(userid, parentid);
		vecbelongids.push_back(parentid);
	}
	vecbelongids.push_back(userid);//添加账户本身

	if (!digitalmysql::sethumaninfo_label(humanid, vecbelongids, labelstring))
		return false;

	return true;
}



#endif

#if 1 //接口返回json

std::string getjson_linyun_error(bool state, std::string message = "")
{
	std::string result = "";
	if (state)
	{
		char buff[BUFF_SZ] = { 0 };
		snprintf(buff, BUFF_SZ, "{ \"state\": \"success\", \"message\": \"%s\"}", message.c_str()); result = buff;
	}
	else
	{
		char buff[BUFF_SZ] = { 0 };
		snprintf(buff, BUFF_SZ, "{ \"state\": \"failed\", \"error_msg\": \"%s\"}", message.c_str()); result = buff;
	}

	return result;
}

std::string getjson_error(int code, std::string errmsg, std::string data = "")
{
	std::string result = "";

	char buff[BUFF_SZ] = { 0 };
	snprintf(buff, BUFF_SZ, "{ \"code\": %d, \"msg\": \"%s\",\"data\":{%s} }", code, errmsg.c_str(), data.c_str());result = buff;

	return result;
}

//登录返回token
std::string getjson_userlogin(authorizationinfo authorizationitem)
{
	bool result = true;
	std::string errmsg = "success";
	std::string result_str = "";

	//检查数据库用户有效性
	if (!digitalmysql::isvaliduser_namepwd(authorizationitem.username, authorizationitem.password))
	{
		errmsg = "check userinfo failed,user maybe disabled...";
		result_str = getjson_error(1, errmsg);
		return result_str;
	}

	//添加本次会话到数据库
	sessioninfo new_sessionitem;
	new_sessionitem.sessionid = md5::getStringMD5(authorizationitem.usercode);//md5加密usercode得到sessionid
	new_sessionitem.refresh_token = authorizationitem.refresh_token;
	new_sessionitem.timeout_time = gettimecount_now() + SESSION_VALID_TIME;
	if (!addsessioninfo(new_sessionitem))
	{
		errmsg = "add session to map failed...";
		result_str = getjson_error(1, errmsg);
		return result_str;
	}
	else
	{
		_debug_to(0, ("insert session: sessionid=%s\n"), new_sessionitem.sessionid);
	}

	std::string str_token;
	if (token_encode(authorizationitem, str_token))
	{
		int	UserID = authorizationitem.userid;
		std::string UserName = authorizationitem.username;

		int AdminFlag = 0;
		digitalmysql::getuserinfo_adminflag(authorizationitem.usercode, AdminFlag);


		long long RemainTime = 0;
		std::string UserRemainTime = "未知";
		if (getremaintime_user(UserID, RemainTime))
			UserRemainTime = gettimeshow_second(RemainTime);

		std::string data = "";
		char buff[BUFF_SZ] = { 0 };
		snprintf(buff, BUFF_SZ, "\"token\":\"%s\",\"UserID\":%d,\"UserName\":\"%s\",\"AdminFlag\":%d, \"UserRemainTime\": \"%s\"", str_token.c_str(), UserID, UserName.c_str(), AdminFlag, UserRemainTime.c_str()); data = buff;
		result_str = getjson_error(0, errmsg, data);
	}
	else
	{
		errmsg = "token encode failed...";
		result_str = getjson_error(1, errmsg);
	}

	return result_str;
}

//数字人列表
std::string getjson_humanlistinfo(std::string humanid = "",int userid = -1)
{
	bool result = true;
	std::string errmsg = "";
	std::string result_str = "";

	//0-【查询id包括：账户本身、子账户/父账户】
	std::vector<int> vecbelongids;
	if (digitalmysql::isrootuser_id(userid))
	{
		//root类型查子账户
		digitalmysql::getuserid_childs(userid, vecbelongids);
	}
	else
	{
		//非root用户查父账户
		int parentid = -1;
		digitalmysql::getuserid_parent(userid, parentid);
		vecbelongids.push_back(parentid);
	}
	vecbelongids.push_back(userid);//添加账户本身
	
	//1-getlist
	digitalmysql::VEC_HUMANINFO vechumaninfo;
	result = digitalmysql::gethumanlistinfo(humanid, vecbelongids, vechumaninfo);
	_debug_to(0, ("gethumanlistinfo: vechumaninfo size=%d\n"), vechumaninfo.size());
	if (!result)
		errmsg = "gethumanlistinfo from mysql failed";

	//2-parsedata
	std::string list_info = "";
	DigitalMan_Items result_object;
	for(size_t i = 0; i < vechumaninfo.size(); i++)
	{
		DigitalMan_Item result_item;
		result_item.HumanID = vechumaninfo[i].humanid;
		result_item.HumanName = vechumaninfo[i].humanname;
		result_item.HumanLabel = vechumaninfo[i].contentid;
		result_item.SpeakSpeed = vechumaninfo[i].speakspeed;
		result_item.Foreground = webpath_from_osspath(vechumaninfo[i].foreground);
		result_item.Background = webpath_from_osspath(vechumaninfo[i].background);
		result_item.Foreground2 = webpath_from_osspath(vechumaninfo[i].foreground2);
		result_item.Background2 = webpath_from_osspath(vechumaninfo[i].background2);

		//RemainTime
		long long human_remaintime = 0;
		if (digitalmysql::gethumaninfo_remaintime(vechumaninfo[i].id, human_remaintime))
			result_item.HumanRemainTime = gettimeshow_day(human_remaintime);
		//Available
		result_item.HumanAvailable = vechumaninfo[i].available;

		//KeyFrame
		std::string format = "";
		std::string base64_encode = "";
		unsigned int width = 1920, height = 1080, bitcount = 32;
		std::string human_keyframe = vechumaninfo[i].keyframe;
		if (aws_enable && !human_keyframe.empty())//在线模式
		{
			std::string folder = getfolder_appdata(); folder = str_replace(folder, std::string("\\"), std::string("/"));
			std::string local_picturepath = folder + std::string("/") + get_path_name(human_keyframe);
			if (!is_existfile(local_picturepath.c_str()))
			{
				std::string http_picturepath = webpath_from_osspath(human_keyframe);
				httpkit::httprequest_download((char*)http_picturepath.c_str(), (char*)local_picturepath.c_str());
			}
			picture::GetPicInfomation(local_picturepath.c_str(), &width, &height, &bitcount, format);
		}
		if (!aws_enable && is_existfile(human_keyframe.c_str()))//本地模式
		{
			picture::GetPicInfomation(human_keyframe.c_str(), &width, &height, &bitcount, format);
			//base64_encode = base64::base64_encode_file(filepath);
		}
		result_item.KeyFrame_Format = format;
		result_item.KeyFrame_Width = width;
		result_item.KeyFrame_Height = height;
		result_item.KeyFrame_BitCount = bitcount;
		result_item.KeyFrame_FilePath = webpath_from_osspath(human_keyframe);
		result_item.KeyFrame_KeyData = base64_encode;
		result_object.vecDigitManItems.push_back(result_item);	
	}

	//3-writejson
	if(result)
		list_info = result_object.writeJson();

	//4-return
	if (!result)
	{
		result_str = getjson_error(1, errmsg, "");
	}
	else
	{
		std::string code = "\"code\": 0,";
		std::string msg = "\"msg\": \"success\",";

		std::string temp_humanlist = "\"HumanList\": [ " + list_info + "]";
		list_info = temp_humanlist;

		result_str = "{" + code + msg + "\"data\":{" + list_info + "}" + "}";//too long,must use string append
	}

	return result_str;
}

//动作列表
std::string getjson_actionlistinfo(std::string humanid)
{
	bool result = true;
	std::string errmsg = "";
	std::string result_str = "";

	//1-getlist
	digitalmysql::VEC_HUMANACTIONINFO vechumanactioninfo;
	result = digitalmysql::getactionlist(humanid, vechumanactioninfo);
	_debug_to(0, ("getactionlist: vechumanactioninfo size=%d\n"), vechumanactioninfo.size());
	if (!result)
		errmsg = "getactionlist from mysql failed";

	//2-parsedata
	std::string list_info = "";
	DigitalMan_ActionItems result_object;
	for (size_t i = 0; i < vechumanactioninfo.size(); i++)
	{
		DigitalMan_ActionItem result_item;
		result_item.ActionID = vechumanactioninfo[i].id;
		result_item.ActionName = vechumanactioninfo[i].actionname;
		result_item.ActionKeyframe = vechumanactioninfo[i].actionkeyframe;
		result_item.ActionVideo = vechumanactioninfo[i].actionvideo;
		result_item.ActionDuration = vechumanactioninfo[i].actionduration;
		result_item.VideoWidth = vechumanactioninfo[i].videowidth;
		result_item.VideoHeight = vechumanactioninfo[i].videoheight;
		result_object.vecDigitManActionItems.push_back(result_item);
	}

	//3-writejson
	if (result)
		list_info = result_object.writeJson();

	//4-return
	if (!result)
	{
		result_str = getjson_error(1, errmsg, "");
	}
	else
	{
		std::string code = "\"code\": 0,";
		std::string msg = "\"msg\": \"success\",";

		std::string temp_actionlist = "\"ActionList\": [ " + list_info + "]";
		list_info = temp_actionlist;

		result_str = "{" + code + msg + "\"data\":{" + list_info + "}" + "}";//too long,must use string append
	}

	return result_str;
}

//任务列表-查询最后编辑的版本
std::string getjson_humanhistoryinfo(digitalmysql::VEC_FILTERINFO& vecfilterinfo, std::string order_key = "createtime", int order_way = 1, int pagesize = 10, int pagenum = 1,int userid = -1)
{
	bool result = true;
	std::string errmsg = "";
	std::string result_str = "";

	//0-【查询id包括：账户本身、子账户/父账户】
	std::vector<int> vecbelongids;
	if (digitalmysql::isrootuser_id(userid))
	{
		//root类型查子账户
		digitalmysql::getuserid_childs(userid, vecbelongids);
	}
	else
	{
		//非root用户查父账户
		int parentid = -1;
		digitalmysql::getuserid_parent(userid, parentid);
		vecbelongids.push_back(parentid);
	}
	vecbelongids.push_back(userid);//添加账户本身

	//1-getlist
	int total = 0; digitalmysql::VEC_TASKINFO vectaskhistory;
	result = digitalmysql::gettaskhistoryinfo(vecfilterinfo, order_key, order_way, pagesize, pagenum, total, vectaskhistory, vecbelongids);
	_debug_to(0, ("get taskhistory size=%d\n"), vectaskhistory.size());
	if (!result)
		errmsg = "get taskhistory from mysql failed";

	//2-parsedata
	std::string other_info = "";
	std::string history_info = "";
	DigitalMan_Tasks result_object;
	for (size_t i = 0; i < vectaskhistory.size(); i++)
	{
		DigitalMan_Task result_item;
		result_item.TaskGroupID = vectaskhistory[i].groupid;
		result_item.TaskID = vectaskhistory[i].taskid;
		result_item.TaskType = vectaskhistory[i].tasktype;
		result_item.TaskMoodType = vectaskhistory[i].moodtype;
		result_item.TaskName = vectaskhistory[i].taskname;
		result_item.TaskVersionName = vectaskhistory[i].versionname;
		result_item.TaskVersion = vectaskhistory[i].version;
		result_item.TaskState = vectaskhistory[i].taskstate;
		result_item.TaskProgerss = vectaskhistory[i].taskprogress;
		result_item.TaskSpeakSpeed = vectaskhistory[i].speakspeed;
		result_item.TaskInputSsml = vectaskhistory[i].ssmltext;
		result_item.TaskCreateTime = vectaskhistory[i].createtime;
		result_item.TaskIsLastEdit = vectaskhistory[i].islastedit;
		result_item.TaskHumanID = vectaskhistory[i].humanid;
		result_item.TaskHumanName = vectaskhistory[i].humanname;
		result_item.Foreground = webpath_from_osspath(vectaskhistory[i].foreground);
		result_item.Front_left = vectaskhistory[i].front_left;
		result_item.Front_right = vectaskhistory[i].front_right;
		result_item.Front_top = vectaskhistory[i].front_top;
		result_item.Front_bottom = vectaskhistory[i].front_bottom;
		result_item.Background = webpath_from_osspath(vectaskhistory[i].background);
		result_item.BackAudio = webpath_from_osspath(vectaskhistory[i].backaudio);
		result_item.Back_volume = vectaskhistory[i].back_volume;
		result_item.Back_loop = vectaskhistory[i].back_loop;
		result_item.Back_start = vectaskhistory[i].back_start;
		result_item.Back_end = vectaskhistory[i].back_end;
		result_item.Window_width = vectaskhistory[i].window_width;
		result_item.Window_height = vectaskhistory[i].window_height;

		//KeyFrame
		std::string format = "";
		std::string base64_encode = "";
		unsigned int width = 1920, height = 1080, bitcount = 32;
		std::string task_keyframe = vectaskhistory[i].video_keyframe;
		if (task_keyframe.empty())//生成中或生成失败状态
		{
			//使用数字人的关键帧
			std::string taskhumanid = vectaskhistory[i].humanid;
			digitalmysql::humaninfo taskhumanitem;
			if (digitalmysql::gethumaninfo(taskhumanid, taskhumanitem))
				task_keyframe = webpath_from_osspath(taskhumanitem.keyframe);
		}

#if 0	//在线模式不获取任务关键帧宽高
		if (aws_enable && !task_keyframe.empty())//在线模式
		{
			std::string folder = getfolder_appdata(); folder = str_replace(folder, std::string("\\"), std::string("/"));
			std::string local_picturepath = folder + std::string("/") + get_path_name(task_keyframe);
			if (!is_existfile(local_picturepath.c_str()))
			{
				std::string http_picturepath = task_keyframe;
				httpkit::httprequest_download((char*)http_picturepath.c_str(), (char*)local_picturepath.c_str());
			}
			picture::GetPicInfomation(local_picturepath.c_str(), &width, &height, &bitcount, format);
		}
#endif
		if (!aws_enable && is_existfile(task_keyframe.c_str()))//本地模式
		{
			picture::GetPicInfomation(task_keyframe.c_str(), &width, &height, &bitcount, format);
			//base64_encode = base64::base64_encode_file(filepath);
		}
		result_item.KeyFrame_Format = format;
		result_item.KeyFrame_Width = width;
		result_item.KeyFrame_Height = height;
		result_item.KeyFrame_BitCount = bitcount;
		result_item.KeyFrame_FilePath = webpath_from_osspath(task_keyframe);
		result_item.KeyFrame_KeyData = base64_encode;

		result_item.Audio_Format = vectaskhistory[i].audio_format;
		result_item.Audio_File   = webpath_from_osspath(vectaskhistory[i].audio_path);
		result_item.Audio_Duration = gettimetext_millisecond(vectaskhistory[i].audio_length);
		result_item.Video_Format = vectaskhistory[i].video_format;
		result_item.Video_Width  = vectaskhistory[i].video_width;
		result_item.Video_Height = vectaskhistory[i].video_height;
		result_item.Video_Fps    = vectaskhistory[i].video_fps;
		result_item.Video_File   = webpath_from_osspath(vectaskhistory[i].video_path);
		result_item.Video_Duration = gettimetext_framecount(vectaskhistory[i].video_length, vectaskhistory[i].video_fps);
		result_object.vecDigitManTasks.push_back(result_item);
	}

	//3-writejson
	if (result)
		history_info = result_object.writeJson();	

	//4-return
	if (!result)
	{
		result_str = getjson_error(1, errmsg, "");
	}
	else
	{
		std::string code = "\"code\": 0,";
		std::string msg = "\"msg\": \"success\",";

		char temp[256] = { 0 };
		std::string temp_otherinfo;
		snprintf(temp, 256, "\"DataTotal\":%d, \"PageSize\":%d, \"PageNum\":%d,", total, pagesize, pagenum); temp_otherinfo = temp;
		other_info = temp_otherinfo;

		std::string temp_humandata = "\"HumanData\": [ " + history_info + "]";
		history_info = temp_humandata;

		result_str = "{" + code + msg + "\"data\":{" + other_info + history_info + "}" + "}";//too long,must use string append
	}

	return result_str;
}
//任务列表-按任务组ID查询所有版本
std::string getjson_taskgroupinfo(std::string groupid)
{
	bool result = true;
	std::string errmsg = "";
	std::string result_str = "";

	//1-getlist
	digitalmysql::VEC_TASKINFO vectaskgroup;
	result = digitalmysql::gettaskgroupinfo(groupid, vectaskgroup);
	_debug_to(0, ("get taskgroup size=%d\n"), vectaskgroup.size());
	if (!result)
		errmsg = "get taskgroup from mysql failed";

	//2-parsedata
	std::string other_info = "";
	std::string history_info = "";
	DigitalMan_Tasks result_object;
	for (size_t i = 0; i < vectaskgroup.size(); i++)
	{
		DigitalMan_Task result_item;
		result_item.TaskGroupID = vectaskgroup[i].groupid;
		result_item.TaskID = vectaskgroup[i].taskid;
		result_item.TaskType = vectaskgroup[i].tasktype;
		result_item.TaskMoodType = vectaskgroup[i].moodtype;
		result_item.TaskName = vectaskgroup[i].taskname;
		result_item.TaskVersionName = vectaskgroup[i].versionname;
		result_item.TaskVersion = vectaskgroup[i].version;
		result_item.TaskState = vectaskgroup[i].taskstate;
		result_item.TaskProgerss = vectaskgroup[i].taskprogress;
		result_item.TaskSpeakSpeed = vectaskgroup[i].speakspeed;
		result_item.TaskInputSsml = vectaskgroup[i].ssmltext;
		result_item.TaskCreateTime = vectaskgroup[i].createtime;
		result_item.TaskIsLastEdit = vectaskgroup[i].islastedit;
		result_item.TaskHumanID = vectaskgroup[i].humanid;
		result_item.TaskHumanName = vectaskgroup[i].humanname;
		result_item.Foreground = webpath_from_osspath(vectaskgroup[i].foreground);
		result_item.Front_left = vectaskgroup[i].front_left;
		result_item.Front_right = vectaskgroup[i].front_right;
		result_item.Front_top = vectaskgroup[i].front_top;
		result_item.Front_bottom = vectaskgroup[i].front_bottom;
		result_item.Background = webpath_from_osspath(vectaskgroup[i].background);
		result_item.BackAudio = webpath_from_osspath(vectaskgroup[i].backaudio);
		result_item.Back_volume = vectaskgroup[i].back_volume;
		result_item.Back_loop = vectaskgroup[i].back_loop;
		result_item.Back_start = vectaskgroup[i].back_start;
		result_item.Back_end = vectaskgroup[i].back_end;
		result_item.Window_width = vectaskgroup[i].window_width;
		result_item.Window_height = vectaskgroup[i].window_height;

		//KeyFrame
		std::string format = "";
		std::string base64_encode = "";
		unsigned int width = 1920, height = 1080, bitcount = 32;
		std::string task_keyframe = vectaskgroup[i].video_keyframe;
		if (task_keyframe.empty())//生成中或生成失败状态
		{
			//使用数字人的关键帧
			std::string taskhumanid = vectaskgroup[i].humanid;
			digitalmysql::humaninfo taskhumanitem;
			if (digitalmysql::gethumaninfo(taskhumanid, taskhumanitem))
				task_keyframe = webpath_from_osspath(taskhumanitem.keyframe);
		}

#if 0	//在线模式不获取任务关键帧宽高
		if (aws_enable && !task_keyframe.empty())//在线模式
		{
			std::string folder = getfolder_appdata(); folder = str_replace(folder, std::string("\\"), std::string("/"));
			std::string local_picturepath = folder + std::string("/") + get_path_name(task_keyframe);
			if (!is_existfile(local_picturepath.c_str()))
			{
				std::string http_picturepath = task_keyframe;
				httpkit::httprequest_download((char*)http_picturepath.c_str(), (char*)local_picturepath.c_str());
			}
			picture::GetPicInfomation(local_picturepath.c_str(), &width, &height, &bitcount, format);
		}
#endif
		//本地模式
		if (!aws_enable && is_existfile(task_keyframe.c_str()))
		{
			picture::GetPicInfomation(task_keyframe.c_str(), &width, &height, &bitcount, format);
			//base64_encode = base64::base64_encode_file(filepath);
		}
		result_item.KeyFrame_Format = format;
		result_item.KeyFrame_Width = width;
		result_item.KeyFrame_Height = height;
		result_item.KeyFrame_BitCount = bitcount;
		result_item.KeyFrame_FilePath = webpath_from_osspath(task_keyframe);
		result_item.KeyFrame_KeyData = base64_encode;

		result_item.Audio_Format = vectaskgroup[i].audio_format;
		result_item.Audio_File = webpath_from_osspath(vectaskgroup[i].audio_path);
		result_item.Audio_Duration = gettimetext_millisecond(vectaskgroup[i].audio_length);
		result_item.Video_Format = vectaskgroup[i].video_format;
		result_item.Video_Width = vectaskgroup[i].video_width;
		result_item.Video_Height = vectaskgroup[i].video_height;
		result_item.Video_Fps = vectaskgroup[i].video_fps;
		result_item.Video_File = webpath_from_osspath(vectaskgroup[i].video_path);
		result_item.Video_Duration = gettimetext_framecount(vectaskgroup[i].video_length, vectaskgroup[i].video_fps);
		result_object.vecDigitManTasks.push_back(result_item);
	}

	//3-writejson
	if (result)
		history_info = result_object.writeJson();

	//4-return
	if (!result)
	{
		result_str = getjson_error(1, errmsg, "");
	}
	else
	{
		std::string code = "\"code\": 0,";
		std::string msg = "\"msg\": \"success\",";

		std::string temp_humandata = "\"HumanData\": [ " + history_info + "]";
		history_info = temp_humandata;

		result_str = "{" + code + msg + "\"data\":{" + history_info + "}" + "}";//too long,must use string append
	}

	return result_str;
}

//背景资源列表
std::string getjson_tasksourcelistinfo(int userid = -1)
{
	bool result = true;
	std::string errmsg = "";
	std::string result_str = "";

	//0-【查询id包括：账户本身、子账户/父账户】
	std::vector<int> vecbelongids;
	if (digitalmysql::isrootuser_id(userid))
	{
		//root类型查子账户
		digitalmysql::getuserid_childs(userid, vecbelongids);
	}
	else
	{
		//非root用户查父账户
		int parentid = -1;
		digitalmysql::getuserid_parent(userid, parentid);
		vecbelongids.push_back(parentid);
	}
	vecbelongids.push_back(userid);//添加账户本身

	//1-getlist
	digitalmysql::VEC_TASKSOURCEINFO vectasksourceinfo;
	result = digitalmysql::gettasksourcelist(vecbelongids, vectasksourceinfo);
	_debug_to(0, ("gettasksourcelist: vectasksourceinfo size=%d\n"), vectasksourceinfo.size());
	if (!result)
		errmsg = "gettasksourcelist from mysql failed";

	//2-parsedata
	std::string image_list_info = "";
	DigitalMan_TaskSources image_result_object;
	for (size_t i = 0; i < vectasksourceinfo.size(); i++)
	{
		if (vectasksourceinfo[i].sourcetype == digitalmysql::source_image)
		{
			DigitalMan_TaskSource result_item;
			result_item.TaskSource_Id = vectasksourceinfo[i].id;
			result_item.TaskSource_Type = digitalmysql::source_image;
			result_item.TaskSource_FilePath = webpath_from_osspath(vectasksourceinfo[i].sourcepath);		//此路径为https或本地路径，不会是OSS路径
			result_item.TaskSource_KeyFrame = "";
			result_item.TaskSource_Width = vectasksourceinfo[i].sourcewidth;
			result_item.TaskSource_Height = vectasksourceinfo[i].sourceheight;
			image_result_object.vecDigitManTaskSources.push_back(result_item);
		}
	}
	std::string video_list_info = "";
	DigitalMan_TaskSources video_result_object;
	for (size_t j = 0; j < vectasksourceinfo.size(); j++)
	{
		if (vectasksourceinfo[j].sourcetype == digitalmysql::source_video)
		{
			DigitalMan_TaskSource result_item;
			result_item.TaskSource_Id = vectasksourceinfo[j].id;
			result_item.TaskSource_Type = digitalmysql::source_video;
			result_item.TaskSource_FilePath = webpath_from_osspath(vectasksourceinfo[j].sourcepath);		//此路径为https或本地路径，不会是OSS路径
			result_item.TaskSource_KeyFrame = webpath_from_osspath(vectasksourceinfo[j].sourcekeyframe);	//此路径为https或本地路径，不会是OSS路径
			result_item.TaskSource_Width = vectasksourceinfo[j].sourcewidth;
			result_item.TaskSource_Height = vectasksourceinfo[j].sourceheight;
			video_result_object.vecDigitManTaskSources.push_back(result_item);
		}
	}
	std::string audio_list_info = "";
	DigitalMan_TaskSources audio_result_object;
	for (size_t k = 0; k < vectasksourceinfo.size(); k++)
	{
		if (vectasksourceinfo[k].sourcetype == digitalmysql::source_audio)
		{
			DigitalMan_TaskSource result_item;
			result_item.TaskSource_Id = vectasksourceinfo[k].id;
			result_item.TaskSource_Type = digitalmysql::source_audio;
			result_item.TaskSource_FilePath = webpath_from_osspath(vectasksourceinfo[k].sourcepath);		//此路径为https或本地路径，不会是OSS路径
			result_item.TaskSource_KeyFrame = "";
			result_item.TaskSource_Width = 0;
			result_item.TaskSource_Height = 0;
			audio_result_object.vecDigitManTaskSources.push_back(result_item);
		}
	}

	//3-writejson
	if (result)
	{
		image_list_info = image_result_object.writeJson();
		video_list_info = video_result_object.writeJson();
		audio_list_info = audio_result_object.writeJson();
	}

	//4-return
	if (!result)
	{
		result_str = getjson_error(1, errmsg, "");
	}
	else
	{
		std::string code = "\"code\": 0,";
		std::string msg = "\"msg\": \"success\",";

		std::string temp_imagelist = "\"ImageList\": [ " + image_list_info + "], ";
		image_list_info = temp_imagelist;
		std::string temp_videolist = "\"VideoList\": [ " + video_list_info + "], ";
		video_list_info = temp_videolist;
		std::string temp_audiolist = "\"AudioList\": [ " + audio_list_info + "] ";
		audio_list_info = temp_audiolist;

		result_str = "{" + code + msg + "\"data\":{" + image_list_info + video_list_info + audio_list_info + "}" + "}";//too long,must use string append
	}

	return result_str;
}

//用户
std::string getjson_userlistinfo(digitalmysql::VEC_FILTERINFO& vecfilterinfo, int pagesize = 10, int pagenum = 1)
{
	bool result = true;
	std::string errmsg = "";
	std::string result_str = "";

	//1-getlist
	int total = 0; digitalmysql::VEC_USERINFO vecuserinfo;
	result = digitalmysql::getuserlistinfo(vecfilterinfo,pagesize,pagenum,total,vecuserinfo);
	_debug_to(0, ("getuserlistinfo: vecuserinfo size=%d\n"), vecuserinfo.size());
	if (!result)
		errmsg = "getuserlistinfo from mysql failed";

	//
	std::vector<std::string> vecRootName;
	for (size_t i = 0; i < vecuserinfo.size(); i++)
	{
		int RootID = 0;
		std::string RootName = "";
		int UserID = vecuserinfo[i].id;

		if (!digitalmysql::isrootuser_id(UserID))
			digitalmysql::getuserid_parent(UserID, RootID);
		if (!digitalmysql::isexistuser_id(RootID) || !digitalmysql::getusername_id(RootID, RootName))
			RootName = vecuserinfo[i].loginName;
		vecRootName.push_back(RootName);
	}

	//2-parsedata
	std::string other_info = "";
	std::string list_info = "";
	DigitalMan_UserItems result_object;
	for (size_t i = 0; i < vecuserinfo.size(); i++)
	{
		DigitalMan_UserItem result_item;
		result_item.UserID = vecuserinfo[i].id;
		result_item.UserName = vecuserinfo[i].loginName;
		result_item.UserPassWord = vecuserinfo[i].loginPassword;
		result_item.RootName = vecRootName[i];
		result_item.AdminFlag = vecuserinfo[i].adminflag;
		result_item.UserType = vecuserinfo[i].usertype;
		result_item.ServiceType = vecuserinfo[i].usertype;
		result_item.ProjectName = vecuserinfo[i].projectName;
		result_item.ReaminTime = gettimeshow_second(vecuserinfo[i].remainTime);
		result_item.DeadlineTime = vecuserinfo[i].deadlineTime;
		result_item.UserEmail = vecuserinfo[i].email;
		result_item.UserPhone = vecuserinfo[i].phone;
		result_object.vecDigitManUserItems.push_back(result_item);
	}

	//3-writejson
	if (result)
		list_info = result_object.writeJson();

	//4-return
	if (!result)
	{
		result_str = getjson_error(1, errmsg, "");
	}
	else
	{
		std::string code = "\"code\": 0,";
		std::string msg = "\"msg\": \"success\",";

		char temp[256] = { 0 };
		std::string temp_otherinfo;
		snprintf(temp, 256, "\"DataTotal\":%d, \"PageSize\":%d, \"PageNum\":%d,", total, pagesize, pagenum); temp_otherinfo = temp;
		other_info = temp_otherinfo;

		std::string temp_userlist = "\"UserList\": [ " + list_info + "]";
		list_info = temp_userlist;

		result_str = "{" + code + msg + "\"data\":{" + other_info + list_info + "}" + "}";//too long,must use string append
	}

	return result_str;
}
//订单
std::string getjson_indentlistinfo(digitalmysql::VEC_FILTERINFO& vecfilterinfo, int pagesize = 10, int pagenum = 1)
{
	bool result = true;
	std::string errmsg = "";
	std::string result_str = "";

	//1-getlist
	int total = 0; digitalmysql::VEC_INDENTINFO vecindentinfo;
	result = digitalmysql::getindentlistinfo(vecfilterinfo, pagesize, pagenum, total, vecindentinfo);
	_debug_to(0, ("getindentlistinfo: vecindentinfo size=%d\n"), vecindentinfo.size());
	if (!result)
		errmsg = "getindentlistinfo from mysql failed";

	//2-parsedata
	std::string other_info = "";
	std::string list_info = "";
	DigitalMan_Idents result_object;
	for (size_t i = 0; i < vecindentinfo.size(); i++)
	{
		DigitalMan_Ident result_item;
		result_item.IdentID = vecindentinfo[i].id;
		result_item.RootID = vecindentinfo[i].belongid;
		result_item.RootName = vecindentinfo[i].rootname;
		result_item.ServiceType = vecindentinfo[i].servicetype;
		result_item.OperationWay = vecindentinfo[i].operationway;
		result_item.IndentType = vecindentinfo[i].indenttype;
		result_item.IndentContent = vecindentinfo[i].indentcontent;
		result_item.CreateTime = vecindentinfo[i].createtime;
		result_object.vecDigitManIdents.push_back(result_item);
	}

	//3-writejson
	if (result)
		list_info = result_object.writeJson();

	//4-return
	if (!result)
	{
		result_str = getjson_error(1, errmsg, "");
	}
	else
	{
		std::string code = "\"code\": 0,";
		std::string msg = "\"msg\": \"success\",";

		char temp[256] = { 0 };
		std::string temp_otherinfo;
		snprintf(temp, 256, "\"DataTotal\":%d, \"PageSize\":%d, \"PageNum\":%d,", total, pagesize, pagenum); temp_otherinfo = temp;
		other_info = temp_otherinfo;

		std::string temp_indentlist = "\"IndentList\": [ " + list_info + "]";
		list_info = temp_indentlist;

		result_str = "{" + code + msg + "\"data\":{" + other_info + list_info + "}" + "}";//too long,must use string append
	}

	return result_str;
}
//任务
std::string getjson_tasklistinfo(digitalmysql::VEC_FILTERINFO& vecfilterinfo, int pagesize = 10, int pagenum = 1)
{
	bool result = true;
	std::string errmsg = "";
	std::string result_str = "";

	//1-getlist
	int total = 0; digitalmysql::VEC_TASKINFO vectasklist;
	result = digitalmysql::gettasklistinfo(vecfilterinfo, pagesize, pagenum, total, vectasklist);
	_debug_to(0, ("gettasklistinfo: vectasklist size=%d\n"), vectasklist.size());
	if (!result)
		errmsg = "gettasklistinfo from mysql failed";

	//2-parsedata
	std::string other_info = "";
	std::string list_info = "";
	DigitalMan_TaskExs result_object;
	for (size_t i = 0; i < vectasklist.size(); i++)
	{
		std::string final_path = (vectasklist[i].tasktype == 0) ? (vectasklist[i].audio_path) : (vectasklist[i].video_path);
		std::string final_duration = (vectasklist[i].tasktype == 0) ? (gettimetext_millisecond(vectasklist[i].audio_length)) : (gettimetext_framecount(vectasklist[i].video_length, vectasklist[i].video_fps));
		
		int final_userid = vectasklist[i].belongid; std::string final_username = ""; 
		digitalmysql::getusername_id(final_userid, final_username);

		int final_rootid = final_userid; std::string final_rootname = "";
		if (!digitalmysql::isrootuser_id(final_userid))
			digitalmysql::getuserid_parent(final_userid, final_rootid);
		digitalmysql::getusername_id(final_rootid, final_rootname);

		DigitalMan_TaskEx result_item;
		result_item.TaskGroupID = vectasklist[i].groupid;
		result_item.TaskID = vectasklist[i].taskid;
		result_item.TaskType = vectasklist[i].tasktype;
		result_item.TaskState = vectasklist[i].taskstate;
		result_item.TaskPriority = vectasklist[i].priority;
		result_item.TaskFileName = get_path_name(final_path);

		result_item.TaskName = vectasklist[i].taskname;
		result_item.TaskDuration = final_duration;
		result_item.TaskCreateTime = vectasklist[i].createtime;
		result_item.TaskFinishTime = vectasklist[i].finishtime;

		result_item.TaskUserID = final_userid;
		result_item.TaskUserName = final_username;
		result_item.TaskRootID = final_rootid;
		result_item.TaskRootName = final_rootname;
		result_item.TaskHumanID = vectasklist[i].humanid;
		result_item.TaskHumanName = vectasklist[i].humanname;
		result_object.vecDigitManTaskExs.push_back(result_item);
	}

	//3-writejson
	if (result)
		list_info = result_object.writeJson();

	//4-return
	if (!result)
	{
		result_str = getjson_error(1, errmsg, "");
	}
	else
	{
		std::string code = "\"code\": 0,";
		std::string msg = "\"msg\": \"success\",";

		char temp[256] = { 0 };
		std::string temp_otherinfo;
		snprintf(temp, 256, "\"DataTotal\":%d, \"PageSize\":%d, \"PageNum\":%d,", total, pagesize, pagenum); temp_otherinfo = temp;
		other_info = temp_otherinfo;

		std::string temp_indentlist = "\"TaskList\": [ " + list_info + "]";
		list_info = temp_indentlist;

		result_str = "{" + code + msg + "\"data\":{" + other_info + list_info + "}" + "}";//too long,must use string append
	}

	return result_str;
}
#endif

#if 1 //VideoMake
#define RUNTASK_TIMEOUT	(3600*24)//执行任务超时时间 

//ActorTaskinfo 结构体
typedef struct _actortaskinfo
{
	int			ActorPriority;
	long long   ActorCreateTime;
	std::string ActorMessageID;
	int			ActorTaskID;
	int			ActorTaskType;
	int			ActorMoodType;
	double		ActorTaskSpeed;
	std::string ActorTaskName;
	std::string ActorTaskText;
	std::string ActorTaskAudio;
	std::string ActorTaskHumanID;
	int			ActorTaskState;				//-1=waitmerge,0=merging,1=mergesuccess,2=mergefailed
	int			ActorTaskSocket;			//与Actor通信的Socket[异步执行任务需要]
	//指定模型文件
	std::string SpeakModelPath;				//../0ModelFile/test/TTS/speak/snapshot_iter_1668699.pdz
	std::string PWGModelPath;				//../0ModelFile/test/TTS/pwg/snapshot_iter_1000000.pdz
	std::string MouthModelPath;				//../0ModelFile/test/W2L/file/xxx.pth
	std::string FaceModelPath;				//../0ModelFile/test/W2L/file/shenzhen_v3_20230227.dfm

	_actortaskinfo()
	{
		ActorPriority = 0;
		ActorCreateTime = 0;
		ActorMessageID = "";
		ActorTaskID = 0;
		ActorTaskType = 1;
		ActorMoodType = 0;
		ActorTaskSpeed = 1.0;
		ActorTaskName = "";
		ActorTaskText = "";
		ActorTaskAudio = "";
		ActorTaskHumanID = "";
		ActorTaskState = 0xFF;
		ActorTaskSocket = -1;

		SpeakModelPath = "";
		PWGModelPath = "";
		MouthModelPath = "";
		FaceModelPath = "";
	}

	void copydata(const _actortaskinfo& item)
	{
		ActorPriority = item.ActorPriority;
		ActorCreateTime = item.ActorCreateTime;
		ActorMessageID = item.ActorMessageID;
		ActorTaskID = item.ActorTaskID;
		ActorTaskType = item.ActorTaskType;
		ActorMoodType = item.ActorMoodType;
		ActorTaskSpeed = item.ActorTaskSpeed;
		ActorTaskName = item.ActorTaskName;
		ActorTaskText = item.ActorTaskText;
		ActorTaskAudio = item.ActorTaskAudio;
		ActorTaskHumanID = item.ActorTaskHumanID;
		ActorTaskState = item.ActorTaskState;
		ActorTaskSocket = item.ActorTaskSocket;

		SpeakModelPath = item.SpeakModelPath;
		PWGModelPath = item.PWGModelPath;
		MouthModelPath = item.MouthModelPath;
		FaceModelPath = item.FaceModelPath;
	}

} actortaskinfo, *pactortaskinfo;
typedef std::map<int, actortaskinfo> ACTORTASKINFO_MAP;
ACTORTASKINFO_MAP Container_actortaskinfo;
pthread_mutex_t mutex_actortaskinfo;// actortaskinfo互斥量

void delete_actortask(int taskid)
{
	pthread_mutex_lock(&mutex_actortaskinfo);
	ACTORTASKINFO_MAP::iterator itDeleteTask = Container_actortaskinfo.find(taskid);
	if (itDeleteTask != Container_actortaskinfo.end())
	{
		Container_actortaskinfo.erase(itDeleteTask);
	}
	pthread_mutex_unlock(&mutex_actortaskinfo);
}
void delete_actortask_invalid()
{
	pthread_mutex_lock(&mutex_actortaskinfo);
	ACTORTASKINFO_MAP::iterator itDeleteTask = Container_actortaskinfo.begin();
	for (itDeleteTask;itDeleteTask != Container_actortaskinfo.end(); ++itDeleteTask)
	{
		if (!digitalmysql::isexisttask_taskid(itDeleteTask->first))
			Container_actortaskinfo.erase(itDeleteTask);
	}
	pthread_mutex_unlock(&mutex_actortaskinfo);
}
void setstate_actortask(int taskid, int state)
{
	pthread_mutex_lock(&mutex_actortaskinfo);
	ACTORTASKINFO_MAP::iterator itUpdateTask = Container_actortaskinfo.find(taskid);
	if (itUpdateTask != Container_actortaskinfo.end())
	{
		if (itUpdateTask->second.ActorTaskState != state)
			itUpdateTask->second.ActorTaskState = state;
	}
	pthread_mutex_unlock(&mutex_actortaskinfo);
}
void setpriority_actortask(int taskid, int priority)
{
	pthread_mutex_lock(&mutex_actortaskinfo);
	ACTORTASKINFO_MAP::iterator itUpdateTask = Container_actortaskinfo.find(taskid);
	if (itUpdateTask != Container_actortaskinfo.end())
	{
		if (itUpdateTask->second.ActorPriority != priority)
			itUpdateTask->second.ActorPriority = priority;
	}
	pthread_mutex_unlock(&mutex_actortaskinfo);
}
void setsocketfd_actortask(int taskid, int socketfd)
{
	pthread_mutex_lock(&mutex_actortaskinfo);
	ACTORTASKINFO_MAP::iterator itUpdateTask = Container_actortaskinfo.find(taskid);
	if (itUpdateTask != Container_actortaskinfo.end())
	{
		if (itUpdateTask->second.ActorTaskSocket != socketfd)
			itUpdateTask->second.ActorTaskSocket = socketfd;
	}
	pthread_mutex_unlock(&mutex_actortaskinfo);
}

//从数据库获取新任务
bool getactortask_nextrun(actortaskinfo& actortaskitem)
{
	//设置任务扫描时间【时间+GUID锁定执行者】
	std::string scandtime = gettimetext_now();
	std::string scandkey = getguidtext();
	std::string scannedtime_text = scandtime + std::string("_") + scandkey;//[时间+GUID] 支持HttpServer并发扫描
	if (!digitalmysql::setscannedtime_nextrun(scannedtime_text))
		return false;

	//根据扫描时间获取任务进行执行
	bool result = false;
	digitalmysql::taskinfo next_taskitem;
	if (digitalmysql::gettaskinfo_nextrun(scannedtime_text, next_taskitem) && next_taskitem.taskid != 0)
	{
		actortaskitem.ActorPriority = next_taskitem.priority;
		actortaskitem.ActorCreateTime = gettimecount_now();
		actortaskitem.ActorMessageID = getguidtext();
		actortaskitem.ActorTaskID = next_taskitem.taskid;
		actortaskitem.ActorTaskType = next_taskitem.tasktype;
		actortaskitem.ActorMoodType = next_taskitem.moodtype;
		actortaskitem.ActorTaskSpeed = next_taskitem.speakspeed;
		actortaskitem.ActorTaskName = next_taskitem.taskname;
		actortaskitem.ActorTaskText = next_taskitem.ssmltext;
		actortaskitem.ActorTaskAudio = next_taskitem.audio_path;
		actortaskitem.ActorTaskHumanID = next_taskitem.humanid;
		actortaskitem.ActorTaskState = 0xFF;

		digitalmysql::humaninfo now_humanItem;
		if (digitalmysql::gethumaninfo(next_taskitem.humanid, now_humanItem))
		{
			actortaskitem.SpeakModelPath = now_humanItem.speakmodelpath;
			actortaskitem.PWGModelPath = now_humanItem.pwgmodelpath;
			actortaskitem.MouthModelPath = now_humanItem.mouthmodelfile;
			actortaskitem.FaceModelPath = now_humanItem.facemodelfile;
			result = true;
		}
	}

	return result;
}

//减少用户可用合成时间
bool updateuserremaintime_taskid(int taskid)
{
	bool result = false;
	//根据taskid获取任务信息
	digitalmysql::taskinfo find_taskitem;
	if (!digitalmysql::gettaskinfo(taskid, find_taskitem))
		return result;
	//音频任务不减少合成时间
	if (find_taskitem.tasktype == 0)
		return result;

	long long root_remaintime = 0;
	if (getremaintime_user(find_taskitem.belongid, root_remaintime))
	{
		if (root_remaintime > 0)
			root_remaintime -= (find_taskitem.video_length / (int)find_taskitem.video_fps);
		if (root_remaintime < 0) 
			root_remaintime = 0;

		setremaintime_user(find_taskitem.belongid, root_remaintime);
		result = true;
	}

	return result;
}

//合成消息 
std::string getNotifyMsg_ToActor(digitalmysql::taskinfo taskitem, digitalmysql::humaninfo taskhumanitem, actortaskinfo actortaskitem)
{
	double send_left = taskitem.front_left;
	double send_top = taskitem.front_top;
	double send_right = taskitem.front_right;
	double send_bottom = taskitem.front_bottom;
	std::string send_imagematting = taskhumanitem.imagematting; 

	int send_taskid = actortaskitem.ActorTaskID;
	int send_tasktype = actortaskitem.ActorTaskType;
	int send_moodtype = actortaskitem.ActorMoodType;
	double send_speakspeed = actortaskitem.ActorTaskSpeed;
	std::string send_tasktext = delay_beforetext + actortaskitem.ActorTaskText + delay_aftertext; 
	std::string send_taskaudio = actortaskitem.ActorTaskAudio;
	std::string send_taskvideo = "";//保留位置,用于生成服务添加原始视频
	int send_taskupload = (aws_enable) ? (1) : (0);//用于判断是否需要上传成品文件到云盘

	//动作列表
	std::string send_actionidlist = "";
	std::vector<int> actionidlist;
	for (size_t i = 0; i < actionidlist.size(); i++)
	{
		std::string actioniditem; 
		char actioniditem_buff[1024] = { 0 };
		snprintf(actioniditem_buff, 1024, "{\"actionid\":%d}", actionidlist[i]);
		actioniditem = actioniditem_buff;

		send_actionidlist += actioniditem;
		if (i != actionidlist.size() - 1)
			send_actionidlist += ",";
	}

	std::string send_humanid = actortaskitem.ActorTaskHumanID;
	std::string send_speakmodel = actortaskitem.SpeakModelPath;
	std::string send_pwgmodel = actortaskitem.PWGModelPath;
	std::string send_mouthmodel = actortaskitem.MouthModelPath;
	std::string seng_facemodel = actortaskitem.FaceModelPath;

	std::string send_background = taskitem.background;
	std::string send_backaudio = taskitem.backaudio;
	double		send_backvolume = taskitem.back_volume;
	int			send_backloop = taskitem.back_loop;
	int			send_backstart = taskitem.back_start;
	int			send_backend = taskitem.back_end;

	int			send_wndwidth = taskitem.window_width;
	int			send_wndheight = taskitem.window_height;
	
	std::string send_messageid = actortaskitem.ActorMessageID;

	//json传递
	send_imagematting = str_replace(send_imagematting, "\"", "\\\""); //引号前加右斜杠
	send_tasktext = jsonstr_replace(send_tasktext); //文本内容替换,避免json格式错误

	//解码https地址参数[录制要求带中文的https路径,需转换为utf8再解码，再以ansi传递]
	ansi_to_utf8(send_background.c_str(), send_background.length(), send_background);
	send_background = url_decode(send_background);
	utf8_to_ansi(send_background.c_str(), send_background.length(), send_background);

	ansi_to_utf8(send_backaudio.c_str(), send_backaudio.length(), send_backaudio);
	send_backaudio = url_decode(send_backaudio);
	utf8_to_ansi(send_backaudio.c_str(), send_backaudio.length(), send_backaudio);

	//
	std::string result_msg = "";
	char msg_buff[BUFF_SZ] = { 0 };
	snprintf(msg_buff, BUFF_SZ,
		"{\"ddrinfo\":[{\"offset\":0,\"length\":0,\"pos\":{\"left\":%.6f,\"top\":%.6f,\"right\":%.6f,\"bottom\":%.6f},\"matte\":\"%s\"}],\
		\"makevideo\":{\"taskid\":%d,\"humanid\":\"%s\",\"tasktype\":%d,\"moodtype\":%d,\"speakspeed\":%.2f,\"tasktext\":\"%s\",\"taskaudio\":\"%s\",\"taskvideo\":\"%s\", \"taskupload\":%d,\"actionidlist\":[%s],\
		\"speakmodel\":\"%s\",\"pwgmodel\":\"%s\",\"mouthmodel\":\"%s\",\"facemodel\":\"%s\"},\
		\"background\":\"%s\",\
		\"backaudio\":{\"path\":\"%s\",\"volume\":%.6f,\"loop\":%d,\"start\":%d,\"end\":%d},\
		\"window\":{\"width\":%d,\"height\":%d},\
		\"msgid\":\"%s\"}",
		send_left, send_top, send_right, send_bottom, send_imagematting.c_str(),
		send_taskid, send_humanid.c_str(), send_tasktype, send_moodtype, send_speakspeed, send_tasktext.c_str(), send_taskaudio.c_str(),send_taskvideo.c_str(), send_taskupload, send_actionidlist.c_str(),
		send_speakmodel.c_str(), send_pwgmodel.c_str(), send_mouthmodel.c_str(), seng_facemodel.c_str(),
		send_background.c_str(), 
		send_backaudio.c_str(), send_backvolume, send_backloop, send_backstart, send_backend, 
		send_wndwidth, send_wndheight,
		send_messageid.c_str());
	result_msg = msg_buff;

	return result_msg;
}

//解析消息返回码
int getcode_recvmsg(std::string recv_message)
{
	int code = -1;
	if (recv_message.empty()) return code;

	json::Value recv_val = json::Deserialize((char*)recv_message.c_str());
	if (recv_val.GetType() == json::ObjectVal)
	{
		json::Object recv_obj = recv_val.ToObject();
		if (recv_obj.HasKey("code"))
		{
			code = recv_obj["code"].ToInt();
		}
	}

	return code;
}

//解析消息忙碌任务MessageID
std::string getbusyid_recvmsg(std::string recv_message)
{
	std::string busyid = "";
	if (recv_message.empty()) return busyid;

	json::Value recv_val = json::Deserialize((char*)recv_message.c_str());
	if (recv_val.GetType() == json::ObjectVal)
	{
		json::Object recv_obj = recv_val.ToObject();
		if (recv_obj.HasKey("busyid"))
		{
			busyid = recv_obj["busyid"].ToString();
		}
	}

	return busyid;
}

//任务串行执行
bool dispose_recvmsg_now(std::string recv_message, digitalmysql::taskinfo taskitem, std::string& json_result)
{
	bool result = false; 
	std::string errmsg = "success";
	std::string data = "";
	char data_buff[BUFF_SZ] = { 0 };
	snprintf(data_buff, BUFF_SZ, "\"TaskGroupID\":\"%s\",\"TaskID\":%d", taskitem.groupid.c_str(), taskitem.taskid); data = data_buff;
	
	int taskid = taskitem.taskid;
	json::Value recv_val = json::Deserialize((char*)recv_message.c_str());
	if (recv_val.GetType() == json::ObjectVal)
	{
		json::Object recv_obj = recv_val.ToObject();
		if (recv_obj.HasKey("code"))
		{
			int code = recv_obj["code"].ToInt();
			if (code == 0)
			{
				if (recv_obj.HasKey("audiopath"))
				{
					std::string audiopath_ansi = recv_obj["audiopath"].ToString();
					audiopath_ansi = str_replace(audiopath_ansi, std::string("\\"), std::string("/"));			//兼容共享路径
					digitalmysql::setaudiopath(taskid, audiopath_ansi);
				}
				if (recv_obj.HasKey("keyframe"))
				{
					std::string keyframepath_ansi = recv_obj["keyframe"].ToString();
					keyframepath_ansi = str_replace(keyframepath_ansi, std::string("\\"), std::string("/"));	//兼容共享路径
					digitalmysql::setkeyframepath(taskid, keyframepath_ansi);
				}
				if (recv_obj.HasKey("videopath"))
				{
					std::string videopath_ansi = recv_obj["videopath"].ToString();
					videopath_ansi = str_replace(videopath_ansi, std::string("\\"), std::string("/"));			//兼容共享路径
					digitalmysql::setvideopath(taskid, videopath_ansi);
				}
				if (recv_obj.HasKey("greenpath"))
				{
					std::string greenpath_ansi = recv_obj["greenpath"].ToString();
					greenpath_ansi = str_replace(greenpath_ansi, std::string("\\"), std::string("\\\\"));		//兼容共享路径
					if (is_existfile(greenpath_ansi.c_str()))
						remove(greenpath_ansi.c_str());//删除本地文件
				}
				digitalmysql::setmergestate(taskid, 1);		 //任务状态为成功
				digitalmysql::setmergeprogress(taskid, 100); //合成进度为100
				digitalmysql::setfinishtime(taskid, gettimetext_now());//更新任务完成时间
				result = true;
			}
			else
			{
				digitalmysql::setmergestate(taskid, 2);		 //任务状态为失败
				digitalmysql::setmergeprogress(taskid, 100); //合成进度为100
				digitalmysql::setfinishtime(taskid, gettimetext_now());//更新任务完成时间
			}
		}
	}

	//构造返回前端的json
	if (taskid != 0)
	{
		digitalmysql::taskinfo taskitem;
		if (digitalmysql::gettaskinfo(taskid, taskitem))
		{
			std::string data = ""; std::string keyvalue = "";
			char buff[BUFF_SZ] = { 0 };
			snprintf(buff, BUFF_SZ, "\"TaskGroupID\":\"%s\",\"TaskID\":%d,\"TaskName\":\"%s\",\"CreateTime\":\"%s\",", taskitem.groupid.c_str(), taskitem.taskid, taskitem.taskname.c_str(), taskitem.createtime.c_str()); keyvalue = keyvalue;
			data += buff;

			std::string audio_duration = gettimetext_millisecond(taskitem.audio_length);
			snprintf(buff, BUFF_SZ, "\"Audio\":{\"AudioFormat\":\"%s\",\"AudioFile\":\"%s\",\"Duration\":\"%s\"},", taskitem.audio_format.c_str(), taskitem.audio_path.c_str(), audio_duration.c_str()); keyvalue = buff;
			data += keyvalue;

			std::string video_duration = gettimetext_framecount(taskitem.video_length, taskitem.video_fps);
			snprintf(buff, BUFF_SZ, "\"Vedio\":{\"VideoFormat\":\"%s\",\"Width\":%d,\"Height\":%d,\"Fps\":%.2f,\"VedioFile\":\"%s\",\"Duration\":\"%s\"}", taskitem.video_format.c_str(), taskitem.video_width, taskitem.video_height, taskitem.video_fps, taskitem.video_path.c_str(), video_duration.c_str()); keyvalue = buff;//last no ","
			data += keyvalue;

			json_result = getjson_error(0, errmsg, data);

		}
		else
		{
			errmsg = "not found task in database...";
			json_result = getjson_error(1, errmsg, data);
		}	
	}
	else
	{
		errmsg = "recv message not found taskid...";
		json_result = getjson_error(1, errmsg, data);
	}

	return result;
}
bool getjson_runtask_now(actorinfo actoritem, actortaskinfo actortaskitem, int recv_timeout, std::string& json_result)
{
	bool result = false;
	std::string errmsg = "success";
	std::string data = "";

	int actor_type = actoritem.actortype;
	std::string actor_ip = actoritem.ip;
	short actor_port = actoritem.port;

	digitalmysql::taskinfo taskitem; digitalmysql::humaninfo taskhumanitem;
	if (digitalmysql::gettaskinfo(actortaskitem.ActorTaskID, taskitem) && digitalmysql::gethumaninfo(taskitem.humanid, taskhumanitem))
	{
		char data_buff[BUFF_SZ] = { 0 };
		snprintf(data_buff, BUFF_SZ, "\"TaskGroupID\":\"%s\",\"TaskID\":%d", taskitem.groupid.c_str(), taskitem.taskid); data = data_buff;

		std::string sendmsg = "", recvmsg = "";
		sendmsg = getNotifyMsg_ToActor(taskitem, taskhumanitem, actortaskitem);
		//send message[根据不同的actortype匹配不同的消息Header]
		int socketfd = -1;
		if (actor_type == 0)		//TTS+W2L
		{
			if (SendTcpMsg_PNPHDR(actor_ip, actor_port, sendmsg, recvmsg, socketfd))
			{
				_debug_to(0, ("[waiting...]: TaskID=%d, recv message: %s\n"), actortaskitem.ActorTaskID, recvmsg.c_str());
				int recvcode = getcode_recvmsg(recvmsg);
				if (recvcode == 1000 && socketfd != -1)
				{
					bool bRecvMessage = false;
					if (RecvTcpMsg_PNPHDR(socketfd, recvmsg, recv_timeout))//生成音频
					{
						bRecvMessage = true;
						_debug_to(0, ("[waiting...]: TaskID=%d, recv message: %s\n"), actortaskitem.ActorTaskID, recvmsg.c_str());
						int recvcode = getcode_recvmsg(recvmsg);
						switch (recvcode)
						{
							case  0://成功
							case  1://合成错误
							case  2://移动错误
							case  3://其他错误
							default:
							{
								result = dispose_recvmsg_now(recvmsg, taskitem, json_result);
								_debug_to(0, ("[waiting...]: TaskID=%d, dispose over[result=%d]...\n"), actortaskitem.ActorTaskID, result);	
							}
							break;
						}
					}

					//超时退出或其他错误【任务状态改为失败】
					if (bRecvMessage == false)
					{
						digitalmysql::setmergestate(actortaskitem.ActorTaskID, 2);		//任务状态为失败
						digitalmysql::setmergeprogress(actortaskitem.ActorTaskID, 100); //合成进度为100
						digitalmysql::setfinishtime(actortaskitem.ActorTaskID, gettimetext_now());//更新任务完成时间

						errmsg = "waiting recv actor message failed...";
						json_result = getjson_error(1, errmsg, data);
						_debug_to(0, ("[waiting...]: TaskID=%d, error message: %s\n"), actortaskitem.ActorTaskID, errmsg.c_str());
					}
					closesocket(socketfd);
				}
				else
				{
					errmsg = "actor is busy,please try next...";
					json_result = getjson_error(1, errmsg, data);
					_debug_to(0, ("[waiting...]: TaskID=%d, error message: %s\n"), actortaskitem.ActorTaskID, errmsg.c_str());
					closesocket(socketfd);
				}
			}
			else
			{
				//关闭socket
				if (socketfd != -1)
					closesocket(socketfd);

				//发送消息失败【任务失败】
				digitalmysql::setmergestate(actortaskitem.ActorTaskID, 2);		 //任务状态为失败
				digitalmysql::setmergeprogress(actortaskitem.ActorTaskID, 100); //合成进度为100
				digitalmysql::setfinishtime(actortaskitem.ActorTaskID, gettimetext_now());//更新任务完成时间

				//返回前端
				errmsg = "send message to actor failed...";
				json_result = getjson_error(1, errmsg, data);
				_debug_to(0, ("[waiting...]: TaskID=%d, error message: %s\n"), actortaskitem.ActorTaskID, errmsg.c_str());
			}
		}
		else if (actor_type == 1)	//TTS
		{
			if (SendTcpMsg_DGHDR(actor_ip, actor_port, sendmsg, recvmsg, socketfd))
			{
				_debug_to(0, ("[waiting...]: TaskID=%d, recv message: %s\n"), actortaskitem.ActorTaskID, recvmsg.c_str());
				int recvcode = getcode_recvmsg(recvmsg);
				if (recvcode == 1000 && socketfd != -1)
				{
					bool bRecvMessage = false;
					if (RecvTcpMsg_DGHDR(socketfd, recvmsg, recv_timeout))//生成音频
					{
						bRecvMessage = true;
						_debug_to(0, ("[waiting...]: TaskID=%d, recv message: %s\n"), actortaskitem.ActorTaskID, recvmsg.c_str());
						int recvcode = getcode_recvmsg(recvmsg);
						switch (recvcode)
						{
							case  0://成功
							case  1://合成错误
							case  2://移动错误
							case  3://其他错误
							default:
							{
								result = dispose_recvmsg_now(recvmsg, taskitem, json_result);
								_debug_to(0, ("[waiting...]: TaskID=%d, dispose over[result=%d]...\n"), actortaskitem.ActorTaskID, result);
							}
							break;
						}
					}

					//超时退出或其他错误【任务状态改为失败】
					if (bRecvMessage == false)
					{
						digitalmysql::setmergestate(actortaskitem.ActorTaskID, 2);		//任务状态为失败
						digitalmysql::setmergeprogress(actortaskitem.ActorTaskID, 100); //合成进度为100
						digitalmysql::setfinishtime(actortaskitem.ActorTaskID, gettimetext_now());//更新任务完成时间

						errmsg = "waiting recv actor message failed...";
						json_result = getjson_error(1, errmsg, data);
						_debug_to(0, ("[waiting...]: TaskID=%d, error message: %s\n"), actortaskitem.ActorTaskID, errmsg.c_str());
					}
					closesocket(socketfd);
				}
				else
				{
					errmsg = "actor is busy,please try next...";
					json_result = getjson_error(1, errmsg, data);
					_debug_to(0, ("[waiting...]: TaskID=%d, error message: %s\n"), actortaskitem.ActorTaskID, errmsg.c_str());
					closesocket(socketfd);
				}
			}
			else
			{
				//关闭socket
				if (socketfd != -1)
					closesocket(socketfd);

				//发送消息失败【任务失败】
				digitalmysql::setmergestate(actortaskitem.ActorTaskID, 2);		 //任务状态为失败
				digitalmysql::setmergeprogress(actortaskitem.ActorTaskID, 100); //合成进度为100
				digitalmysql::setfinishtime(actortaskitem.ActorTaskID, gettimetext_now());//更新任务完成时间

				//返回前端
				errmsg = "send message to actor failed...";
				json_result = getjson_error(1, errmsg, data);
				_debug_to(0, ("[waiting...]: TaskID=%d, error message: %s\n"), actortaskitem.ActorTaskID, errmsg.c_str());
			}
		}
		else
		{
			errmsg = "actoritem info error...";
			json_result = getjson_error(1, errmsg, data);
			_debug_to(0, ("[waiting...]: TaskID=%d, error message: %s\n"), actortaskitem.ActorTaskID, errmsg.c_str());
		}
	}
	else
	{
		errmsg = "not found task in database...";
		json_result = getjson_error(1, errmsg, data);
		_debug_to(0, ("[waiting...]: TaskID=%d, error message: %s\n"), actortaskitem.ActorTaskID, errmsg.c_str());
	}

	if (result == false)//处理结果为失败【任务失败】
	{
		digitalmysql::setmergestate(actortaskitem.ActorTaskID, 2);		 //任务状态为失败
		digitalmysql::setmergeprogress(actortaskitem.ActorTaskID, 100); //合成进度为100
		digitalmysql::setfinishtime(actortaskitem.ActorTaskID, gettimetext_now());//更新任务完成时间
	}
	return result;
}
//任务并行执行(发送消息+接收消息)
bool dispose_recvmsg_recv(std::string recv_message, int taskid)
{
	bool result = false; 
	json::Value recv_val = json::Deserialize((char*)recv_message.c_str());
	if (recv_val.GetType() == json::ObjectVal)
	{
		json::Object recv_obj = recv_val.ToObject();
		if (recv_obj.HasKey("code"))
		{
			int code = recv_obj["code"].ToInt();
			if (code == 0)
			{
				if (recv_obj.HasKey("audiopath"))
				{
					std::string audiopath_ansi = recv_obj["audiopath"].ToString();
					audiopath_ansi = str_replace(audiopath_ansi, std::string("\\"), std::string("/"));			//兼容共享路径
					digitalmysql::setaudiopath(taskid, audiopath_ansi);
				}
				if (recv_obj.HasKey("keyframe"))
				{
					std::string keyframepath_ansi = recv_obj["keyframe"].ToString();
					keyframepath_ansi = str_replace(keyframepath_ansi, std::string("\\"), std::string("/"));	//兼容共享路径
					digitalmysql::setkeyframepath(taskid, keyframepath_ansi);
				}
				if (recv_obj.HasKey("videopath"))
				{
					std::string videopath_ansi = recv_obj["videopath"].ToString();
					videopath_ansi = str_replace(videopath_ansi, std::string("\\"), std::string("/"));			//兼容共享路径
					digitalmysql::setvideopath(taskid, videopath_ansi);
				}
				if (recv_obj.HasKey("greenpath"))
				{
					std::string greenpath_ansi = recv_obj["greenpath"].ToString();
					greenpath_ansi = str_replace(greenpath_ansi, std::string("\\"), std::string("\\\\"));		//兼容共享路径
					if (is_existfile(greenpath_ansi.c_str()))
						remove(greenpath_ansi.c_str());//删除本地文件
				}
				digitalmysql::setmergestate(taskid, 1);		 //任务状态为成功
				digitalmysql::setmergeprogress(taskid, 100); //合成进度为100
				digitalmysql::setfinishtime(taskid, gettimetext_now());//更新任务完成时间

				//调整用户可用生成时长
				updateuserremaintime_taskid(taskid);
			}
			else
			{
				digitalmysql::setmergestate(taskid, 2);		 //任务状态为失败
				digitalmysql::setmergeprogress(taskid, 100); //合成进度为100
				digitalmysql::setfinishtime(taskid, gettimetext_now());//更新任务完成时间
			}
			result = true;
		}
	}

	return result;
}
//接收线程
void* pthread_recvtask_thread(void* arg)
{
	int ActorTaskID = *(int*)arg;

	//找到任务Item
	bool bFindTask = false;
	actortaskinfo find_actortaskitem;
	ACTORTASKINFO_MAP::iterator itFindTask = Container_actortaskinfo.find(ActorTaskID);
	if (itFindTask != Container_actortaskinfo.end())
	{
		if (itFindTask->second.ActorTaskState == 0)//任务在执行中
		{
			bFindTask = true;
			find_actortaskitem.copydata(itFindTask->second);
		}
	}
	if (bFindTask && find_actortaskitem.ActorTaskSocket == -1)
	{
		bFindTask = false;
		delete_actortask(find_actortaskitem.ActorTaskID);
		_debug_to(1, ("[recving...]: TaskID=%d, socket invalid...\n"), find_actortaskitem.ActorTaskID);
	}

	//接收消息
	bool bRecvMessage = false;
	if (bFindTask)
	{
		long long dwS = gettimecount_now();
		_debug_to(0, ("[recving...]: TaskID=%d, start recving...\n"), find_actortaskitem.ActorTaskID);
		std::string recvmsg = "";
		if (RecvTcpMsg_PNPHDR(find_actortaskitem.ActorTaskSocket, recvmsg, RUNTASK_TIMEOUT))//生成视频
		{
			bRecvMessage = true;
			_debug_to(0, ("[recving...]: TaskID=%d, recv message: %s\n"), find_actortaskitem.ActorTaskID, recvmsg.c_str());
			int recvcode = getcode_recvmsg(recvmsg);
			switch (recvcode)
			{
				case  0://成功
				case  1://合成错误
				case  2://移动错误
				case  3://其他错误
				default:
				{
					bool ret_dispose = dispose_recvmsg_recv(recvmsg, find_actortaskitem.ActorTaskID);
					_debug_to(0, ("[recving...]: TaskID=%d, dispose over[result=%d]...\n"), find_actortaskitem.ActorTaskID, ret_dispose);
				}
				break;
			}
		}
		long long dwE = gettimecount_now();
		closesocket(find_actortaskitem.ActorTaskSocket);
		delete_actortask(find_actortaskitem.ActorTaskID);
		_debug_to(0, ("[recving...]: TaskID=%d, end...[%lld] s\n"), find_actortaskitem.ActorTaskID, dwE - dwS);
	}
	//超时退出或其他错误【任务失败】
	if (bFindTask == false || bRecvMessage == false)
	{
		digitalmysql::setmergestate(find_actortaskitem.ActorTaskID, 2);		 //任务状态为失败
		digitalmysql::setmergeprogress(find_actortaskitem.ActorTaskID, 100); //合成进度为100
		digitalmysql::setfinishtime(find_actortaskitem.ActorTaskID, gettimetext_now());//更新任务完成时间
	}
	//通知前端
	std::string htmlnotifymsg = getNotifyMsg_ToHtml(find_actortaskitem.ActorTaskID);
	bool notifyresult = sendRabbitmqMsg(htmlnotifymsg);
	std::string msgresult = (notifyresult) ? ("success") : ("failed");
	_debug_to(0, ("[recving...]: Send HTML Notify[%s]: %s\n"), msgresult.c_str(), htmlnotifymsg.c_str());

	_debug_to(0, ("[recving...]: TaskID=%d, pthread_recvtask_thread exit...\n"), find_actortaskitem.ActorTaskID);
	return nullptr;
}
//发送线程
pthread_t threadid_sendtask_thread;
void* pthread_sendtask_thread(void* arg)
{
	int runtime = 0;
	while (true)
	{
		//内存中删除被用户删除的任务[优化]
		if ((runtime++) > 3600)//每小时检查一次
		{
			runtime = 0;
			delete_actortask_invalid();
		}

		bool bFindTask = false;
		actortaskinfo find_actortaskitem;
	    //从数据库获取下一条任务
		if (getactortask_nextrun(find_actortaskitem))
		{
			bFindTask = true;
			pthread_mutex_lock(&mutex_actortaskinfo);
			Container_actortaskinfo.insert(std::make_pair(find_actortaskitem.ActorTaskID, find_actortaskitem));
			pthread_mutex_unlock(&mutex_actortaskinfo);
			_debug_to(0, ("[sending...]: TaskID=%d, found task in mysql success\n"), find_actortaskitem.ActorTaskID);
		}

		//发送消息
		if (bFindTask)
		{
			bool bSuccessAssign = false; //是否成功分配到Actor
			size_t nFailedAssignCnt = 0; //统计失败的Actor个数
			ACTORINFO_MAP::iterator itFindActor = Container_TTSW2LActor.begin();
			for (itFindActor; itFindActor != Container_TTSW2LActor.end(); ++itFindActor)
			{
				std::string find_actorip = itFindActor->second.ip;
				short find_actorport = itFindActor->second.port;

				//尝试发送消息
				digitalmysql::taskinfo taskitem; digitalmysql::humaninfo taskhumanitem;
				if (digitalmysql::gettaskinfo(find_actortaskitem.ActorTaskID, taskitem) && digitalmysql::gethumaninfo(taskitem.humanid, taskhumanitem))
				{
					std::string sendmsg = "", recvmsg = "";
					sendmsg = getNotifyMsg_ToActor(taskitem, taskhumanitem, find_actortaskitem);
					if (SendTcpMsg_PNPHDR(find_actorip, find_actorport, sendmsg, recvmsg, find_actortaskitem.ActorTaskSocket))
					{
						_debug_to(0, ("[sending...]: TaskID=%d, recv message: %s\n"), find_actortaskitem.ActorTaskID, recvmsg.c_str());
						int recvcode = getcode_recvmsg(recvmsg);
						switch (recvcode)
						{
							case  0://非预期的返回值【音频并行合成处理】
							{
								bool ret_dispose = dispose_recvmsg_recv(recvmsg, find_actortaskitem.ActorTaskID);
								_debug_to(0, ("[sending...]: TaskID=%d, dispose over[result=%d]...\n"), find_actortaskitem.ActorTaskID, ret_dispose);
								
								//通知前端
								std::string htmlnotifymsg = getNotifyMsg_ToHtml(find_actortaskitem.ActorTaskID);
								bool notifyresult = sendRabbitmqMsg(htmlnotifymsg);
								std::string msgresult = (notifyresult) ? ("success") : ("failed");
								_debug_to(0, ("[sending...]: Send HTML Notify[%s]: %s\n"), msgresult.c_str(), htmlnotifymsg.c_str());

								//清理内存数据
								bSuccessAssign = true;//分配成功[异常情况]
								delete_actortask(find_actortaskitem.ActorTaskID);
								_debug_to(0, ("[sending...]: TaskID=%d, recv exception...\n"), find_actortaskitem.ActorTaskID);

								closesocket(find_actortaskitem.ActorTaskSocket);
								break;
							}
							case  1://配置错误
							case  2://配置错误
							{
								nFailedAssignCnt++;
								break;
							}
							case  3://忙碌
							{
								//根据busyid删除指定任务,避免多余重试
								std::string now_busyid = getbusyid_recvmsg(recvmsg);
								ACTORTASKINFO_MAP::iterator itBusyTask = Container_actortaskinfo.begin();
								for (itBusyTask; itBusyTask != Container_actortaskinfo.end(); ++itBusyTask)
								{
									if (itBusyTask->second.ActorMessageID == now_busyid)
									{
										setstate_actortask(itBusyTask->second.ActorTaskID, 0);//更新状态
									}
								}
								closesocket(find_actortaskitem.ActorTaskSocket);
								break;
							}
							case  1000://正常[已进入合成视频流程]
							{
								//更改数据库任务状态
								digitalmysql::setmergestate(find_actortaskitem.ActorTaskID, 0);		//任务状态为进行中
								digitalmysql::setmergeprogress(find_actortaskitem.ActorTaskID, 0);	//合成进度为0
								//通知前端
								std::string htmlnotifymsg = getNotifyMsg_ToHtml(find_actortaskitem.ActorTaskID);
								bool notifyresult = sendRabbitmqMsg(htmlnotifymsg);
								std::string msgresult = (notifyresult) ? ("success") : ("failed");
								_debug_to(0, ("[sending...]: Send HTML Notify[%s]: %s\n"), msgresult.c_str(), htmlnotifymsg.c_str());

								//开启循环监听消息返回线程【因合成Actor数量较少，不用考虑监听线程过多的问题】
								pthread_t threadid_recvtask_thread;
								int ret_startrecv = pthread_create(&threadid_recvtask_thread, nullptr, pthread_recvtask_thread, &find_actortaskitem.ActorTaskID);
								if (ret_startrecv == 0)
								{
									bSuccessAssign = true;//分配成功[正常情况]
									setstate_actortask(find_actortaskitem.ActorTaskID, 0);//更新状态
									setsocketfd_actortask(find_actortaskitem.ActorTaskID, find_actortaskitem.ActorTaskSocket);//保留socket
									_debug_to(0, ("[sending...]: TaskID=%d, wait recv message...\n"), find_actortaskitem.ActorTaskID);
								}
								pthread_detach(threadid_recvtask_thread);
								break;
							}
							default: //网络原因导致未收到消息
							{
								closesocket(find_actortaskitem.ActorTaskSocket);
								break;
							}
						}
					}
					else
					{
						//关闭socket
						if (find_actortaskitem.ActorTaskSocket != -1)
							closesocket(find_actortaskitem.ActorTaskSocket);

						//发送消息失败【任务失败】
						digitalmysql::setmergestate(find_actortaskitem.ActorTaskID, 2);		 //任务状态为失败
						digitalmysql::setmergeprogress(find_actortaskitem.ActorTaskID, 100); //合成进度为100
						digitalmysql::setfinishtime(find_actortaskitem.ActorTaskID, gettimetext_now());//更新任务完成时间
					}
				}

				//所有Actor都返回失败
				if (nFailedAssignCnt >= Container_TTSW2LActor.size())
				{
					bSuccessAssign = true;//分配成功[异常情况]
					delete_actortask(find_actortaskitem.ActorTaskID);
					_debug_to(0, ("[sending...]: TaskID=%d, assign exception...\n"), find_actortaskitem.ActorTaskID);
				}
				
				//不再向下一个Actor发送任务
				if (bSuccessAssign)
					break;
			}

			//所有Actor处于忙碌状态
			if (bSuccessAssign == false)
				digitalmysql::clearscannedtime_id(find_actortaskitem.ActorTaskID);//任务可再被扫描【重要】
		}

		sleep(1000);//1秒
		//pthread_exit(nullptr);//中途退出当前线程
	}

	_debug_to(0, ("[sending...]: pthread_sendtask_thread exit...\n"));
	return nullptr;
}

//定时减少数字人时长[每天04:00执行]
pthread_t threadid_sethumanremain_thread;
void* pthread_sethumanremain_thread(void* arg)
{
	std::string last_monthday = "";
	while (true)
	{
		std::string str_timenumber = gettime_custom();
		if (str_timenumber.length() == 14)
		{
			std::string str_monthday = str_timenumber.substr(4, 4);//2023[0922]0430
			std::string str_hour = str_timenumber.substr(8, 2);//20230922[04]30
			if ((str_monthday.compare(last_monthday)!=0) && (str_hour.compare("04")==0))
			{
				last_monthday = str_monthday;
				std::vector<int> vecHumanIndexs;
				bool result = digitalmysql::gethumanid_all(vecHumanIndexs);//获取数字人ID而不是HumanID,HumanID可能重复
				if (result)
				{
					for (size_t i = 0; i < vecHumanIndexs.size(); i++)
					{
						long long remaintime = 0;
						if (digitalmysql::gethumaninfo_remaintime(vecHumanIndexs[i], remaintime))
						{
							remaintime -= 86400;//减去一天的时间
							if (remaintime < 0) remaintime = 0;
							digitalmysql::sethumaninfo_remaintime(vecHumanIndexs[i], remaintime);
						}
					}
				}
			}
		}
		sleep(3000*1000);//3000秒[检测当前时间]
	}

	_debug_to(0, ("pthread_sethumanremain_thread exit...\n"));
	return nullptr;
}

#endif

#if 1 //多线程HttpServer

//HTTP命令支持跨域访问
bool checkOptionsRequest(struct evhttp_request* req)
{
	try
	{
		if (req->type == EVHTTP_REQ_OPTIONS) //为支持跨域访问，需要对OPTIONS进行快速处理
		{
			evkeyvalq* header = evhttp_request_get_output_headers(req);
			if (header)
			{
				evhttp_add_header(header, "Access-Control-Allow-Origin", "*");
				evhttp_add_header(header, "Access-Control-Allow-Methods", "GET,PUT,POST,HEAD,OPTIONS,DELETE,PATCH");
				evhttp_add_header(header, "Access-Control-Allow-Headers", "Content-Type,Content-Length,Authorization,Accept,X-Requested-With");
				evhttp_add_header(header, "Access-Control-Max-Age", "3600");
			}
			evhttp_send_reply(req, HTTP_OK, nullptr, nullptr);
			return true;
		}
	}
	catch (...)
	{
		;
	}
	return false;
}

//解析Json参数
void ParseBodyStr(json::Object json_obj, std::map<std::string, std::string>& mapStrParameter, std::map<std::string, int>& mapIntParameter, std::map<std::string, double>& mapDoubleParameter)
{
	json::Object::ValueMap::iterator it = json_obj.begin();
	for (it; it != json_obj.end(); ++it)
	{
		std::string key = it->first;
		if (it->second.GetType() == json::StringVal)
		{
			std::string value = it->second.ToString();
			mapStrParameter[key] = value;
		}
		if (it->second.GetType() == json::IntVal)
		{
			int value = it->second.ToInt();
			mapIntParameter[key] = value;
		}
		if (it->second.GetType() == json::DoubleVal)
		{
			double value = it->second.ToDouble();
			mapDoubleParameter[key] = value;
		}
		if (it->second.GetType() == json::ObjectVal)
		{
			json::Object json_subobj = it->second.ToObject();
			ParseBodyStr(json_subobj, mapStrParameter, mapIntParameter, mapDoubleParameter);
		}
		if (it->second.GetType() == json::ArrayVal)
		{
			json::Array json_subarray = it->second.ToArray();
			for (size_t j = 0; j < json_subarray.size(); ++j)
			{
				if (json_subarray[j].GetType() == json::ObjectVal)
				{
					json::Object json_subarray_obj = json_subarray[j].ToObject();
					ParseBodyStr(json_subarray_obj, mapStrParameter, mapIntParameter, mapDoubleParameter);
				}
			}
		}
	}
}

//返回内容构造函数
typedef struct _replyinfo
{
	int			http_code;
	int			reply_type;//0=字符串json返回,1=二进制数据返回,2=重定向返回
	//for string
	std::string content_string;
	//for data
	std::string content_type;
	char*		content_databuff; 
	long		content_datalen;
	//for callback
	std::string allow_credentials;
	std::string allow_origin;
	std::string redirect_url;
	std::string cookie_value;

	_replyinfo()
	{
		http_code = HTTP_OK;
		reply_type = 0;
		content_string = "unsupported request";
		content_type = "";
		content_databuff = nullptr;
		content_datalen = 0;
		allow_credentials = "true";
		allow_origin = "*";
		redirect_url = "";
	}

}replyinfo;
void global_http_reply(evhttp_request* req, replyinfo replyitem)
{
	try
	{
		evkeyvalq* out_header = evhttp_request_get_output_headers(req);
		if (!out_header)
		{
			_debug_to(1, ("http server: reply get header error...\n"));
			return;
		}

		std::string content_string_utf8 = replyitem.content_string;
#if defined WIN32//这里进行了修改，只有WINDOWS下需要转化，linux下直接认为是UTF8编码
		ansi_to_utf8(replyitem.content_string.c_str(), replyitem.content_string.length(), content_string_utf8);
#else
		content_string_utf8 = replyitem.content_string;
#endif

		//
		if (replyitem.reply_type == 0)
		{
			evhttp_add_header(out_header, "Access-Control-Allow-Origin", replyitem.allow_origin.c_str());
			evhttp_add_header(out_header, "Access-Control-Allow-Credentials", replyitem.allow_credentials.c_str());
			evhttp_add_header(out_header, "Connection", "keep-alive");
			evhttp_add_header(out_header, "Access-Control-Allow-Methods", "GET,PUT,POST,HEAD,OPTIONS,DELETE,PATCH");
			evhttp_add_header(out_header, "Access-Control-Allow-Headers", "Content-Type,Content-Length,Authorization,Accept,X-Requested-With");
			evhttp_add_header(out_header, "Access-Control-Max-Age", "3600");
			evhttp_add_header(out_header, "Content-Type", "application/json; charset=UTF-8");
			struct evbuffer* out_buffer = evbuffer_new();
			if (out_buffer)
			{
				evbuffer_add_printf(out_buffer, ("%s\n"), content_string_utf8.c_str());//返回的内容
				evhttp_send_reply(req, replyitem.http_code, "proxy", out_buffer);
				evbuffer_free(out_buffer);
				_debug_to(0, ("http server: reply return string [%s]\n"), content_string_utf8.c_str());
			}
			else
			{
				evhttp_send_reply(req, HTTP_NOTFOUND, nullptr, nullptr);
				_debug_to(1, ("http server: reply return string error...\n"));
			}
		}
		if (replyitem.reply_type == 1)
		{
			evhttp_add_header(out_header, "Access-Control-Allow-Origin", replyitem.allow_origin.c_str());
			evhttp_add_header(out_header, "Access-Control-Allow-Credentials", replyitem.allow_credentials.c_str());
			evhttp_add_header(out_header, "Connection", "keep-alive");
			evhttp_add_header(out_header, "Access-Control-Allow-Methods", "GET,PUT,POST,HEAD,OPTIONS,DELETE,PATCH");
			evhttp_add_header(out_header, "Access-Control-Allow-Headers", "Content-Type,Content-Length,Authorization,Accept,X-Requested-With");
			evhttp_add_header(out_header, "Access-Control-Max-Age", "3600");
			evhttp_add_header(out_header, "Content-Type", replyitem.content_type.c_str());
			evbuffer* out_buffer = evhttp_request_get_output_buffer(req); //返回的body
			if (replyitem.content_databuff && replyitem.content_datalen)
			{
				evbuffer_add(out_buffer, replyitem.content_databuff, replyitem.content_datalen);
				evhttp_send_reply(req, replyitem.http_code, "proxy", out_buffer);
				free(replyitem.content_databuff);
				_debug_to(0, ("http server: reply return data [%s]\n"), content_string_utf8.c_str());
			}
			else
			{
				evhttp_send_reply(req, HTTP_NOTFOUND, nullptr, nullptr);
				_debug_to(1, ("http server: reply return data error...\n"));
			}
		}
		if (replyitem.reply_type == 2)
		{
			evhttp_add_header(out_header, "Access-Control-Allow-Origin", replyitem.allow_origin.c_str());
			evhttp_add_header(out_header, "Access-Control-Allow-Credentials", replyitem.allow_credentials.c_str());
			evhttp_add_header(out_header, "Connection", "keep-alive");
			evhttp_add_header(out_header, "Access-Control-Allow-Methods", "GET,PUT,POST,HEAD,OPTIONS,DELETE,PATCH");
			evhttp_add_header(out_header, "Access-Control-Allow-Headers", "Content-Type,Content-Length,Authorization,Accept,X-Requested-With");
			evhttp_add_header(out_header, "Access-Control-Max-Age", "3600");
			evhttp_add_header(out_header, "Content-Type", "application/json; charset=UTF-8");
			evhttp_add_header(out_header, "Location", replyitem.redirect_url.c_str());
			evhttp_add_header(out_header, "Set-Cookie", replyitem.cookie_value.c_str());
			struct evbuffer* out_buffer = evbuffer_new();
			if (out_buffer)
			{
				evbuffer_add_printf(out_buffer, ("%s\n"), content_string_utf8.c_str());//返回的内容
				evhttp_send_reply(req, replyitem.http_code, "proxy", out_buffer);
				evbuffer_free(out_buffer);

				_debug_to(0, ("http server: reply return redirect [%s]\n"), content_string_utf8.c_str());
			}
			else
			{
				evhttp_send_reply(req, HTTP_NOTFOUND, nullptr, nullptr);
				_debug_to(1, ("http server: reply return string error...\n"));
			}
		}
	}
	catch (...)
	{
		_debug_to(1, ("http server: reply throw exception\n"));
	}
}

//HTTP服务回调函数,是在线程里回调的，注意线程安全
void global_http_generic_handler(struct evhttp_request* req, void* arg)
{
	if (checkOptionsRequest(req)) return;
	httpServer::httpThread* pServer = reinterpret_cast<httpServer::httpThread*>(arg);
	if (pServer == nullptr) return;
	int http_code = HTTP_OK;

	//Authorization
	std::string		   Authorization_token = "";
	authorizationinfo  req_authorizationitem;
	std::string		   req_authinfo_host = "";
	int				   req_authinfo_userid = -1;
	int				   req_authinfo_usertype = -1;
	std::string		   req_authinfo_usercode = "";
	std::string		   req_authinfo_username = "";
	std::string		   req_authinfo_password = "";
	long long		   req_authinfo_accesstime = 0;

	//reply
	replyinfo reply_item;
	reply_item.http_code = HTTP_OK;
	reply_item.reply_type = 0;
	reply_item.content_string = "unsupported request";
	reply_item.content_type = "";
	reply_item.content_databuff = nullptr;
	reply_item.content_datalen = 0;
	reply_item.allow_credentials = "true";
	reply_item.allow_origin = "*";
	reply_item.redirect_url = "";

	//debug outout
	std::string debug_str = ""; 
	bool debug_ret = true;

	//start
	long long start_time = gettimecount_now();
	try {
		char pathchar[MAX_PATH];
		char querychar[MAX_PATH];
		char hostchar[MAX_PATH];
		char urichar[MAX_PATH];
		memset(pathchar, 0, sizeof(char) * MAX_PATH);
		memset(querychar, 0, sizeof(char) * MAX_PATH);
		memset(hostchar, 0, sizeof(char) * MAX_PATH);
		memset(urichar, 0, sizeof(char) * MAX_PATH);

		const char* temppathchar = evhttp_uri_get_path(req->uri_elems);
		if (temppathchar)COMMON_STRCPY(pathchar, temppathchar, static_cast<ULONG>(MAX_PATH - 1));
		const char* tempquerychar = evhttp_uri_get_query(req->uri_elems);
		if (tempquerychar) COMMON_STRCPY(querychar, tempquerychar, static_cast<ULONG>(MAX_PATH - 1));
		if (req->remote_host) COMMON_STRCPY(hostchar, req->remote_host, static_cast<ULONG>(MAX_PATH - 1));
		if (req->input_headers) {
			const char* tempurichar = evhttp_find_header(req->input_headers, "Host");
			if (tempurichar) COMMON_STRCPY(urichar, tempurichar, static_cast<ULONG>(MAX_PATH - 1));

			//Authorization
			const char* temp_Authorization = evhttp_find_header(req->input_headers, "Authorization");
			if (temp_Authorization) Authorization_token = temp_Authorization;
		}

		struct evkeyvalq params;
		size_t querylen = 0U;
		char* querydecodechar = evhttp_uridecode(querychar, 0, &querylen);
		if (querydecodechar) {
			memset(querychar, 0, sizeof(char) * MAX_PATH);
			COMMON_STRCPY(querychar, querydecodechar, static_cast<ULONG>(MAX_PATH - 1));
			evhttp_parse_query_str(querydecodechar, &params);
			delete[]querydecodechar;
		}

		std::string bodyStr_ansi;
		std::string pathStr,queryStr, hostStr, uriStr;
		pathStr = pathchar;  //pathStr = UrlDecode(pathStr);  //网络路径字符串解码
		queryStr = querychar;
		hostStr = hostchar;
		uriStr = urichar;

		std::string bodyStr;// = post_data;
		char* post_data = reinterpret_cast<char*>(EVBUFFER_DATA(req->input_buffer));//(char *)EVBUFFER_DATA(req->input_buffer);//注：我们要求的是UTF-8封装的json格式
		size_t post_len = EVBUFFER_LENGTH(req->input_buffer);
		if (post_data && post_len > 0U)
		{
			if (str_existsubstr(pathStr, std::string("Add")))
			{
				bodyStr = "form-data request";
			}
			else if (req->body_size > 0U){// && bodyStr.length() >= req->body_size)
				char* tempchar = new char[req->body_size + 1U];
				memset(tempchar, 0, sizeof(char) * (req->body_size + 1U));
				//::CopyMemory(tempchar, post_data, req->body_size);
				memcpy(tempchar, post_data, req->body_size);
				bodyStr = tempchar;
				delete[]tempchar;
			}

#if defined WIN32
			utf8_to_ansi(bodyStr.c_str(), bodyStr.length(), bodyStr_ansi);
#else
			httpReqBodyStr_ansi = bodyStr;
#endif
		}

		std::string typeStr = "UNDEFINE";
		if (req->type == EVHTTP_REQ_GET)     typeStr = "GET";
		if (req->type == EVHTTP_REQ_POST)    typeStr = "POST";
		if (req->type == EVHTTP_REQ_HEAD)    typeStr = "HEAD";
		if (req->type == EVHTTP_REQ_PUT)	 typeStr = "PUT";
		if (req->type == EVHTTP_REQ_DELETE)  typeStr = "DELETE";
		if (req->type == EVHTTP_REQ_OPTIONS) typeStr = "OPTIONS";
		if (req->type == EVHTTP_REQ_TRACE)   typeStr = "TRACE";
		if (req->type == EVHTTP_REQ_CONNECT) typeStr = "CONNECT";
		if (req->type == EVHTTP_REQ_PATCH)   typeStr = "PATCH";
		_debug_to(0, ("http server receive %s[%d] from %s, path is %s, query param is %s, body is %s\n"), typeStr.c_str(), req->type, hostStr.c_str(), pathStr.c_str(), queryStr.c_str(), bodyStr.c_str());

		//解析路径层级
		std::vector<std::string> pathVector;
		globalSpliteString(pathStr, pathVector, ("/"));
//		std::vector<std::string>::iterator path_it = pathVector.begin();

		//解析路径中参数
		size_t tempPos;
		int overtime;
		std::vector<std::string> queryVector;
		globalSpliteString(queryStr, queryVector, ("&"));
		std::map<std::string, std::string> queryMap;
		std::string ParamName_utf8, ParamValue_utf8;
		for (std::vector<std::string>::iterator query_it = queryVector.begin(); query_it != queryVector.end(); query_it++)
		{
			tempPos = query_it->find(("="));
			if (tempPos == std::string::npos || tempPos == 0U) continue;
			ParamName_utf8 = query_it->substr(0U, tempPos);
			ParamValue_utf8 = query_it->substr(tempPos + 1U, query_it->length() - tempPos - 1U);

			//
			std::string ParamName_ansi; utf8_to_ansi(ParamName_utf8.c_str(), ParamName_utf8.length(), ParamName_ansi);
			std::string ParamValue_ansi; utf8_to_ansi(ParamValue_utf8.c_str(), ParamValue_utf8.length(), ParamValue_ansi);
			queryMap[ParamName_ansi] = ParamValue_ansi;
		}

		//解析Body参数
		std::map<std::string, std::string> mapBodyStrParameter;
		std::map<std::string, int> mapBodyIntParameter;
		std::map<std::string, double> mapBodyDoubleParameter;
		if (!bodyStr_ansi.empty())
		{
			json::Value body_val = json::Deserialize((char*)bodyStr_ansi.c_str());
			if (body_val.GetType() == json::ObjectVal)
			{
				json::Object body_obj = body_val.ToObject();
				ParseBodyStr(body_obj, mapBodyStrParameter, mapBodyIntParameter, mapBodyDoubleParameter);
			}
		}

#if 1 //解析认证信息
		if (str_existsubstr(pathStr, std::string("playout")) && !str_existsubstr(pathStr, std::string("Login")))//路径中含有playout且不含Login则需要验证token
		{
			//解析Token
			if (!token_decode(req_authorizationitem, Authorization_token))
			{
				debug_str = "{token decode failed...}";
				std::string errormsg = "token decode failed...";
				reply_item.http_code = HTTP_UNAUTHORIZED;
				reply_item.content_string = getjson_error(HTTP_UNAUTHORIZED, errormsg);
				goto http_reply;
			}
			//保存本次认证信息
			req_authinfo_host = req_authorizationitem.session;
			req_authinfo_userid = req_authorizationitem.userid;
			req_authinfo_usertype = req_authorizationitem.usertype;
			req_authinfo_usercode = req_authorizationitem.usercode;
			req_authinfo_username = req_authorizationitem.username;
			req_authinfo_password = req_authorizationitem.password;
			req_authinfo_accesstime = req_authorizationitem.accesstime;

			//检查会话是否过期
			std::string req_sessionid = md5::getStringMD5(req_authinfo_usercode);//md5加密usercode得到sessionid
			_debug_to(0, ("find session: sessionid=%s\n"), req_sessionid);
			sessioninfo find_sessionitem;
			if (!getsessioninfo(req_sessionid, find_sessionitem))
			{
				debug_str = "{session get from map failed...}";
				std::string errormsg = "session get from map failed...";
				reply_item.http_code = HTTP_UNAUTHORIZED;
				reply_item.content_string = getjson_error(HTTP_UNAUTHORIZED, errormsg);
				goto http_reply;
			}
			if (!checksession(req_sessionid))
			{
				debug_str = "{session check from map failed...}";
				std::string errormsg = "session check from map failed...";
				reply_item.http_code = HTTP_UNAUTHORIZED;
				reply_item.content_string = getjson_error(HTTP_UNAUTHORIZED, errormsg);
				goto http_reply;
			}
			//刷新会话
			if (gettimecount_now() > (req_authinfo_accesstime + SESSION_REFRESH_TIME))
			{
				//SSO时间戳刷新【保证SSO认证不过期】
				if (req_authinfo_usertype == 1)
				{
					ssoDataFrom::ssoTokenInfo refresh_ssoTokenItem;
					if (!ssoDataFrom::SSO_gettoken_refresh(url_gettoken, find_sessionitem.refresh_token, client_authorization, refresh_ssoTokenItem))
					{
						debug_str = "{session refresh token in sso center failed...}";
						std::string errormsg = "session refresh token in sso center failed...";
						reply_item.http_code = HTTP_UNAUTHORIZED;
						reply_item.content_string = getjson_error(HTTP_UNAUTHORIZED, errormsg);
						goto http_reply;
					}
					find_sessionitem.refresh_token = refresh_ssoTokenItem.refresh_token;
				}
				//本地时间戳刷新【保证本地token不过期】
				find_sessionitem.timeout_time = gettimecount_now() + SESSION_VALID_TIME;
				if (!addsessioninfo(find_sessionitem))
				{
					debug_str = "{session refresh token in map failed...}";
					std::string errormsg = "session refresh token in map failed...";
					reply_item.http_code = HTTP_UNAUTHORIZED;
					reply_item.content_string = getjson_error(HTTP_UNAUTHORIZED, errormsg);
					goto http_reply;
				}
				_debug_to(0, ("refresh session: sessionid=%s\n"), find_sessionitem.sessionid);
			}
		}
#endif
		
#if 1 //凌云部分	
		if (req->type == EVHTTP_REQ_POST)
		{
			if (pathStr.compare(("/action")) == 0 && queryStr.compare("action-id=open") == 0)
			{
				debug_str = "{/action?action-id=open}";
				//凌云-开通服务接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				std::string userCode = "";
				if (mapBodyStrParameter.find("userCode") != mapBodyStrParameter.end())
					userCode = mapBodyStrParameter["userCode"];
				CHECK_REQUEST_STR("userCode", userCode, errmsg, checkrequest);

				if (checkrequest)
				{
					if (digitalmysql::isexistuser_usercode(userCode))
						digitalmysql::setuserinfo_disable(userCode, false); //权限开启
					
					reply_item.content_string = getjson_linyun_error(true);
				}
				else
				{
					reply_item.content_string = getjson_linyun_error(false, errmsg);
					debug_ret = false;
				}
			}
			if (pathStr.compare(("/action")) == 0 && queryStr.compare("action-id=close") == 0)
			{
				debug_str = "{/action?action-id=close}";
				//凌云-关闭服务接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				std::string userCode = "";
				if (mapBodyStrParameter.find("userCode") != mapBodyStrParameter.end())
					userCode = mapBodyStrParameter["userCode"];
				CHECK_REQUEST_STR("userCode", userCode, errmsg, checkrequest);

				if (checkrequest)
				{
					if (digitalmysql::isexistuser_usercode(userCode))
						digitalmysql::setuserinfo_disable(userCode, true);//权限关闭

					reply_item.content_string = getjson_linyun_error(true);
				}
				else
				{
					reply_item.content_string = getjson_linyun_error(false, errmsg);
					debug_ret = false;
				}
			}
		}
		if (req->type == EVHTTP_REQ_GET)
		{
			if (pathStr.compare("/login/callback") == 0)
			{
				debug_str = "{/login/callback}";
				//凌云-登录回调接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				std::string LinYunCode = "";
				std::string LinYunState = "";
				if (queryMap.find("code") != queryMap.end())
					LinYunCode = queryMap["code"];
				CHECK_REQUEST_STR("LinYunCode", LinYunCode, errmsg, checkrequest);
				if (queryMap.find("state") != queryMap.end())
					LinYunState = queryMap["state"];
				CHECK_REQUEST_STR("LinYunState", LinYunState, errmsg, checkrequest);

				if (checkrequest)
				{
					//获取Token
					ssoDataFrom::ssoTokenInfo ssoTokenItem;
					if (ssoDataFrom::SSO_gettoken_bycode(url_gettoken, url_redirect, LinYunCode, client_authorization, ssoTokenItem))
					{
						//获取用户信息
						std::string token = ssoTokenItem.access_token;
						ssoDataFrom::ssoUserInfo ssoUserItem;
						if (ssoDataFrom::SSO_getuser_bytoken(url_getuser, token, ssoUserItem))
						{
							digitalmysql::userinfo new_useritem;
							new_useritem.id = ssoUserItem.id;
							new_useritem.disabled = ssoUserItem.disabled;
							new_useritem.usertype = 1;
							new_useritem.servicetype = 1;
							new_useritem.rootflag = ssoUserItem.rootflag;
							new_useritem.adminflag = 0;
							new_useritem.userCode = ssoUserItem.userCode;
							new_useritem.parentCode = ssoUserItem.parentCode;
							new_useritem.siteCode = ssoUserItem.siteCode;
							new_useritem.loginName = ssoUserItem.loginName;
							new_useritem.loginPassword = ssoUserItem.loginPassword;
							new_useritem.phone = ssoUserItem.phone;
							new_useritem.email = ssoUserItem.email;
							new_useritem.projectName = "";
							new_useritem.remainTime = 0;
							new_useritem.deadlineTime = "";

							bool existuser = digitalmysql::isexistuser_id(ssoUserItem.id);
							if (!existuser && !digitalmysql::adduserinfo(new_useritem, false))//不存在则保存用户信息
							{
								errmsg = "save userinfo from sso center failed...";
								reply_item.content_string = getjson_error(1, errmsg);
								debug_ret = false;
							}
							else
							{
								authorizationinfo login_authorizationitem;
								login_authorizationitem.session = LinYunState;
								login_authorizationitem.userid = new_useritem.id;
								login_authorizationitem.usertype = 1;
								login_authorizationitem.usercode = new_useritem.userCode;
								login_authorizationitem.username = new_useritem.loginName;
								login_authorizationitem.password = new_useritem.loginPassword;
								login_authorizationitem.refresh_token = ssoTokenItem.refresh_token;

								reply_item.allow_credentials = "true";  //!!!
								reply_item.allow_origin = "null";		//!!!
								reply_item.content_string = getjson_userlogin(login_authorizationitem);
							}
						}
						else
						{
							errmsg = "get userinfo from sso center failed...";
							reply_item.content_string = getjson_error(1, errmsg);
							debug_ret = false;
						}
					}
					else
					{
						errmsg = "get token from sso center failed...";
						reply_item.content_string = getjson_error(1, errmsg);
						debug_ret = false;
					}
				}
			}
		}
		if (req->type == EVHTTP_REQ_DELETE)
		{
			if (pathStr.compare("/loginout/callback") == 0)
			{
				debug_str = "{/loginout/callback}";
				//凌云-退出回调接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				std::string sobeycloud_user = "";
				const char* user_value = evhttp_find_header(req->input_headers, "sobeycloud-user");
				if (user_value)
					sobeycloud_user = user_value;
				std::string clear_sessionid = md5::getStringMD5(sobeycloud_user);//md5加密usercode得到sessionid
				clearsessioninfo(clear_sessionid);//清除对应的会话

				reply_item.content_string = getjson_error(0, errmsg);
			}
		}
		
#endif

#if 1 //数字人服务部分		
		if (req->type == EVHTTP_REQ_POST)
		{
			if (pathStr.compare(("/v1/videomaker/playout/Login")) == 0)
			{
				debug_str = "{/v1/videomaker/playout/Login}";
				//登录接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";
				std::string Session_State = getguidtext();//唯一标识此次会话

				//1
				int LoginType = -1;
				if (mapBodyIntParameter.find("LoginType") != mapBodyIntParameter.end())
					LoginType = mapBodyIntParameter["LoginType"];
				CHECK_REQUEST_NUM("LoginType", LoginType, errmsg, checkrequest);


				int			UserId = -1;
				std::string UserName = "";
				std::string PassWord = "";
				std::string Refresh_Token = "";
				if (LoginType == 0)//本地登录
				{
					if (mapBodyStrParameter.find("UserName") != mapBodyStrParameter.end())
						UserName = mapBodyStrParameter["UserName"];
					CHECK_REQUEST_STR("UserName", UserName, errmsg, checkrequest);

					if (mapBodyStrParameter.find("PassWord") != mapBodyStrParameter.end())
						PassWord = mapBodyStrParameter["PassWord"];
					CHECK_REQUEST_STR("PassWord", PassWord, errmsg, checkrequest);

					//2
					if (checkrequest)
					{
						//登录验证
						digitalmysql::userinfo check_useritem;
						if (digitalmysql::getuserinfo(UserName, PassWord, check_useritem))
						{
							//构造Token+添加会话
							authorizationinfo login_authorizationitem;
							login_authorizationitem.session  = Session_State;
							login_authorizationitem.userid   = check_useritem.id;
							login_authorizationitem.usertype = 0;
							login_authorizationitem.usercode = Session_State;//Session_State作为usercode【本地实现退出时，需修改此处为和用户绑定的唯一值】
							login_authorizationitem.username = UserName;
							login_authorizationitem.password = PassWord;
							login_authorizationitem.refresh_token = Refresh_Token;
							reply_item.content_string = getjson_userlogin(login_authorizationitem);
						}
						else
						{
							errmsg = "check userinfo failed,please check password...";
							reply_item.content_string = getjson_error(1, errmsg);
							debug_ret = false;
						}
					}
					else
					{
						reply_item.content_string = getjson_error(1, errmsg);
						debug_ret = false;
					}
				}
				if (LoginType == 1)//凌云登录
				{
					//检查是否开启凌云用户登录
					if (!sso_enable)
					{
						errmsg = "current ssoenable is false, please check config...";
						checkrequest = false;
					}

					//2
					if (checkrequest)
					{
						std::string Login_redirect_url = url_getcode;
						Login_redirect_url += std::string("?client_id=") + client_id;
						Login_redirect_url += std::string("&redirect_uri=") + url_redirect;
						Login_redirect_url += std::string("&response_type=code");
						Login_redirect_url += std::string("&state=") + Session_State;

						std::string Login_cookie_value = "No Cookie";
						const char* cookie_value = evhttp_find_header(req->input_headers, "Cookie");
						if (cookie_value)
							Login_cookie_value = cookie_value;

						//重定向到SSO获取Code
						reply_item.http_code = HTTP_MOVETEMP;
						reply_item.reply_type = 2;
						reply_item.allow_credentials = "true";	//!!!
						reply_item.allow_origin = url_homepage;	//!!!
						reply_item.redirect_url = Login_redirect_url;
						reply_item.cookie_value = Login_cookie_value;
					}
					else
					{
						reply_item.content_string = getjson_error(1, errmsg);
						debug_ret = false;
					}	
				}		
			}
			else if (pathStr.compare(("/v1/videomaker/playout/HumanList")) == 0)
			{
				debug_str = "{/v1/videomaker/playout/HumanList}";
				//获取数字人列表接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1
				std::string HumanID = "";
				if (mapBodyStrParameter.find("HumanID") != mapBodyStrParameter.end())
					HumanID = mapBodyStrParameter["HumanID"];

				//2
				if (checkrequest)
				{
					reply_item.content_string = getjson_humanlistinfo(HumanID, req_authinfo_userid);
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			else if (pathStr.compare(("/v1/videomaker/playout/ActionList")) == 0)
			{
				debug_str = "{/v1/videomaker/playout/ActionList}";
				//获取动作列表接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1
				std::string HumanID = "";
				if (mapBodyStrParameter.find("HumanID") != mapBodyStrParameter.end())
					HumanID = mapBodyStrParameter["HumanID"];
				CHECK_REQUEST_STR("HumanID", HumanID, errmsg, checkrequest);

				//2
				if (checkrequest)
				{
					reply_item.content_string = getjson_actionlistinfo(HumanID);
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			else if (pathStr.compare(("/v1/videomaker/playout/BackgroundList")) == 0)
			{
				debug_str = "{/v1/videomaker/playout/BackgroundList}";
				//获取背景资源接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1


				//2
				if (checkrequest)
				{
					reply_item.content_string = getjson_tasksourcelistinfo(req_authinfo_userid);
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			else if (pathStr.compare(("/v1/videomaker/playout/HumanHistory")) == 0)
			{
				debug_str = "{/v1/videomaker/playout/HumanHistory}";
				//获取稿件列表接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1
				digitalmysql::VEC_FILTERINFO vecfilterinfo;
				std::string Field1 = "", Value1 = "";
				if (mapBodyStrParameter.find("Field1") != mapBodyStrParameter.end())
					Field1 = mapBodyStrParameter["Field1"];
				if (mapBodyStrParameter.find("Value1") != mapBodyStrParameter.end())
					Value1 = mapBodyStrParameter["Value1"];
				digitalmysql::filterinfo filteritem1;
				filteritem1.filterfield = Field1; filteritem1.filtervalue = Value1;
				vecfilterinfo.push_back(filteritem1);

				std::string Field2 = "", Value2 = "";
				if (mapBodyStrParameter.find("Field2") != mapBodyStrParameter.end())
					Field2 = mapBodyStrParameter["Field2"];
				if (mapBodyStrParameter.find("Value2") != mapBodyStrParameter.end())
					Value2 = mapBodyStrParameter["Value2"];
				digitalmysql::filterinfo filteritem2;
				filteritem2.filterfield = Field2; filteritem2.filtervalue = Value2;
				vecfilterinfo.push_back(filteritem2);

				int PageSize = 10, PageNum = 1;
				if (mapBodyIntParameter.find("PageSize") != mapBodyIntParameter.end())
					PageSize = mapBodyIntParameter["PageSize"];
				if (mapBodyIntParameter.find("PageNum") != mapBodyIntParameter.end())
					PageNum = mapBodyIntParameter["PageNum"];

				std::string SortField = "createtime"; int SortValue = 1;
				if (mapBodyStrParameter.find("SortField") != mapBodyStrParameter.end())
					SortField = mapBodyStrParameter["SortField"];
				if (mapBodyIntParameter.find("SortValue") != mapBodyIntParameter.end())
					SortValue = mapBodyIntParameter["SortValue"];

				//2
				if (checkrequest)
				{
					reply_item.content_string = getjson_humanhistoryinfo(vecfilterinfo, SortField, SortValue, PageSize, PageNum, req_authinfo_userid);
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			else if (pathStr.compare(("/v1/videomaker/playout/GetVersionList")) == 0)
			{
				debug_str = "{/v1/videomaker/playout/GetVersionList}";
				//获取单个稿件所有版本接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1
				std::string TaskGroupID = "";
				if (mapBodyStrParameter.find("TaskGroupID") != mapBodyStrParameter.end())
					TaskGroupID = mapBodyStrParameter["TaskGroupID"];
				CHECK_REQUEST_STR("TaskGroupID", TaskGroupID, errmsg, checkrequest);

				if (checkrequest)
				{
					reply_item.content_string = getjson_taskgroupinfo(TaskGroupID);
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			else if (pathStr.compare(("/v1/videomaker/playout/UpdateLabel")) == 0)
			{
				debug_str = "{/v1/videomaker/playout/UpdateLabel}";
				//更新数字人标签接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1
				std::string HumanID = "", HumanLabel = "";
				if (mapBodyStrParameter.find("HumanID") != mapBodyStrParameter.end())
					HumanID = mapBodyStrParameter["HumanID"];
				CHECK_REQUEST_STR("HumanID", HumanID, errmsg, checkrequest);
				if (mapBodyStrParameter.find("HumanLabel") != mapBodyStrParameter.end())
					HumanLabel = mapBodyStrParameter["HumanLabel"];

				//2
				if (checkrequest)
				{
					if (setlabelstring_human(req_authinfo_userid, HumanID, HumanLabel))
					{
						reply_item.content_string = getjson_error(0, errmsg);
					}
					else
					{
						errmsg = "update label to mysql failed...";
						reply_item.content_string = getjson_error(1, errmsg);
						debug_ret = false;
					}
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			else if (pathStr.compare(("/v1/videomaker/playout/SaveTask")) == 0)
			{
				debug_str = "{/v1/videomaker/playout/SaveTask}";
				//合成数字人视频接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1=以下参数同VideoMake
				std::string TaskName = "";
				if (mapBodyStrParameter.find("TaskName") != mapBodyStrParameter.end())
					TaskName = mapBodyStrParameter["TaskName"];
				CHECK_REQUEST_STR("TaskName", TaskName, errmsg, checkrequest);
				std::string HumanID = "";
				if (mapBodyStrParameter.find("HumanID") != mapBodyStrParameter.end())
					HumanID = mapBodyStrParameter["HumanID"];
				CHECK_REQUEST_STR("HumanID", HumanID, errmsg, checkrequest);
				std::string InputSsml = "";
				if (mapBodyStrParameter.find("InputSsml") != mapBodyStrParameter.end())
					InputSsml = mapBodyStrParameter["InputSsml"];
				std::string InputAudio = "";
				if (mapBodyStrParameter.find("InputAudio") != mapBodyStrParameter.end())
					InputAudio = mapBodyStrParameter["InputAudio"];
				//
				double Front_left = 0.0, Front_right = 1.0, Front_top = 0.0, Front_bottom = 1.0;
				if (mapBodyDoubleParameter.find("left") != mapBodyDoubleParameter.end())
					Front_left = mapBodyDoubleParameter["left"];
				Front_left = (Front_left < 0.0) ? (0.0) : (Front_left);
				if (mapBodyDoubleParameter.find("right") != mapBodyDoubleParameter.end())
					Front_right = mapBodyDoubleParameter["right"];
				Front_right = (Front_right < 0.0) ? (0.0) : (Front_right);
				if (mapBodyDoubleParameter.find("top") != mapBodyDoubleParameter.end())
					Front_top = mapBodyDoubleParameter["top"];
				Front_top = (Front_top < 0.0) ? (0.0) : (Front_top);
				if (mapBodyDoubleParameter.find("bottom") != mapBodyDoubleParameter.end())
					Front_bottom = mapBodyDoubleParameter["bottom"];
				Front_bottom = (Front_bottom < 0.0) ? (0.0) : (Front_bottom);
				//
				std::string Foreground = "", Background = "";
				if (mapBodyStrParameter.find("Foreground") != mapBodyStrParameter.end())
					Foreground = mapBodyStrParameter["Foreground"];
				if (mapBodyStrParameter.find("Background") != mapBodyStrParameter.end())
					Background = mapBodyStrParameter["Background"];
				//
				std::string BackAudio = "";
				if (mapBodyStrParameter.find("BackAudio") != mapBodyStrParameter.end())
					BackAudio = mapBodyStrParameter["BackAudio"];
				int BackAudio_volume = 0, BackAudio_loop = 1, BackAudio_start = 0, BackAudio_end = 65535;
				if (mapBodyIntParameter.find("volume") != mapBodyIntParameter.end())
					BackAudio_volume = mapBodyIntParameter["volume"];
				if (mapBodyIntParameter.find("loop") != mapBodyIntParameter.end())
					BackAudio_loop = mapBodyIntParameter["loop"];
				if (mapBodyIntParameter.find("start") != mapBodyIntParameter.end())
					BackAudio_start = mapBodyIntParameter["start"];
				if (mapBodyIntParameter.find("end") != mapBodyIntParameter.end())
					BackAudio_end = mapBodyIntParameter["end"];
				int Window_Width = 1920, Window_Height = 1080;
				if (mapBodyIntParameter.find("WndWidth ") != mapBodyIntParameter.end())
					Window_Width = mapBodyIntParameter["WndWidth "];
				if (mapBodyIntParameter.find("WndHeight") != mapBodyIntParameter.end())
					Window_Height = mapBodyIntParameter["WndHeight"];
				//
				std::string TaskGroupID = "", TaskVersionName = ""; int TaskVersion = 0;
				if (mapBodyStrParameter.find("TaskGroupID") != mapBodyStrParameter.end())
					TaskGroupID = mapBodyStrParameter["TaskGroupID"];
				TaskGroupID = (TaskGroupID.empty()) ? (getguidtext()) : (TaskGroupID);
				if (mapBodyIntParameter.find("TaskVersion") != mapBodyIntParameter.end())
					TaskVersion = mapBodyIntParameter["TaskVersion"];
				if (mapBodyStrParameter.find("TaskVersionName") != mapBodyStrParameter.end())
					TaskVersionName = mapBodyStrParameter["TaskVersionName"];
				TaskVersionName = (TaskVersionName.empty()) ? (gettimetext_now()) : (TaskVersionName);
				int TaskID = 0; int TaskType = 1, Makesynch = 0, TaskMoodType = 0;
				if (mapBodyIntParameter.find("TaskID") != mapBodyIntParameter.end())
					TaskID = mapBodyIntParameter["TaskID"];
				if (mapBodyIntParameter.find("TaskType") != mapBodyIntParameter.end())
					TaskType = mapBodyIntParameter["TaskType"];
				if (mapBodyIntParameter.find("Makesynch") != mapBodyIntParameter.end())
					Makesynch = mapBodyIntParameter["Makesynch"];
				if (mapBodyIntParameter.find("TaskMoodType") != mapBodyIntParameter.end())
					TaskMoodType = mapBodyIntParameter["TaskMoodType"];
				double Speed = 1.0;
				if (mapBodyDoubleParameter.find("Speed") != mapBodyDoubleParameter.end())
					Speed = mapBodyDoubleParameter["Speed"];

				//根据SaveType类型,对参数做调整
				int SaveType = 0;
				if (mapBodyIntParameter.find("SaveType") != mapBodyIntParameter.end())
					SaveType = mapBodyIntParameter["SaveType"];
				bool exist_task = (digitalmysql::isexisttask_taskid(TaskID)) ? (true) : (false);
				if (SaveType == 0)//保存当前版本
				{
					if (!exist_task)
					{
						TaskVersion = 0;	//版本号改为0，表示第一个版本
						TaskID = 0;			//ID号改为0，执行新增任务流程
					}
				}
				else			  //另存新版本
				{
					TaskVersion = digitalmysql::gettasknextversion(TaskGroupID);//最大版本号+1
					TaskID = 0;				//ID号改为0，执行新增任务流程
				}

				//检查-检查任务是否在执行中
				if (checkrequest)
				{
					bool bActorRun = false; bool bMySqlRun = false;
					bMySqlRun = (digitalmysql::getmergestate(TaskID) == 0);
					ACTORTASKINFO_MAP::iterator itRunTask = Container_actortaskinfo.find(TaskID);
					if (itRunTask != Container_actortaskinfo.end())
						bActorRun = (itRunTask->second.ActorTaskState == 0);
					bool bTaskRun = (TaskType == 0) ? (bMySqlRun) : (bActorRun && bMySqlRun);//音频任务只看MySql状态
					if (bTaskRun)
					{
						errmsg = "task is running, please try again alter...";
						checkrequest = false;
					}
				}

				//2
				if (checkrequest)
				{
					//获取数字人信息
					std::string HumanName = "";
					std::string SpeakModelFullPath = "";	//"../ModelFile/test/TTS/speak/snapshot_iter_1668699.pdz";
					std::string PwgModelFullPath = "";		//"../ModelFile/test/TTS/pwg/snapshot_iter_1000000.pdz";
					std::string MouthModelsPath = "";		// "../ModelFile/test/W2L/file/shenzhen_v3_20230227.pth";
					std::string FaceModelsPath = "";		// "../ModelFile/test/W2L/file/shenzhen_v3_20230227.dfm";
					std::string HumanForeground = "";		//16:9
					std::string HumanForeground2 = "";		//9:16
					std::string HumanBackground = "";		//16:9
					std::string HumanBackground2 = "";		//9:16
					digitalmysql::humaninfo HumanItem;
					if (digitalmysql::gethumaninfo(HumanID, HumanItem))
					{
						HumanName = HumanItem.humanname;
						SpeakModelFullPath = HumanItem.speakmodelpath;
						PwgModelFullPath = HumanItem.pwgmodelpath;
						MouthModelsPath = HumanItem.mouthmodelfile;
						FaceModelsPath = HumanItem.facemodelfile;
						//优先赋值不为空的字段
						HumanForeground = (HumanItem.foreground.empty()) ? (HumanItem.foreground2) : (HumanItem.foreground);
						HumanForeground2 = (HumanItem.foreground2.empty()) ? (HumanItem.foreground) : (HumanItem.foreground2);
						HumanBackground = (HumanItem.background.empty()) ? (HumanItem.background2) : (HumanItem.background);
						HumanBackground2 = (HumanItem.background2.empty()) ? (HumanItem.background) : (HumanItem.background2);
						//前端传空值时优化处理
						if (Foreground.empty())
							Foreground = (Window_Width > Window_Height) ? (HumanForeground) : (HumanForeground2);
						if (Background.empty())
							Background = (Window_Width > Window_Height) ? (HumanBackground) : (HumanBackground2);
					}

					_debug_to(0, ("[SaveTask] BeforeInsert: TaskID=%d, update=%d\n"), TaskID, exist_task);
					//添加任务到数据库
					digitalmysql::taskinfo new_taskitem;
					new_taskitem.taskid = TaskID;
					new_taskitem.belongid = req_authinfo_userid;
					new_taskitem.privilege = 1;
					new_taskitem.groupid = TaskGroupID;
					new_taskitem.versionname = TaskVersionName;
					new_taskitem.version = TaskVersion;
					new_taskitem.tasktype = TaskType;
					new_taskitem.moodtype = TaskMoodType;
					new_taskitem.speakspeed = Speed;
					new_taskitem.taskname = TaskName;
					new_taskitem.taskstate = 0xFE;//已保存状态
					new_taskitem.taskprogress = 0;
					new_taskitem.createtime = gettimetext_now();
					new_taskitem.scannedtime = "";
					new_taskitem.finishtime = "";
					new_taskitem.priority = 0;
					new_taskitem.islastedit = 1;
					new_taskitem.humanid = HumanID;
					new_taskitem.humanname = HumanName;
					new_taskitem.ssmltext = InputSsml;
					new_taskitem.audio_path = "";
					new_taskitem.audio_format = "";
					new_taskitem.audio_length = 0;
					new_taskitem.video_path = "";
					new_taskitem.video_format = "";
					new_taskitem.video_keyframe = "";
					new_taskitem.video_length = 0;
					new_taskitem.video_width = 0;
					new_taskitem.video_height = 0;
					new_taskitem.video_fps = 0.0;
					new_taskitem.foreground = Foreground;
					new_taskitem.front_left = Front_left;
					new_taskitem.front_right = Front_right;
					new_taskitem.front_top = Front_top;
					new_taskitem.front_bottom = Front_bottom;
					new_taskitem.background = Background;
					new_taskitem.backaudio = BackAudio;
					new_taskitem.back_volume = (double)BackAudio_volume;
					new_taskitem.back_loop = BackAudio_loop;
					new_taskitem.back_start = BackAudio_start;
					new_taskitem.back_end = BackAudio_end;
					new_taskitem.window_width = Window_Width;
					new_taskitem.window_height = Window_Height;

					digitalmysql::addtaskinfo(TaskID, new_taskitem, exist_task);
					digitalmysql::setmergestate(TaskID, 0xFE);//任务状态为已保存
					digitalmysql::setmergeprogress(TaskID, 0);//合成进度为0
					_debug_to(0, ("[SaveTask] AfterInsert: TaskID=%d, update=%d\n"), TaskID, exist_task);

					//更新版本编辑状态
					digitalmysql::settasklastedit(TaskGroupID, TaskID);

					//
					char temp_buff[256] = { 0 };
					snprintf(temp_buff, 256, "\"TaskGroupID\":\"%s\",\"TaskID\":%d", TaskGroupID.c_str(), TaskID); data = temp_buff;
					reply_item.content_string = getjson_error(0, errmsg, data);
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			else if (pathStr.compare(("/v1/videomaker/playout/VideoMake")) == 0)
			{
				debug_str = "{/v1/videomaker/playout/VideoMake}";
				//合成数字人视频接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";
				bool new_audiotask = false;//此标志判断是否是新建的试听任务,如果是,失败后会删除音频任务

				//1
				std::string TaskName = "";
				if (mapBodyStrParameter.find("TaskName") != mapBodyStrParameter.end())
					TaskName = mapBodyStrParameter["TaskName"];
				CHECK_REQUEST_STR("TaskName", TaskName, errmsg, checkrequest);
				std::string HumanID = "";
				if (mapBodyStrParameter.find("HumanID") != mapBodyStrParameter.end())
					HumanID = mapBodyStrParameter["HumanID"];
				CHECK_REQUEST_STR("HumanID", HumanID, errmsg, checkrequest);
				std::string InputSsml = "";
				if (mapBodyStrParameter.find("InputSsml") != mapBodyStrParameter.end())
					InputSsml = mapBodyStrParameter["InputSsml"];
				std::string InputAudio = "";
				if (mapBodyStrParameter.find("InputAudio") != mapBodyStrParameter.end())
					InputAudio = mapBodyStrParameter["InputAudio"];
				//
				double Front_left = 0.0, Front_right = 1.0, Front_top = 0.0, Front_bottom = 1.0;
				if (mapBodyDoubleParameter.find("left") != mapBodyDoubleParameter.end())
					Front_left = mapBodyDoubleParameter["left"];
				Front_left = (Front_left < 0.0) ? (0.0) : (Front_left);
				if (mapBodyDoubleParameter.find("right") != mapBodyDoubleParameter.end())
					Front_right = mapBodyDoubleParameter["right"];
				Front_right = (Front_right < 0.0) ? (0.0) : (Front_right);
				if (mapBodyDoubleParameter.find("top") != mapBodyDoubleParameter.end())
					Front_top = mapBodyDoubleParameter["top"];
				Front_top = (Front_top < 0.0) ? (0.0) : (Front_top);
				if (mapBodyDoubleParameter.find("bottom") != mapBodyDoubleParameter.end())
					Front_bottom = mapBodyDoubleParameter["bottom"];
				Front_bottom = (Front_bottom < 0.0) ? (0.0) : (Front_bottom);
				//
				std::string Foreground = "", Background = "";
				if (mapBodyStrParameter.find("Foreground") != mapBodyStrParameter.end())
					Foreground = mapBodyStrParameter["Foreground"];
				if (mapBodyStrParameter.find("Background") != mapBodyStrParameter.end())
					Background = mapBodyStrParameter["Background"];
				//
				std::string BackAudio = "";
				if (mapBodyStrParameter.find("BackAudio") != mapBodyStrParameter.end())
					BackAudio = mapBodyStrParameter["BackAudio"];
				int BackAudio_volume = 0, BackAudio_loop = 1, BackAudio_start = 0, BackAudio_end = 65535;
				if (mapBodyIntParameter.find("volume") != mapBodyIntParameter.end())
					BackAudio_volume = mapBodyIntParameter["volume"];
				if (mapBodyIntParameter.find("loop") != mapBodyIntParameter.end())
					BackAudio_loop = mapBodyIntParameter["loop"];
				if (mapBodyIntParameter.find("start") != mapBodyIntParameter.end())
					BackAudio_start = mapBodyIntParameter["start"];
				if (mapBodyIntParameter.find("end") != mapBodyIntParameter.end())
					BackAudio_end = mapBodyIntParameter["end"];
				int Window_Width = 1920, Window_Height = 1080;
				if (mapBodyIntParameter.find("WndWidth") != mapBodyIntParameter.end())
					Window_Width = mapBodyIntParameter["WndWidth"];
				if (mapBodyIntParameter.find("WndHeight") != mapBodyIntParameter.end())
					Window_Height = mapBodyIntParameter["WndHeight"];
				//
				std::string TaskGroupID = "", TaskVersionName = ""; int TaskVersion = 0;
				if (mapBodyStrParameter.find("TaskGroupID") != mapBodyStrParameter.end())
					TaskGroupID = mapBodyStrParameter["TaskGroupID"];
				TaskGroupID = (TaskGroupID.empty()) ? (getguidtext()) : (TaskGroupID);
				if (mapBodyIntParameter.find("TaskVersion") != mapBodyIntParameter.end())
					TaskVersion = mapBodyIntParameter["TaskVersion"];
				if (mapBodyStrParameter.find("TaskVersionName") != mapBodyStrParameter.end())
					TaskVersionName = mapBodyStrParameter["TaskVersionName"];
				TaskVersionName = (TaskVersionName.empty()) ? (gettimetext_now()) : (TaskVersionName);
				int TaskID = 0; int TaskType = 1, Makesynch = 0, TaskMoodType = 0;
				if (mapBodyIntParameter.find("TaskID") != mapBodyIntParameter.end())
					TaskID = mapBodyIntParameter["TaskID"];
				if (mapBodyIntParameter.find("TaskType") != mapBodyIntParameter.end())
					TaskType = mapBodyIntParameter["TaskType"];
				if (mapBodyIntParameter.find("Makesynch") != mapBodyIntParameter.end())
					Makesynch = mapBodyIntParameter["Makesynch"];
				if (mapBodyIntParameter.find("TaskMoodType") != mapBodyIntParameter.end())
					TaskMoodType = mapBodyIntParameter["TaskMoodType"];
				double Speed = 1.0;
				if (mapBodyDoubleParameter.find("Speed") != mapBodyDoubleParameter.end())
					Speed = mapBodyDoubleParameter["Speed"];

				//检查-检查任务参数
				if (TaskType == 0 || TaskType == 1)
				{
					CHECK_REQUEST_STR("InputSsml", InputSsml, errmsg, checkrequest);
					InputAudio = "";

					//根据前端请求是否带TaskID判断是否是新建的试听任务
					new_audiotask = ((TaskID <= 0)&&(TaskType == 0)) ? (true) : (false);
				}
				else if (TaskType == 2)
				{
					CHECK_REQUEST_STR("InputAudio", InputAudio, errmsg, checkrequest);
					InputSsml = "";
				}
				//检查-检查对应数字人的available状态，为0表示正在训练中
				if (checkrequest && !digitalmysql::isavailable_humanid(HumanID))
				{
					if (HumanID.empty())
						errmsg = "the request humanid is empty...";//前端发起请求偶现
					else
						errmsg = "the digital man is in training...";
					checkrequest = false;
				}
				//检查-检查任务是否在执行中
				if (checkrequest)
				{
					bool bActorRun = false; bool bMySqlRun = false;
					bMySqlRun = (digitalmysql::getmergestate(TaskID) == 0);
					ACTORTASKINFO_MAP::iterator itRunTask = Container_actortaskinfo.find(TaskID);
					if (itRunTask != Container_actortaskinfo.end())
						bActorRun = (itRunTask->second.ActorTaskState == 0);
					bool bTaskRun = (TaskType == 0) ? (bMySqlRun) : (bActorRun && bMySqlRun);//音频任务只看MySql状态
					if (bTaskRun)
					{
						errmsg = "task is running, please try again alter...";
						checkrequest = false;
					}
				}
				//检查-检查用户可用时间是否充足
				if (checkrequest)
				{
					long long root_remaintime = 0;
					if (!getremaintime_user(req_authinfo_userid, root_remaintime))
					{
						errmsg = "get root account remaintime failed...";
						checkrequest = false;
					}
					else
					{
						if (root_remaintime <= 0)
						{
							errmsg = "root account remaintime not enough...";
							checkrequest = false;
						}
					}
				}
				//检查-检查模型可用时间是否充足
				if (checkrequest)
				{
					long long human_remaintime = 0;
					if (getremaintime_human(req_authinfo_userid, HumanID, human_remaintime))
					{
						if (human_remaintime <= 0)
						{
							errmsg = "current human remaintime not enough...";
							checkrequest = false;
						}
					}
					else
					{
						errmsg = "get current human remaintime failed...";
						checkrequest = false;
					}
				}

				//更新版本号
				bool exist_task = (digitalmysql::isexisttask_taskid(TaskID)) ? (true) : (false);
				if (!exist_task) TaskVersion = 0;

				//2
				if (checkrequest)
				{
					//获取数字人信息
					std::string HumanName = "";
					std::string SpeakModelFullPath = "";	//"../ModelFile/test/TTS/speak/snapshot_iter_1668699.pdz";
					std::string PwgModelFullPath = "";		//"../ModelFile/test/TTS/pwg/snapshot_iter_1000000.pdz";
					std::string MouthModelsPath = "";		// "../ModelFile/test/W2L/file/shenzhen_v3_20230227.pth";
					std::string FaceModelsPath = "";		// "../ModelFile/test/W2L/file/shenzhen_v3_20230227.dfm";
					std::string HumanForeground = "";		//16:9
					std::string HumanForeground2 = "";		//9:16
					std::string HumanBackground = "";		//16:9
					std::string HumanBackground2 = "";		//9:16
					digitalmysql::humaninfo HumanItem;
					if (digitalmysql::gethumaninfo(HumanID, HumanItem))
					{
						HumanName = HumanItem.humanname;
						SpeakModelFullPath = HumanItem.speakmodelpath;
						PwgModelFullPath = HumanItem.pwgmodelpath;
						MouthModelsPath = HumanItem.mouthmodelfile;
						FaceModelsPath = HumanItem.facemodelfile;
						//优先赋值不为空的字段
						HumanForeground = (HumanItem.foreground.empty()) ? (HumanItem.foreground2) : (HumanItem.foreground);
						HumanForeground2 = (HumanItem.foreground2.empty()) ? (HumanItem.foreground) : (HumanItem.foreground2);
						HumanBackground = (HumanItem.background.empty()) ? (HumanItem.background2) : (HumanItem.background);
						HumanBackground2 = (HumanItem.background2.empty()) ? (HumanItem.background) : (HumanItem.background2);
						//前端传空值时优化处理
						if (Foreground.empty())
							Foreground = (Window_Width > Window_Height) ? (webpath_from_osspath(HumanForeground)) : (webpath_from_osspath(HumanForeground2));
						if (Background.empty())
							Background = (Window_Width > Window_Height) ? (webpath_from_osspath(HumanBackground)) : (webpath_from_osspath(HumanBackground2));
					}

					_debug_to(0, ("[VideoMake] BeforeInsert: TaskID=%d, update=%d\n"), TaskID, exist_task);
					//添加任务到数据库
					digitalmysql::taskinfo new_taskitem;
					new_taskitem.taskid = TaskID;
					new_taskitem.belongid = req_authinfo_userid;
					new_taskitem.privilege = 1;
					new_taskitem.groupid = TaskGroupID;
					new_taskitem.versionname = TaskVersionName;
					new_taskitem.version = TaskVersion;
					new_taskitem.tasktype = TaskType;
					new_taskitem.moodtype = TaskMoodType;
					new_taskitem.speakspeed = Speed;
					new_taskitem.taskname = TaskName;
					new_taskitem.taskstate = 0xFF;//等待执行状态
					new_taskitem.taskprogress = 0;
					new_taskitem.createtime = gettimetext_now();
					new_taskitem.scannedtime = "";
					new_taskitem.finishtime = "";
					new_taskitem.priority = 0;
					new_taskitem.islastedit = 1;
					new_taskitem.humanid = HumanID;
					new_taskitem.humanname = HumanName;
					new_taskitem.ssmltext = InputSsml;
					new_taskitem.audio_path = InputAudio;
					new_taskitem.audio_format = "";
					new_taskitem.audio_length = 0;
					new_taskitem.video_path = "";
					new_taskitem.video_keyframe = "";
					new_taskitem.video_format = "";
					new_taskitem.video_length = 0;
					new_taskitem.video_width = 0;
					new_taskitem.video_height = 0;
					new_taskitem.video_fps = 0.0;
					new_taskitem.foreground = Foreground;
					new_taskitem.front_left = Front_left;
					new_taskitem.front_right = Front_right;
					new_taskitem.front_top = Front_top;
					new_taskitem.front_bottom = Front_bottom;
					new_taskitem.background = Background;
					new_taskitem.backaudio = BackAudio;
					new_taskitem.back_volume = (double)BackAudio_volume;
					new_taskitem.back_loop = BackAudio_loop;
					new_taskitem.back_start = BackAudio_start;
					new_taskitem.back_end = BackAudio_end;
					new_taskitem.window_width = Window_Width;
					new_taskitem.window_height = Window_Height;

					digitalmysql::addtaskinfo(TaskID, new_taskitem, exist_task);
					digitalmysql::setmergestate(TaskID, 0xFF);//任务状态为等待执行
					digitalmysql::setmergeprogress(TaskID, 0);//合成进度为0
					_debug_to(0, ("[VideoMake] AfterInsert: TaskID=%d, update=%d\n"), TaskID, exist_task);

					//更新版本编辑状态
					digitalmysql::settasklastedit(TaskGroupID, TaskID);

					//构造合成任务
					if (digitalmysql::isexisttask_taskid(TaskID))
					{
						actortaskinfo new_actortaskitem;
						new_actortaskitem.ActorPriority = 0;
						new_actortaskitem.ActorCreateTime = gettimecount_now();
						new_actortaskitem.ActorMessageID = getguidtext();
						new_actortaskitem.ActorTaskID = TaskID;
						new_actortaskitem.ActorTaskType = TaskType;
						new_actortaskitem.ActorMoodType = TaskMoodType;
						new_actortaskitem.ActorTaskSpeed = Speed;
						new_actortaskitem.ActorTaskName = TaskName;
						new_actortaskitem.ActorTaskText = InputSsml;
						new_actortaskitem.ActorTaskAudio = InputAudio;
						new_actortaskitem.ActorTaskHumanID = HumanID;
						new_actortaskitem.ActorTaskState = 0xFF;
						new_actortaskitem.SpeakModelPath = SpeakModelFullPath;
						new_actortaskitem.PWGModelPath = PwgModelFullPath;
						new_actortaskitem.MouthModelPath = MouthModelsPath;
						new_actortaskitem.FaceModelPath = FaceModelsPath;

						char data_buff[BUFF_SZ] = { 0 };
						snprintf(data_buff, BUFF_SZ, "\"TaskGroupID\":\"%s\",\"TaskID\":%d", TaskGroupID.c_str(), TaskID); data = data_buff;
						if (!Makesynch)	//thread run
						{
							reply_item.content_string = getjson_error(0, errmsg, data);
							_debug_to(0, ("[task_%d]: add to queue success.\n"), TaskID);
						}
						else			//now run
						{
							//更改数据库任务状态
							digitalmysql::setmergestate(TaskID, 0);		//任务状态为进行中
							digitalmysql::setmergeprogress(TaskID, 0);	//合成进度为0

							//尝试执行任务
							bool ret_result = false;
							long long dwS = gettimecount_now();
							ACTORINFO_MAP::iterator itFindActor = Container_TTSActor.begin();
							for (itFindActor; itFindActor != Container_TTSActor.end(); ++itFindActor)
							{
								std::string ret_json = "";
								actorinfo now_actoritem; now_actoritem.copydata(itFindActor->second);
								if (getjson_runtask_now(now_actoritem, new_actortaskitem, RUNTASK_TIMEOUT, ret_json))//生成音频
								{
									ret_result = true;
									reply_item.content_string = ret_json;
									break;
								}
							}
							long long dwE = gettimecount_now();
							_debug_to(0, ("[task_%d]: run success. speed %d s.\n"), TaskID, dwE - dwS);

							//每一个Actor都未能成功生成音频
							if (!ret_result) 
							{
								errmsg = "all actor have been tried and failed to generate, please try again later...";
								reply_item.content_string = getjson_error(1, errmsg, data);

								//新建的试听任务失败时,下次任务无法提交TaskID,直接删除避免多个失败的音频任务出现
								if (new_audiotask)
								{
									std::string deletemsg;
									digitalmysql::deletetask_taskid(TaskID, false, deletemsg);//无文件需要删除
								}
							}
						}
					}
					else
					{
						errmsg = "add task to mysql failed...";
						reply_item.content_string = getjson_error(1, errmsg);
						debug_ret = false;
					}
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
		}

#endif

#if 1 //管理员服务部分
		if (req->type == EVHTTP_REQ_POST)
		{
			//用户管理
			if (pathStr.compare(("/v1/manage/playout/UserList")) == 0)
			{
				debug_str = "{/v1/manage/playout/UserList}";
				//获取用户列表接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1
				digitalmysql::VEC_FILTERINFO vecfilterinfo;
				digitalmysql::filterinfo filteritem[256]; int filterIdx = 0;
				std::string UserName = "", UserType = "", ServiceType="", ProjectName="", UserEmail="", UserPhone="";
				if (mapBodyStrParameter.find("UserName") != mapBodyStrParameter.end())
					UserName = mapBodyStrParameter["UserName"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "loginname", UserName); filterIdx++;

				if (mapBodyStrParameter.find("UserType") != mapBodyStrParameter.end())
					UserType = mapBodyStrParameter["UserType"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "usertype", UserType); filterIdx++;

				if (mapBodyStrParameter.find("ServiceType") != mapBodyStrParameter.end())
					ServiceType = mapBodyStrParameter["ServiceType"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "servicetype", ServiceType); filterIdx++;

				if (mapBodyStrParameter.find("ProjectName") != mapBodyStrParameter.end())
					ProjectName = mapBodyStrParameter["ProjectName"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "projectname", ProjectName); filterIdx++;

				if (mapBodyStrParameter.find("UserEmail") != mapBodyStrParameter.end())
					UserEmail = mapBodyStrParameter["UserEmail"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "email", UserEmail); filterIdx++;

				if (mapBodyStrParameter.find("UserPhone") != mapBodyStrParameter.end())
					UserPhone = mapBodyStrParameter["UserPhone"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "phone", UserPhone); filterIdx++;
				//_debug_to(0, ("UserList: 1=%s 2=%s 3=%s 4=%s 5=%s 6=%s\n"), UserName.c_str(), UserType.c_str(), ServiceType.c_str(), ProjectName.c_str(), UserEmail.c_str(), UserPhone.c_str());

				int PageSize = 10, PageNum = 1;
				if (mapBodyIntParameter.find("PageSize") != mapBodyIntParameter.end())
					PageSize = mapBodyIntParameter["PageSize"];
				if (mapBodyIntParameter.find("PageNum") != mapBodyIntParameter.end())
					PageNum = mapBodyIntParameter["PageNum"];

				//2
				if (checkrequest)
				{
					reply_item.content_string = getjson_userlistinfo(vecfilterinfo, PageSize, PageNum);
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			else if (pathStr.compare(("/v1/manage/playout/NewUser")) == 0)
			{
				debug_str = "{/v1/manage/playout/NewUser}";
				//新建用户接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1
				std::string UserName = "", UserPassWord = "";
				int AdminFlag = -1, UserType = -1, ServiceType = -1;
				if (mapBodyStrParameter.find("UserName") != mapBodyStrParameter.end())
					UserName = mapBodyStrParameter["UserName"];
				CHECK_REQUEST_STR("UserName", UserName, errmsg, checkrequest);
				if (mapBodyStrParameter.find("UserPassWord") != mapBodyStrParameter.end())
					UserPassWord = mapBodyStrParameter["UserPassWord"];
				CHECK_REQUEST_STR("UserPassWord", UserPassWord, errmsg, checkrequest);

				if (mapBodyIntParameter.find("AdminFlag") != mapBodyIntParameter.end())
					AdminFlag = mapBodyIntParameter["AdminFlag"];
				CHECK_REQUEST_NUM("AdminFlag", AdminFlag, errmsg, checkrequest);
				if (mapBodyIntParameter.find("UserType") != mapBodyIntParameter.end())
					UserType = mapBodyIntParameter["UserType"];
				CHECK_REQUEST_NUM("UserType", UserType, errmsg, checkrequest);
				if (mapBodyIntParameter.find("ServiceType") != mapBodyIntParameter.end())
					ServiceType = mapBodyIntParameter["ServiceType"];
				CHECK_REQUEST_NUM("ServiceType", ServiceType, errmsg, checkrequest);

				std::string ProjectName = "", UserEmail = "", UserPhone = "";
				if (mapBodyStrParameter.find("ProjectName") != mapBodyStrParameter.end())
					ProjectName = mapBodyStrParameter["ProjectName"];
				CHECK_REQUEST_STR("ProjectName", ProjectName, errmsg, checkrequest);
				if (mapBodyStrParameter.find("UserEmail") != mapBodyStrParameter.end())
					UserEmail = mapBodyStrParameter["UserEmail"];
				if (mapBodyStrParameter.find("UserPhone") != mapBodyStrParameter.end())
					UserPhone = mapBodyStrParameter["UserPhone"];

				//2
				if (checkrequest)
				{
					std::string new_guidcode = getguidtext();

					digitalmysql::userinfo new_useritem;
					new_useritem.id = getrandomnum();
					new_useritem.disabled = 0;
					new_useritem.usertype = UserType;
					new_useritem.servicetype = ServiceType;
					new_useritem.rootflag = 1;//根用户,如需创建子用户需前端支持再修改
					new_useritem.adminflag = AdminFlag;
					new_useritem.userCode = new_guidcode;
					new_useritem.parentCode = new_guidcode;
					new_useritem.siteCode = new_guidcode;
					new_useritem.loginName = UserName;
					new_useritem.loginPassword = UserPassWord;
					new_useritem.phone = UserPhone;
					new_useritem.email = UserEmail;
					new_useritem.projectName = ProjectName;
					new_useritem.remainTime = 0;
					new_useritem.deadlineTime = "";

					if (digitalmysql::adduserinfo(new_useritem))
					{
						char data_buff[BUFF_SZ] = { 0 };
						snprintf(data_buff, BUFF_SZ, "\"UserID\":%d", new_useritem.id); data = data_buff;
						reply_item.content_string = getjson_error(0, errmsg, data);
					}
					else
					{
						errmsg = "add userinfo to mysql failed...";
						reply_item.content_string = getjson_error(1, errmsg);
						debug_ret = false;
					}
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			else if (pathStr.compare(("/v1/manage/playout/UpdateUser")) == 0)
			{
				debug_str = "{/v1/manage/playout/UpdateUser}";
				//更新用户信息接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1
				std::string UserName = "";
				if (mapBodyStrParameter.find("UserName") != mapBodyStrParameter.end())
					UserName = mapBodyStrParameter["UserName"];
				CHECK_REQUEST_STR("UserName", UserName, errmsg, checkrequest);
				int UserID = -1, AdminFlag = -1, UserType = -1, ServiceType = -1;
				if (mapBodyIntParameter.find("UserID") != mapBodyIntParameter.end())
					UserID = mapBodyIntParameter["UserID"];
				CHECK_REQUEST_NUM("UserID", UserID, errmsg, checkrequest);
				if (mapBodyIntParameter.find("AdminFlag") != mapBodyIntParameter.end())
					AdminFlag = mapBodyIntParameter["AdminFlag"];
				CHECK_REQUEST_NUM("AdminFlag", AdminFlag, errmsg, checkrequest);
				if (mapBodyIntParameter.find("UserType") != mapBodyIntParameter.end())
					UserType = mapBodyIntParameter["UserType"];
				CHECK_REQUEST_NUM("UserType", UserType, errmsg, checkrequest);
				if (mapBodyIntParameter.find("ServiceType") != mapBodyIntParameter.end())
					ServiceType = mapBodyIntParameter["ServiceType"];
				CHECK_REQUEST_NUM("ServiceType", ServiceType, errmsg, checkrequest);

				std::string ProjectName = "", UserEmail = "", UserPhone = "";
				if (mapBodyStrParameter.find("ProjectName") != mapBodyStrParameter.end())
					ProjectName = mapBodyStrParameter["ProjectName"];
				CHECK_REQUEST_STR("ProjectName", ProjectName, errmsg, checkrequest);
				if (mapBodyStrParameter.find("UserEmail") != mapBodyStrParameter.end())
					UserEmail = mapBodyStrParameter["UserEmail"];
				if (mapBodyStrParameter.find("UserPhone") != mapBodyStrParameter.end())
					UserPhone = mapBodyStrParameter["UserPhone"];

				//2
				if (checkrequest)
				{
					if (digitalmysql::isexistuser_id(UserID))
					{
						digitalmysql::userinfo update_useritem;
						if (digitalmysql::getuserinfo(UserID, update_useritem))
						{
							update_useritem.usertype = UserType;
							update_useritem.servicetype = ServiceType;
							update_useritem.loginName = UserName;
							update_useritem.projectName = ProjectName;
							if (!UserPhone.empty())
								update_useritem.phone = UserPhone;
							if (!UserEmail.empty())
								update_useritem.email = UserEmail;

							if (digitalmysql::adduserinfo(update_useritem, true))
							{
								char data_buff[BUFF_SZ] = { 0 };
								snprintf(data_buff, BUFF_SZ, "\"UserID\":%d", update_useritem.id); data = data_buff;
								reply_item.content_string = getjson_error(0, errmsg, data);
							}
							else
							{
								errmsg = "add userinfo to mysql failed...";
								reply_item.content_string = getjson_error(1, errmsg);
								debug_ret = false;
							}
						}
						else
						{
							errmsg = "get userinfo from mysql failed...";
							reply_item.content_string = getjson_error(1, errmsg);
							debug_ret = false;
						}
					}
					else
					{
						errmsg = "user not exist in mysql...";
						reply_item.content_string = getjson_error(1, errmsg);
						debug_ret = false;
					}
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			else if (pathStr.compare(("/v1/manage/playout/RemoveUser")) == 0)
			{
				debug_str = "{/v1/manage/playout/RemoveUser}";
				//删除用户接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1
				int UserID = -1;
				if (mapBodyIntParameter.find("UserID") != mapBodyIntParameter.end())
					UserID = mapBodyIntParameter["UserID"];
				CHECK_REQUEST_NUM("UserID", UserID, errmsg, checkrequest);

				//2
				if (checkrequest)
				{
					if (digitalmysql::isexistuser_id(UserID))
					{
						std::vector<int> vecremoveids;
						vecremoveids.push_back(UserID);
						if (digitalmysql::isrootuser_id(UserID))
							digitalmysql::getuserid_childs(UserID, vecremoveids);

						for (size_t i = 0; i < vecremoveids.size(); i++)
						{
							if (!digitalmysql::deleteuserinfo_id(vecremoveids[i], errmsg))
							{
								reply_item.content_string = getjson_error(1, errmsg);
								debug_ret = false;
								break;
							}
						}
						if (debug_ret)
							reply_item.content_string = getjson_error(0, errmsg);
					}
					else
					{
						errmsg = "user not exist in mysql...";
						reply_item.content_string = getjson_error(1, errmsg);
						debug_ret = false;
					}
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			//订单管理
			else if (pathStr.compare(("/v1/manage/playout/IndentList")) == 0)
			{
				debug_str = "{/v1/manage/playout/IndentList}";
				//获取订单列表接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				int PageSize = 10, PageNum = 1;
				if (mapBodyIntParameter.find("PageSize") != mapBodyIntParameter.end())
					PageSize = mapBodyIntParameter["PageSize"];
				if (mapBodyIntParameter.find("PageNum") != mapBodyIntParameter.end())
					PageNum = mapBodyIntParameter["PageNum"];

				//1
				digitalmysql::VEC_FILTERINFO vecfilterinfo;
				digitalmysql::filterinfo filteritem[256]; int filterIdx = 0;
				std::string CreateTimeStart = "0000-01-01 00:00:00", CreateTimeEnd = "9999-01-01 23:59:59";
				if (mapBodyStrParameter.find("CreateTimeStart") != mapBodyStrParameter.end() && (!mapBodyStrParameter["CreateTimeStart"].empty()))
					CreateTimeStart = mapBodyStrParameter["CreateTimeStart"]; 
				if (mapBodyStrParameter.find("CreateTimeEnd") != mapBodyStrParameter.end() && (!mapBodyStrParameter["CreateTimeEnd"].empty()))
					CreateTimeEnd = mapBodyStrParameter["CreateTimeEnd"];
				if (!CreateTimeStart.empty() || !CreateTimeEnd.empty())
				{
					std::string CreateTimeValue = std::string("'") + CreateTimeStart + std::string("' and '") + CreateTimeEnd + std::string("'");

					filteritem[filterIdx].filtertype = 1;
					ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "createtime", CreateTimeValue); filterIdx++;
				}
				
				std::string RootName = "", ServiceType = "", OperationWay = "", IndentType = "", IndentContent = "";
				if (mapBodyStrParameter.find("RootName") != mapBodyStrParameter.end())
					RootName = mapBodyStrParameter["RootName"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "sbt_userinfo.loginname", RootName); filterIdx++;

				if (mapBodyStrParameter.find("ServiceType") != mapBodyStrParameter.end())
					ServiceType = mapBodyStrParameter["ServiceType"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "sbt_indentinfo.servicetype", ServiceType); filterIdx++;

				if (mapBodyStrParameter.find("OperationWay") != mapBodyStrParameter.end())
					OperationWay = mapBodyStrParameter["OperationWay"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "operationway", OperationWay); filterIdx++;

				if (mapBodyStrParameter.find("IndentType") != mapBodyStrParameter.end())
					IndentType = mapBodyStrParameter["IndentType"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "indenttype", IndentType); filterIdx++;

				if (mapBodyStrParameter.find("IndentContent") != mapBodyStrParameter.end())
					IndentContent = mapBodyStrParameter["IndentContent"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "indentcontent", IndentContent); filterIdx++;

				//2
				if (checkrequest)
				{
					reply_item.content_string = getjson_indentlistinfo(vecfilterinfo, PageSize, PageNum);
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			else if (pathStr.compare(("/v1/manage/playout/NewIndent")) == 0)
			{
				debug_str = "{/v1/manage/playout/NewIndent}";
				//新建订单接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1
				int UserID = -1, ServiceType = -1, IndentType = -1;
				if (mapBodyIntParameter.find("UserID") != mapBodyIntParameter.end())
					UserID = mapBodyIntParameter["UserID"];
				CHECK_REQUEST_NUM("UserID", UserID, errmsg, checkrequest);
				if (mapBodyIntParameter.find("ServiceType") != mapBodyIntParameter.end())
					ServiceType = mapBodyIntParameter["ServiceType"];
				CHECK_REQUEST_NUM("ServiceType", ServiceType, errmsg, checkrequest);
				if (mapBodyIntParameter.find("IndentType") != mapBodyIntParameter.end())
					IndentType = mapBodyIntParameter["IndentType"];
				CHECK_REQUEST_NUM("IndentType", IndentType, errmsg, checkrequest);

				std::string DeadlineTime = "";
				if (mapBodyStrParameter.find("DeadlineTime") != mapBodyStrParameter.end())
					DeadlineTime = mapBodyStrParameter["DeadlineTime"];
				CHECK_REQUEST_STR("DeadlineTime", DeadlineTime, errmsg, checkrequest);

				//检查数据库用户有效性
				int RootID = UserID; std::string RootName = "";
				if (!digitalmysql::isrootuser_id(UserID))
					digitalmysql::getuserid_parent(UserID,RootID);
				if (!digitalmysql::isexistuser_id(RootID) || !digitalmysql::getusername_id(RootID,RootName))
				{
					checkrequest = false;
					errmsg = "not find user in mysql...";
				}


				//2
				if (checkrequest)
				{
					digitalmysql::indentinfo new_indentitem;
					new_indentitem.id = getrandomnum();
					new_indentitem.belongid = RootID;
					new_indentitem.rootname = RootName;
					new_indentitem.servicetype = ServiceType;
					new_indentitem.operationway = 0;
					new_indentitem.indenttype = IndentType;
					new_indentitem.indentcontent = "";
					new_indentitem.createtime = gettimetext_now();
					if (IndentType == 0)//新增数字人模型
					{
						std::string HumanName = "";
						if (mapBodyStrParameter.find("HumanName") != mapBodyStrParameter.end())
							HumanName = mapBodyStrParameter["HumanName"];
						CHECK_REQUEST_STR("HumanName", HumanName, errmsg, checkrequest);
						if (!checkrequest)
						{
							reply_item.content_string = getjson_error(1, errmsg);
							debug_ret = false;
						}
						else
						{
							std::string humanid = gethuman_uniqueid(req_authinfo_userid, HumanName);//获取数字人唯一ID
							std::string filefolder_humansource = folder_digitalmodel + std::string("/") + humanid;
							if (aws_enable)
								filefolder_humansource = std::string(PREFIX_OSS) + aws_rootfolder + std::string("/ModelFile/") + humanid;

							//保存数据库
							digitalmysql::humaninfo humanitem_add;
							humanitem_add.belongid = RootID;
							humanitem_add.privilege = 1;
							humanitem_add.humanname = HumanName;
							humanitem_add.humanid = humanid;
							humanitem_add.contentid = "";
							humanitem_add.sourcefolder = filefolder_humansource;
							humanitem_add.available = 0;//不可用
							humanitem_add.speakspeed = 1.0;
							humanitem_add.seriousspeed = 0.8;
							humanitem_add.imagematting = "";
							humanitem_add.keyframe = "";
							humanitem_add.foreground = "";
							humanitem_add.background = "";
							humanitem_add.foreground2 = "";
							humanitem_add.background2 = "";
							humanitem_add.speakmodelpath = "";
							humanitem_add.pwgmodelpath = "";
							humanitem_add.mouthmodelfile = "";
							humanitem_add.facemodelfile = "";
							bool updatehuman = (digitalmysql::isexisthuman_humanid(humanitem_add.humanid)) ? (true) : (false);
							if (digitalmysql::addhumaninfo(humanitem_add, updatehuman))
							{
								std::string time_start = gettimetext_now();
								std::string time_end = DeadlineTime;
								long long human_remaintime = 0;
								if (gettimecount_interval(time_start, time_end, human_remaintime) && setremaintime_human(humanitem_add.belongid, humanitem_add.humanid, human_remaintime))
								{
									new_indentitem.indentcontent = std::string("新增数字人模型: ") + HumanName + std::string(",到期时间: ") + DeadlineTime;
									reply_item.content_string = getjson_error(0, errmsg);
								}
								else
								{
									errmsg = "update humaninfo remaintime to mysql failed...";
									reply_item.content_string = getjson_error(1, errmsg);
									debug_ret = false;
								}
							}
							else
							{
								errmsg = "add humaninfo to mysql failed...";
								reply_item.content_string = getjson_error(1, errmsg);
								debug_ret = false;
							}
						}	
					}
					if (IndentType == 1)//续费数字人模型
					{
						std::string HumanID = "", HumanName = "";
						if (mapBodyStrParameter.find("HumanID") != mapBodyStrParameter.end())
							HumanID = mapBodyStrParameter["HumanID"];
						CHECK_REQUEST_STR("HumanID", HumanID, errmsg, checkrequest);
						if (mapBodyStrParameter.find("HumanName") != mapBodyStrParameter.end())
							HumanName = mapBodyStrParameter["HumanName"];
						CHECK_REQUEST_STR("HumanName", HumanName, errmsg, checkrequest);
						if (!checkrequest)
						{
							reply_item.content_string = getjson_error(1, errmsg);
							debug_ret = false;
						}
						else
						{
							if (digitalmysql::isexisthuman_humanid(HumanID))
							{
								std::string time_start = gettimetext_now();
								std::string time_end = DeadlineTime;
								long long human_remaintime = 0;
								if (gettimecount_interval(time_start, time_end, human_remaintime) && setremaintime_human(RootID, HumanID, human_remaintime))
								{
									new_indentitem.indentcontent = std::string("续费数字人模型: ") + HumanName + std::string(",到期时间: ") + DeadlineTime;
									reply_item.content_string = getjson_error(0, errmsg);
								}
								else
								{
									errmsg = "update humaninfo remaintime to mysql failed...";
									reply_item.content_string = getjson_error(1, errmsg);
									debug_ret = false;
								}
							}
							else
							{
								errmsg = "human not exist in mysql...";
								reply_item.content_string = getjson_error(1, errmsg);
								debug_ret = false;
							}
						}
					}
					if (IndentType == 2)//开通生成服务
					{
						std::string UserCode = "";
						if (digitalmysql::isexistuser_id(UserID) && digitalmysql::getusercode_id(UserID, UserCode))
						{
							if (digitalmysql::setuserinfo_deadlinetime(UserCode, DeadlineTime))
							{
								new_indentitem.indentcontent = std::string("开通生成服务: 到期时间:") + DeadlineTime;
								reply_item.content_string = getjson_error(0, errmsg);
							}
							else
							{
								errmsg = "update user deadlinetime failed...";
								reply_item.content_string = getjson_error(1, errmsg);
								debug_ret = false;
							}
						}
						else
						{
							errmsg = "user not exist in mysql...";
							reply_item.content_string = getjson_error(1, errmsg);
							debug_ret = false;
						}
					}
					if (IndentType == 3)//续费生成服务
					{
						std::string UserCode = "";
						if (digitalmysql::isexistuser_id(UserID) && digitalmysql::getusercode_id(UserID, UserCode))
						{
							if (digitalmysql::setuserinfo_deadlinetime(UserCode, DeadlineTime))
							{
								new_indentitem.indentcontent = std::string("续费生成服务: 到期时间:") + DeadlineTime;
								reply_item.content_string = getjson_error(0, errmsg);
							}
							else
							{
								errmsg = "update user deadlinetime failed...";
								reply_item.content_string = getjson_error(1, errmsg);
								debug_ret = false;
							}
						}
						else
						{
							errmsg = "user not exist in mysql...";
							reply_item.content_string = getjson_error(1, errmsg);
							debug_ret = false;
						}
					}
					if (IndentType == 4)//加量时长包
					{
						std::string AppendTime = "";
						if (mapBodyStrParameter.find("AppendTime") != mapBodyStrParameter.end())
							AppendTime = mapBodyStrParameter["AppendTime"];
						CHECK_REQUEST_STR("AppendTime", AppendTime, errmsg, checkrequest);
						if (!checkrequest)
						{
							reply_item.content_string = getjson_error(1, errmsg);
							debug_ret = false;
						}
						else
						{
							std::string UserCode = "";
							if (digitalmysql::isexistuser_id(UserID) && digitalmysql::getusercode_id(UserID, UserCode))
							{
								long long user_remaintime = 0;
								if (digitalmysql::getuserinfo_remaintime(UserCode, user_remaintime))
								{
									user_remaintime += (atoll(AppendTime.c_str()) * 60);//累加剩余时间
									if (digitalmysql::setuserinfo_deadlinetime(UserCode, DeadlineTime) && digitalmysql::setuserinfo_remaintime(UserCode, user_remaintime))
									{
										new_indentitem.indentcontent = std::string("生成服务加量: ") + AppendTime + std::string("分钟, 加量后剩余:") + gettimeshow_second(user_remaintime) + std::string("分钟, 到期时间:") + DeadlineTime;
										reply_item.content_string = getjson_error(0, errmsg);
									}
									else
									{
										errmsg = "update user deadlinetime/remaintime failed...";
										reply_item.content_string = getjson_error(1, errmsg);
										debug_ret = false;
									}
								}
								else
								{
									errmsg = "get user remaintime from mysql failed...";
									reply_item.content_string = getjson_error(1, errmsg);
									debug_ret = false;
								}
							}
							else
							{
								errmsg = "user not exist in mysql...";
								reply_item.content_string = getjson_error(1, errmsg);
								debug_ret = false;
							}
						}
					}

					//
					if (debug_ret && digitalmysql::addindentinfo(new_indentitem))
					{
						char data_buff[BUFF_SZ] = { 0 };
						snprintf(data_buff, BUFF_SZ, "\"IndentID\":%d", new_indentitem.id); data = data_buff;
						reply_item.content_string = getjson_error(0, errmsg, data);
					}
					else
					{
						if(errmsg.empty())
							errmsg = "add indent to mysql failed...";
						reply_item.content_string = getjson_error(1, errmsg);
						debug_ret = false;
					}
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			//任务管理
			else if (pathStr.compare(("/v1/manage/playout/TaskList")) == 0)
			{
				debug_str = "{/v1/manage/playout/TaskList}";
				//任务列表接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				int PageSize = 10, PageNum = 1;
				if (mapBodyIntParameter.find("PageSize") != mapBodyIntParameter.end())
					PageSize = mapBodyIntParameter["PageSize"];
				if (mapBodyIntParameter.find("PageNum") != mapBodyIntParameter.end())
					PageNum = mapBodyIntParameter["PageNum"];

				//1
				digitalmysql::VEC_FILTERINFO vecfilterinfo;
				digitalmysql::filterinfo filteritem[256]; int filterIdx = 0;
				std::string CreateTimeStart = "0000-01-01 00:00:00", CreateTimeEnd = "9999-01-01 23:59:59";
				if (mapBodyStrParameter.find("CreateTimeStart") != mapBodyStrParameter.end() && (!mapBodyStrParameter["CreateTimeStart"].empty()))
					CreateTimeStart = mapBodyStrParameter["CreateTimeStart"];
				if (mapBodyStrParameter.find("CreateTimeEnd") != mapBodyStrParameter.end() && (!mapBodyStrParameter["CreateTimeEnd"].empty()))
					CreateTimeEnd = mapBodyStrParameter["CreateTimeEnd"];
				if (!CreateTimeStart.empty() || !CreateTimeEnd.empty())
				{
					std::string CreateTimeValue = std::string("'") + CreateTimeStart + std::string("' and '") + CreateTimeEnd + std::string("'");

					filteritem[filterIdx].filtertype = 1;
					ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "sbt_doctask.createtime", CreateTimeValue); filterIdx++;
				}

				std::string FinishTimeStart = "0000-01-01 00:00:00", FinishTimeEnd = "9999-01-01 23:59:59";
				if (mapBodyStrParameter.find("FinishTimeStart") != mapBodyStrParameter.end() && (!mapBodyStrParameter["FinishTimeStart"].empty()))
					FinishTimeStart = mapBodyStrParameter["FinishTimeStart"];
				if (mapBodyStrParameter.find("FinishTimeEnd") != mapBodyStrParameter.end() && (!mapBodyStrParameter["FinishTimeEnd"].empty()))
					FinishTimeEnd = mapBodyStrParameter["FinishTimeEnd"];
				if (!FinishTimeStart.empty() || !FinishTimeEnd.empty())
				{
					std::string FinishTimeValue = std::string("'") + FinishTimeStart + std::string("' and '") + FinishTimeEnd + std::string("'");

					filteritem[filterIdx].filtertype = 1;
					ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "sbt_doctask.finishtime", FinishTimeValue); filterIdx++;
				}

				std::string UserName = "", RootName = ""; 
				if (mapBodyStrParameter.find("UserName") != mapBodyStrParameter.end())
					UserName = mapBodyStrParameter["UserName"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "sbt_userinfo.loginname", UserName); filterIdx++;
					
				int rootid = 0; std::vector<int> vecbelongids;
				if (mapBodyStrParameter.find("RootName") != mapBodyStrParameter.end())
					RootName = mapBodyStrParameter["RootName"];
				if (!RootName.empty() && digitalmysql::getuserid_likename(RootName, rootid))
				{
					if (rootid != 0 && digitalmysql::getuserid_childs(rootid, vecbelongids))
						vecbelongids.push_back(rootid);

					std::string BelongIdValue = "";
					for (size_t i = 0; i < vecbelongids.size(); i++)
					{
						char temp[256] = { 0 };
						std::string belingid = "";
						snprintf(temp, 256, "%d", vecbelongids[i]); belingid = temp;
						if (vecbelongids.size() > 1 && i != (vecbelongids.size() - 1))
							belingid += std::string(",");
						BelongIdValue += belingid;
					}
					filteritem[filterIdx].filtertype = 2;
					ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "sbt_doctask.belongid", BelongIdValue); filterIdx++;
				}

				std::string TaskFileName = "";
				if (mapBodyStrParameter.find("TaskFileName") != mapBodyStrParameter.end())
					TaskFileName = mapBodyStrParameter["TaskFileName"];
				if (!TaskFileName.empty())
				{
					char tempbuff[2048] = { 0 };
					std::string CustomFilter = "";
					snprintf(tempbuff, 2048, "audiofile like '%%%s%%' or videofile like '%%%s%%'", TaskFileName.c_str(), TaskFileName.c_str()); CustomFilter = tempbuff;

					filteritem[filterIdx].filtertype = 3;
					ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "Custom", CustomFilter); filterIdx++;
				}

				std::string HumanName = "", TaskName = "", TaskState = "";
				if (mapBodyStrParameter.find("HumanName") != mapBodyStrParameter.end())
					HumanName = mapBodyStrParameter["HumanName"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "sbt_doctask.humanname", HumanName); filterIdx++;
				if (mapBodyStrParameter.find("TaskName") != mapBodyStrParameter.end())
					TaskName = mapBodyStrParameter["TaskName"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "sbt_doctask.taskname", TaskName); filterIdx++;
				if (mapBodyStrParameter.find("TaskState") != mapBodyStrParameter.end())
					TaskState = mapBodyStrParameter["TaskState"];
				ADD_FILTER_INFO(vecfilterinfo, filteritem[filterIdx], "sbt_doctask.state", TaskState); filterIdx++;
				

				//2
				if (checkrequest)
				{
					reply_item.content_string = getjson_tasklistinfo(vecfilterinfo, PageSize, PageNum);
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			else if (pathStr.compare(("/v1/manage/playout/UpdateTask")) == 0)
			{
				debug_str = "{/v1/manage/playout/UpdateTask}";
				//更新任务接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1
				int TaskID = -1;
				if (mapBodyIntParameter.find("TaskID") != mapBodyIntParameter.end())
					TaskID = mapBodyIntParameter["TaskID"];
				CHECK_REQUEST_NUM("TaskID", TaskID, errmsg, checkrequest);

				int TaskPriority = -1;
				if (mapBodyIntParameter.find("TaskPriority") != mapBodyIntParameter.end())
					TaskPriority = mapBodyIntParameter["TaskPriority"];
				CHECK_REQUEST_NUM("TaskPriority", TaskPriority, errmsg, checkrequest);

				if (checkrequest)
				{
					if (digitalmysql::isexisttask_taskid(TaskID) && digitalmysql::setpriority(TaskID, TaskPriority))
					{
						setpriority_actortask(TaskID, TaskPriority);
						reply_item.content_string = getjson_error(0, errmsg);
					}
					else
					{
						errmsg = "update task priority failed...";
						reply_item.content_string = getjson_error(1, errmsg);
						debug_ret = false;
					}
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}
			else if (pathStr.compare(("/v1/manage/playout/RemoveTask")) == 0)
			{
				debug_str = "{/v1/manage/playout/RemoveTask}";
				//删除任务接口
				bool checkrequest = true; std::string errmsg = "success"; std::string data = "";

				//1
				int TaskID = -1;
				if (mapBodyIntParameter.find("TaskID") != mapBodyIntParameter.end())
					TaskID = mapBodyIntParameter["TaskID"];
				CHECK_REQUEST_NUM("TaskID", TaskID, errmsg, checkrequest);

				int _DeleteFile = 1;
				//if (mapBodyIntParameter.find("DeleteFile") != mapBodyIntParameter.end())
				//	_DeleteFile = mapBodyIntParameter["DeleteFile"];

				//2
				if (checkrequest)
				{
					//删除数据库+本地文件
					if (aws_enable)
					{
						digitalmysql::taskinfo deltaskitem;
						if (digitalmysql::gettaskinfo(TaskID, deltaskitem))
						{
							awsUpload uploadObj;
							uploadObj.SetAWSConfig(aws_webprefix, aws_endpoint, aws_url, aws_ak, aws_sk, aws_bucket);

							//删除云盘文件
							std::vector<std::string> vectaskfile;
							vectaskfile.push_back(deltaskitem.audio_path);
							vectaskfile.push_back(deltaskitem.video_path);
							vectaskfile.push_back(deltaskitem.video_keyframe);
							for (size_t i = 0; i < vectaskfile.size(); i++)
							{
								std::string objectfile_path = vectaskfile[i];
								if (str_prefixsame(objectfile_path, aws_webprefix))//认为是web路径
								{
									ansi_to_utf8(objectfile_path.c_str(), objectfile_path.length(), objectfile_path);//awsUpload需UTF8编码参数
									std::string ossfile_path = osspath_from_webpath(objectfile_path);
									objectfile_path = str_replace(ossfile_path, std::string(PREFIX_OSS), std::string(""));
									uploadObj.RemoveFile(objectfile_path);
								}
							}
						}
					}

					bool result = digitalmysql::deletetask_taskid(TaskID, _DeleteFile, errmsg);
					int code = (result) ? (0) : (1);
					reply_item.content_string = getjson_error(code, errmsg);
				}
				else
				{
					reply_item.content_string = getjson_error(1, errmsg);
					debug_ret = false;
				}
			}	
		}
#endif

		
		//请求返回
http_reply:
		global_http_reply(req, reply_item);

		long long end_time = gettimecount_now();
		std::string ret_str = (debug_ret) ? ("OK") : ("FAILED");
		_debug_to(1, ("http server: request = [%s][%s], result = [%s], runtime=[%lld]s, code = [%d]\n"), typeStr.c_str(), debug_str.c_str(), ret_str.c_str(), end_time - start_time, reply_item.http_code);
	}
	catch (...) {
		_debug_to(1, ("http server: handler throw exception\n"));
	}
}
#endif

void InitCMDWnd()
{
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin, &mode);
	mode &= ~ENABLE_QUICK_EDIT_MODE; //移除快速编辑模式
	mode &= ~ENABLE_INSERT_MODE; //移除插入模式
	mode &= ~ENABLE_MOUSE_INPUT;
	SetConsoleMode(hStdin, mode);

	char cmd[64] = {0};
	sprintf(cmd, "mode con cols=%d lines=%d", 160, 40);
	system(cmd);
}

int main()
{
	//创建互斥量
	HANDLE hMutex = CreateMutex(NULL, TRUE, "Mutex_HttpServer");
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		printf("The Application is Running...\n");
		system("pause");
		exit(0);
	}

	//设置异常处理函数
	SetUnhandledExceptionFilter(ExceptionHandler);

	//
	InitCMDWnd();
	std::string apppath = getexepath(); apppath = str_replace(apppath, std::string("\\"), std::string("/"));

	bool loadconfig = true;
	std::string config_error = "";
	std::string configpath = apppath + "/config.txt";
	std::string configpath_utf8; ansi_to_utf8(configpath.c_str(), configpath.length(), configpath_utf8);
	_debug_to(0, ("COFIG PATH=%s\n"), configpath_utf8.c_str());
	if (loadconfig && !getconfig_global(configpath, config_error))
	{
		_debug_to(1, ("GLOBAL config load failed: %s\n", config_error.c_str()));
		loadconfig = false;
	}
	if (loadconfig && !getconfig_cert(configpath, config_error))
	{
		_debug_to(1, ("AWS config load failed: %s\n", config_error.c_str()));
		loadconfig = false;
	}
	if (loadconfig && !getconfig_aws(configpath, config_error))
	{
		_debug_to(1, ("AWS config load failed: %s\n", config_error.c_str()));
		loadconfig = false;
	}
	if (loadconfig && !getconfig_sso(configpath, config_error))
	{
		_debug_to(1, ("SSO config load failed: %s\n", config_error.c_str()));
		loadconfig = false;
	}
	if (loadconfig && !digitalmysql::getconfig_mysql(configpath, config_error))
	{
		_debug_to(1, ("MYSQL config load failed: %s\n", config_error.c_str()));
		loadconfig = false;
	}
	if (loadconfig && !getconfig_actornode(configpath, config_error))
	{
		_debug_to(1, ("ACTOR config load failed: %s\n", config_error.c_str()));
		loadconfig = false;
	}
	if (loadconfig && !getconfig_actornode_tts(configpath, config_error))
	{
		_debug_to(1, ("TTSACTOR config load failed: %s\n", config_error.c_str()));
		loadconfig = false;
	}
	if (loadconfig && !getconfig_rabbitmq(configpath, config_error))
	{
		_debug_to(1, ("RABBITMQ config load failed: %s\n", config_error.c_str()));
		loadconfig = false;
	}
	g_RabbitmqSender = new nsRabbitmq::cwRabbitmqPublish(rabbitmq_ip, rabbitmq_port, rabbitmq_user, rabbitmq_passwd, nullptr, nullptr);

	//初始化
	httpServer::complex_httpServer server;
	if (cert_enable)
	{
		std::string path_certificate = apppath + std::string("/cert/") + key_certificate;
		std::string path_private = apppath + std::string("/cert/") + key_private;
		httpServer::openssl_common_init(path_certificate, path_private);//OpenSSL Need
	}

	int httpPort = 8081;//监听端口
	int threadCount = 10;//开启线程池个数
	int ret_startserver = server.start_http_server(global_http_generic_handler, nullptr, httpPort, threadCount, 10240);//开启监听
	if (ret_startserver < 0)
	{
		_debug_to(1, ("start http_server failed\n"));
	}
	else {
		_debug_to(1, ("start http_server success\n"));
	}

	//开启合成任务分配线程
	pthread_mutex_init(&mutex_actortaskinfo, NULL);
	int ret_sendtask = pthread_create(&threadid_sendtask_thread, nullptr, pthread_sendtask_thread, nullptr);
	if (ret_sendtask != 0)
	{
		_debug_to(1, ("thread_sendtask create error\n"));
	}
	else
	{
		_debug_to(1, ("thread_sendtask is runing\n"));
	}

	//开启定时清理会话线程
	pthread_mutex_init(&mutex_sessioninfo, NULL);
	int ret_clearsession = pthread_create(&threadid_clearsession_thread, nullptr, pthread_clearsession_thread, nullptr);
	if (ret_clearsession != 0)
	{
		_debug_to(1, ("thread_clearsession create error\n"));
	}
	else
	{
		_debug_to(1, ("thread_clearsession is runing\n"));
	}

	//开启定时减少数字人模型可用时长线程
	int ret_sethumanremain = pthread_create(&threadid_sethumanremain_thread, nullptr, pthread_sethumanremain_thread, nullptr);
	if (ret_sethumanremain != 0)
	{
		_debug_to(1, ("thread_sethumanremain create error\n"));
	}
	else
	{
		_debug_to(1, ("thread_sethumanremain is runing\n"));
	}

start_wait:
	//keep runing
	while (1)
	{
		char ch[256] = { 0 };
		printf(("输入'Q'或‘q’退出程序:\n"));
		gets_s(ch, 255);
		std::string str; str = ch;
		if (str.compare("Q") == 0 || str.compare("q") == 0)
		{
			//结束http监听
			server.stop_http_server();
			break;
		}
	}

	//释放互斥量
	ReleaseMutex(hMutex);
	CloseHandle(hMutex);
}






