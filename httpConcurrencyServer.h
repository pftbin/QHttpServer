#pragma once

#if defined WIN32  //SYS-WIN32

//
#include <string>
#include <iostream>
#include <WinSock2.h>

//LIBEVENT
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

#pragma comment(lib,"event.lib")
#pragma comment(lib,"event_core.lib")
#pragma comment(lib,"event_extra.lib")
#pragma comment(lib,"event_openssl.lib")

//OpenSSL
#include "openssl/ssl.h"
#include "openssl/err.h"
#pragma comment(lib,"libssl.lib")
#pragma comment(lib,"libcrypto.lib")


//PTHREAD
#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>

namespace httpServer
{
	//openssl
	void openssl_common_init(std::string certificate_chain, std::string private_key);

	//libevent
	typedef void(*http_cb_Func)(struct evhttp_request *, void *);//定义函数指针
	struct httpThread
	{
		HANDLE  threadhandle;
		struct event_base* event_base;
		struct evhttp* http_server;
		void *pparam;
		int   mark;
		HWND  mainWindow;
		DWORD threadid;
		CRITICAL_SECTION get_CriticalSection;

		httpThread()
		{
            threadhandle = nullptr;
            event_base = nullptr;
            http_server = nullptr;
            pparam = nullptr;
            threadid = 0;
            mainWindow = nullptr;
		}
	};
	struct complex_httpServer
	{
		struct httpThread *pSeverThread;
		HANDLE http_checkEvent[2];

		CRITICAL_SECTION get_CriticalSection;

		std::string ipaddr;
		int port;

		http_cb_Func pFunc;//回调函数指针

		HWND mainWindow;

		int nthreads;
		int backlog;
		int nfd;
		complex_httpServer()
		{
			::InitializeCriticalSection(&get_CriticalSection);
            mainWindow = nullptr;

            pSeverThread = nullptr;
            http_checkEvent[0] = nullptr;
            http_checkEvent[1] = nullptr;

			port = 8130;
			nthreads = 10;
			backlog = 10240;
			nfd = -1;
		}
		~complex_httpServer()
		{
			stop_http_server();
			::DeleteCriticalSection(&get_CriticalSection);
		}

		int start_http_server();
		int start_http_server(http_cb_Func _pFunc, HWND _mainWindow, int _port, int httpthreads, int nbacklog, std::string _ipaddr = (""));
		void stop_failed_server(int threadcount);
		void stop_http_server();
	};
};


#else  //SYS-LINUX

#include <pthread.h>
#include "event2/thread.h"
#include "event2/buffer.h"
#include "evhttp.h"
#include "event.h"
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include "logutilPublic.h"
namespace httpServer
{
	typedef void(*http_cb_Func)(struct evhttp_request *, void *);//定义函数指针

    typedef struct httpThread
	{
		pthread_t ths;
		struct event_base * eventbase;
		struct evhttp* httpserver;
		void *pparam;
		int   mark;

		char pathchar[MAX_PATH];
		char querychar[MAX_PATH];
		char hostchar[MAX_PATH];
		char urichar[MAX_PATH];
    }httpThread;

	typedef class complex_httpServer
	{
	public:
        httpThread* pSeverThread;
		http_cb_Func pFunc;//回调函数指针
		void* mainWindow;
		int nthreads;
		int backlog;
		int nfd;
		int port;

        std::string ipaddr;

		int starthttpserver();
        int start_http_server(http_cb_Func _pFunc, void* _mainWindow, int _port, int httpthreads, int nbacklog, std::string _ipaddr = (""));
		void stop_http_server();
	}complex_httpServer;
}
#endif
