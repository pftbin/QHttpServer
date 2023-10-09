#include "httpConcurrencyServer.h"
#include "public.h"

//custom loger
static FileWriter loger_httpserver("httpserver.log");

static bool globalIsIPStringValid(std::string IPString)
{
	std::string strIpAddr;
	strIpAddr = IPString;

	std::vector<std::string> ipVector;
	globalSpliteString(IPString, ipVector, ("."));
	if (ipVector.size() != 4U) return false;
	for (std::vector<std::string>::iterator it = ipVector.begin(); it != ipVector.end(); it++)
	{
		int temp;
		globalStrToIntDef(const_cast<LPTSTR>(it->c_str()), temp);
		if (temp < 0 || temp > 255) return false;
	}
	struct in_addr addr;
	//if (inet_pton(AF_INET, strIpAddr.c_str(), (void *)&addr) == 1)
	if (inet_pton(AF_INET, strIpAddr.c_str(), reinterpret_cast<void*>(&addr)) == 1)
		return true;
	else
		return false;
}

//OPENSSL
/* 这个是调用openSSL提供的打印log接口 */
static void die_most_horribly_from_openssl_error(const char* func)
{
	_debug_to(loger_httpserver, 1, ("%s failed...\n"), func);

	/* This is the OpenSSL function that prints the contents of the
	 * error stack to the specified file handle. */
	ERR_print_errors_fp(stderr);

	exit(EXIT_FAILURE);
}
void error_exit(const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
}
/* OpenSSL有使用未初始化内存的习惯, 我们安装一个自定义malloc函数，用于实际调用 */
static void* my_zeroing_malloc(size_t howmuch)
{
	return calloc(1, howmuch);
}
//这个回调负责创建一个新的SSL连接并将其包装在OpenSSL bufferevent中。就是这样我们实现了一个HTTPS服务器而不是一个普通的旧HTTP服务器。
static struct bufferevent* bevcb(struct event_base* base, void* arg)
{
	struct bufferevent* r;
	SSL_CTX* ctx = (SSL_CTX*)arg;

	r = bufferevent_openssl_socket_new(base,
		-1,
		SSL_new(ctx),
		BUFFEREVENT_SSL_ACCEPTING,
		BEV_OPT_CLOSE_ON_FREE);
	return r;
}
static void server_setup_certs(SSL_CTX* ctx, const char* certificate_file, const char* private_key)
{
	_debug_to(loger_httpserver, 1, ("Loading certificate chain = '%s', private key = '%s'\n"), certificate_file, private_key);
	if (1 != SSL_CTX_use_certificate_file(ctx, certificate_file, SSL_FILETYPE_PEM))
		die_most_horribly_from_openssl_error("SSL_CTX_use_certificate_chain_file");

	if (1 != SSL_CTX_use_PrivateKey_file(ctx, private_key, SSL_FILETYPE_PEM))
		die_most_horribly_from_openssl_error("SSL_CTX_use_PrivateKey_file");

	if (1 != SSL_CTX_check_private_key(ctx))
		die_most_horribly_from_openssl_error("SSL_CTX_check_private_key");
}

#if defined WIN32  //SYS-WIN32

#pragma comment(lib,"pthreadVC2.lib")
namespace httpServer
{
	//
	bool OpenSSL_Enable;
	std::string str_certificate_file;
	std::string str_private_key;
	void openssl_common_init(std::string certificate_file, std::string private_key)
	{
		// 初始化OpenSSL库
		SSL_library_init();
		SSL_load_error_strings();
		OpenSSL_add_all_algorithms();

		str_certificate_file = certificate_file; str_private_key = private_key;
		_debug_to(loger_httpserver, 1, ("OpenSSL version = %s, libevent version = %s, \ncertificate = %s, private key = %s\n"), SSLeay_version(SSLEAY_VERSION), event_get_version(), str_certificate_file.c_str(), str_private_key.c_str());
		
		OpenSSL_Enable = false;
		if (str_certificate_file.empty() || str_private_key.empty() || get_path_name(str_certificate_file).empty() || get_path_name(str_private_key).empty())
		{
			OpenSSL_Enable = false;
		}
		else if (is_existfile(str_certificate_file.c_str()) && is_existfile(str_private_key.c_str()))
		{
			OpenSSL_Enable = true;
		}
	}

	//
	int init_win_socket()
	{
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			return -1;
		}
		return 0;
	}
	int httpserver_bindsocket(int port, int backlog) {
		int r;

		int nsfd;
		nsfd = socket(AF_INET, SOCK_STREAM, 0);//创建套接字
		if (nsfd < 0) return -1;

		int one = 1;
		r = setsockopt(nsfd, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(int));//设置套接字的SO_REUSEADDR属性 执行正确返回0

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port);

		r = bind(nsfd, (struct sockaddr*)&addr, sizeof(addr));
		if (r < 0) return -1;
		r = listen(nsfd, backlog);
		if (r < 0) return -1;

		unsigned long ul = 1;
		int ret = ioctlsocket(nsfd, FIONBIO, (unsigned long *)&ul);//允许非阻塞模式
		if (ret == SOCKET_ERROR)//设置失败。
		{
			return -1;
		}

		return nsfd;
	}
	DWORD WINAPI GlobalHttpBaseFunc(LPVOID lpParam)
	{
        try {
			struct httpThread * pThread = (struct httpThread*)lpParam;
			event_base_dispatch(pThread->event_base);//等待事件被触发，然后调用它们的回调函数
			evhttp_free(pThread->http_server);//释放以前创建的HTTP服务器
            pThread->http_server = nullptr;
			return NULL;
        } catch (...) {
			return NULL;
		}
	}

	//httpServer命名空间内
	int complex_httpServer::start_http_server()
	{
		stop_http_server();

		//http_checkEvent[0] = ::CreateEvent(NULL, true, false, NULL);
		//http_checkEvent[1] = ::CreateEvent(NULL, true, false, NULL);

#if defined WIN32
		init_win_socket();//初始化winsock服务
#endif
		if (nthreads > 0)
		{
			try
			{
				pSeverThread = new struct httpThread[nthreads];//创建100个httpTreads结构体
				if (pSeverThread == nullptr)
				{
					_debug_to(loger_httpserver, 1, ("start http server failed,error:new http thread failed\n"));
					return -1;
				}
			}
			catch (...)
			{
				_debug_to(loger_httpserver, 1, ("start http server failed, error:new http thread catch exception\n"));
				return -1;
			}
		}
		_debug_to(loger_httpserver, 1, ("start http server begin, port is %d, ipaddr is %s\n"), port, ipaddr.c_str());
		try
		{
			int r, i;
			evthread_use_windows_threads();//开启多线程功能
			//evthread_set_condition_callbacks();

			//evthread_set_id_callback();

			nfd = httpserver_bindsocket(port, backlog);//返回一个初始化的监听ANY_IPADDR的套接字
			if (nfd < 0) return -1;
			for (i = 0; i < nthreads; i++)
			{
				pSeverThread[i].get_CriticalSection = get_CriticalSection;//get_CriticalSection已经初始化的临界区资源
				pSeverThread[i].mainWindow = mainWindow;//NULL
				struct event_base* base = event_base_new();//event_base是libevent的事务处理框架，负责事件注册、删除等 
				//event_base_new 创建event_base对象
				if (base == nullptr)
				{
					_debug_to(loger_httpserver, 1, ("start http server failed,error:new http base failed\n"));
					stop_failed_server(i);
					return -1;
				}
				pSeverThread[i].event_base = base;
				struct evhttp* httpd = evhttp_new(base);//创建http服务器 base是用于接收HTTP事件的事件库
				if (httpd == nullptr)
				{
					_debug_to(loger_httpserver, 1, ("start http server failed,error:new evhttp_new failed\n"));
					stop_failed_server(i);
					event_base_free(base);
					return -1;
				}
				pSeverThread[i].http_server = httpd;

				if (OpenSSL_Enable)
				{
					//------OPENSSL------
					/* 创建SSL上下文环境 ，可以理解为 SSL句柄 */
					SSL_CTX* ctx = SSL_CTX_new(SSLv23_server_method());
					SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE | SSL_OP_SINGLE_ECDH_USE | SSL_OP_NO_SSLv2);
					SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

					/*选择椭圆曲线与椭圆曲线密码套件一起使用*/
					EC_KEY* ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
					if (!ecdh)
						die_most_horribly_from_openssl_error("EC_KEY_new_by_curve_name");
					if (1 != SSL_CTX_set_tmp_ecdh(ctx, ecdh))
						die_most_horribly_from_openssl_error("SSL_CTX_set_tmp_ecdh");

					/* 选择服务器证书 和 服务器私钥. */
					if (str_certificate_file.empty() || str_private_key.empty())
						die_most_horribly_from_openssl_error("SSL_CTX_set_certificate_key");
					const char* certificate_file = str_certificate_file.c_str();	//"server-certificate-chain.pem";
					const char* private_key = str_private_key.c_str();				//"server-private-key.pem";

					/* 设置服务器证书 和 服务器私钥 到 OPENSSL ctx上下文句柄中 */
					server_setup_certs(ctx, certificate_file, private_key);

					/* 使我们创建好的evhttp句柄 支持SSL加密。实际上，加密的动作和解密的动作都已经帮我们自动完成，我们拿到的数据就已经解密之后的 */
					evhttp_set_bevcb(httpd, bevcb, ctx);
					//------OPENSSL------
				}

				r = evhttp_accept_socket(httpd, nfd);//使http server可以接受来自指定的socket的连接，可重复调用来绑定到不同的socket
				if (r != 0)
				{
					_debug_to(loger_httpserver, 1, ("start http server failed,error:accept socket failed\n"));
					stop_failed_server(i);
					evhttp_free(httpd);
					event_base_free(base);
					return -1;
				}
				evhttp_set_timeout(httpd, 60);//为一个http请求设置超时时间 以秒为单位，最大60秒
				//evhttp_set_gencb(httpd, pFunc, this);
				evhttp_set_gencb(httpd, pFunc, &pSeverThread[i]);//设置请求处理回调方法

				//evhttp_set_max_body_size(httpd, 65536);

				//DWORD dwThread;
				pSeverThread[i].threadhandle = ::CreateThread(nullptr, 1024, GlobalHttpBaseFunc, &pSeverThread[i], 0, &pSeverThread[i].threadid);

				//CreateThread 第一个参数 安全属性，第二个参数 栈空间大小 ，第三个参数 线程函数 ，传入的参数，是否创建完毕即可调度，保存线程ID
				//返回新线程的句柄
				if (pSeverThread[i].threadhandle == nullptr)
				{
					_debug_to(loger_httpserver, 1, ("start http server failed,error:create thread failed\n"));
					stop_failed_server(i);
					evhttp_free(httpd);
					event_base_free(base);
					return -1;
				}
			}
			return 0;
		}
		catch (...)
		{
			_debug_to(loger_httpserver, 1, ("start http server failed,error:exception\n"));
			return -1;
		}
		return 0;
	}

	int complex_httpServer::start_http_server(http_cb_Func _pFunc, HWND _mainWindow, int _port, int httpthreads, int nbacklog, std::string _ipaddr)
	{
		port = _port;
		nthreads = httpthreads; //100
		backlog = nbacklog; //1024*100

		if (port < 1 || port > 65535) port = 8130;

        if (!globalIsIPStringValid(_ipaddr))//判断ip是否合法
		{
			ipaddr = ("0.0.0.0");
		}
		else
		{
			ipaddr = _ipaddr;
		}
		pFunc = _pFunc;
		mainWindow = _mainWindow;
		std::string http_addr;	
		http_addr = ipaddr;

		if (start_http_server() == 0)
			return 0;
		else return -1;
	}

	void complex_httpServer::stop_failed_server(int threadcount)
	{
		if (threadcount < 1)
			return;

		try
		{
			if (pSeverThread)
			{
                _debug_to(loger_httpserver, 1, ("stop_failed_server begin\n"));
				int i = threadcount - 1;
				while (i >= 0)
				{
					if (pSeverThread[i].event_base)
					{
						event_base_loopbreak(pSeverThread[i].event_base);
					}
					i--;
				}

				i = threadcount - 1;
				while (i >= 0)
				{
					if (pSeverThread[i].threadhandle)
					{
						if (::WaitForSingleObject(pSeverThread[i].threadhandle, 1000) != WAIT_OBJECT_0)
						{
							try
							{
								::TerminateThread(pSeverThread[i].threadhandle, 0);
							}
							catch (...)
							{
								;
							}
                            _debug_to(loger_httpserver, 1, ("stop_failed_server timeout ,terminate thread\n"));
						}
						::CloseHandle(pSeverThread[i].threadhandle);
					}
					i--;
				}
				i = threadcount - 1;
				while (i >= 0)
				{
					if (pSeverThread[i].event_base)
					{
						//event_base_loopbreak(pSeverThread[i].event_base);
						event_base_free(pSeverThread[i].event_base);
                        pSeverThread[i].event_base = nullptr;
					}
					i--;
				}
				WSACleanup();
				delete[]pSeverThread;
                pSeverThread = nullptr;
                _debug_to(loger_httpserver, 1, ("stop failed server finish\n"));
			}
		}
		catch (...)
		{
			;;
		}
	}

	void complex_httpServer::stop_http_server()
	{
		try
		{
			if (pSeverThread)
			{
                _debug_to(loger_httpserver, 1, ("stop http sever begin\n"));
				int i = nthreads - 1;
				while (i >= 0)
				{
					if (pSeverThread[i].event_base)
					{
						event_base_loopbreak(pSeverThread[i].event_base);
						evthread_make_base_notifiable(pSeverThread[i].event_base);
					}
					i--;
				}
				i = nthreads - 1;
                _debug_to(loger_httpserver, 1, ("stop http sever base finished\n"));
				while (i >= 0)
				{
					if (pSeverThread[i].threadhandle)
					{
						if (::WaitForSingleObject(pSeverThread[i].threadhandle, 10000) != WAIT_OBJECT_0)
						{
							try
							{
								::TerminateThread(pSeverThread[i].threadhandle, 0);
							}
							catch (...)
							{
								;
							}
                            _debug_to(loger_httpserver, 1, ("stop http server timeout ,terminate thread\n"));
						}
						::CloseHandle(pSeverThread[i].threadhandle);
					}
					i--;
				}
                _debug_to(loger_httpserver, 1, ("stop http sever thread finished\n"));
				i = nthreads - 1;
				while (i >= 0)
				{
					if (pSeverThread[i].event_base)
					{
						//event_base_loopbreak(pSeverThread[i].event_base);
						event_base_free(pSeverThread[i].event_base);
                        pSeverThread[i].event_base = nullptr;
					}
					i--;		
				}
                _debug_to(loger_httpserver, 1, ("stop http sever event base release finished\n"));
				if (nfd > 0)
				{
					shutdown(nfd, SD_BOTH);
					Sleep(100);
				}
				WSACleanup();
                mainWindow = nullptr;
				delete[]pSeverThread;
                pSeverThread = nullptr;
                _debug_to(loger_httpserver, 1, ("stop http server finish\n"));
			}
		}
		catch (...)
		{
			;
		}

        if (http_checkEvent[0] != nullptr)
		{
			::CloseHandle(http_checkEvent[0]);
            http_checkEvent[0] = nullptr;
		}

        if (http_checkEvent[1] != nullptr)
		{
			::CloseHandle(http_checkEvent[1]);
            http_checkEvent[1] = nullptr;
		}
	}
};

#else //SYS-LINUX

namespace httpServer
{
    void* GlobalHttpBaseFunc(void * lpParam)
    {
        httpThread *ptd = reinterpret_cast<httpThread*>(lpParam);
		event_base_dispatch(ptd->eventbase);//等待事件被触发，然后调用它们的回调函数
		evhttp_free(ptd->httpserver);//释放以前创建的HTTP服务器
        ptd->httpserver = nullptr;
        return nullptr;
	}

    int httpserver_bindsocket(int port, int backlog)
    {
        int nsfd = socket(AF_INET, SOCK_STREAM, 0);//创建套接字
		if (nsfd < 0) return -1;

		int one = 1;
        socklen_t intLen = static_cast<socklen_t>(sizeof(int));
        int r = setsockopt(nsfd, SOL_SOCKET, SO_REUSEADDR, static_cast<const void*>(&one), intLen);//设置套接字的SO_REUSEADDR属性 执行正确返回0

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
        addr.sin_family = static_cast<unsigned short>(AF_INET);
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(static_cast<unsigned short>(port));

        ULONG tempSize = sizeof(addr);
        r = bind(nsfd, reinterpret_cast<struct sockaddr*>(&addr), static_cast<socklen_t>(tempSize));
        if (r < 0) {
            ::close(nsfd);
            return -1;
        }
		r = listen(nsfd, backlog);
        if (r < 0) {
            ::close(nsfd);
            return -1;
        }
		int flags = fcntl(nsfd, F_GETFL, 0);
        if (fcntl(nsfd, F_SETFL, flags | O_NONBLOCK) < 0) {//允许非阻塞模式
            _debug_to(loger_httpserver, 1, ("http server fcntl fail\n"));
        }
		return nsfd;
	}

	int complex_httpServer::starthttpserver()
	{
        pSeverThread = new struct httpThread[static_cast<ULONG>(nthreads)];
		evthread_use_pthreads();//开启多线程功能
		int nfd = httpserver_bindsocket(port, backlog);
		if (nfd < 0)
		{
			_debug_to(loger_httpserver, 1, ("httpserver_bindsocket\n"));
			return -1;
		}
		for (int i = 0; i < nthreads; i++)
		{
			struct event_base *base = event_base_new();
            if (base == nullptr)
			{
				_debug_to(loger_httpserver, 1, ("event_base_new\n"));
				return -1;
			}
			pSeverThread[i].eventbase = base;
			struct evhttp *httpd = evhttp_new(base);
            if (httpd == nullptr)
			{
				_debug_to(loger_httpserver, 1, ("evhttp_new\n"));
				return -1;
			}
			pSeverThread[i].httpserver = httpd;
			int r = evhttp_accept_socket(httpd, nfd);
			if (r != 0)
			{
				_debug_to(loger_httpserver, 1, ("evhttp_accept_socket\n"));
				return 0;
			}
			evhttp_set_timeout(httpd, 10);
			evhttp_set_gencb(httpd, pFunc, &pSeverThread[i]);
            r = pthread_create(&pSeverThread[i].ths, nullptr, GlobalHttpBaseFunc, &pSeverThread[i]);
			if (r != 0)
			{
				_debug_to(loger_httpserver, 1, ("pthread_create\n"));
				return -1;
			}
			//threadt[i] = pSeverThread[i].ths;
		}
        //for (int i = 0; i < nthreads; i++)
        //{
        //    pthread_join(pSeverThread[i].ths, nullptr);
        //}
		return 0;

	}

    int complex_httpServer::start_http_server(http_cb_Func _pFunc, void* _mainWindow, int _port, int httpthreads, int nbacklog, std::string _ipaddr)
	{
		pFunc = _pFunc;
		mainWindow = _mainWindow;
		port = _port;
		nthreads = httpthreads;
        if (nthreads < 1 || nthreads > 1000) nthreads = 10;
		backlog = nbacklog;

        if (!globalIsIPStringValid(_ipaddr))//判断ip是否合法
        {
            ipaddr = ("0.0.0.0");
        }
        else
        {
            ipaddr = _ipaddr;
        }

		if (starthttpserver() == 0)
			return 0;
        else
            return -1;
	}

	void complex_httpServer::stop_http_server()
	{
        /*if (pSeverThread)
		{
			int i = nthreads - 1;
			while (i >= 0)
			{
				if (pSeverThread[i].eventbase)
				{
					event_base_loopbreak(pSeverThread[i].eventbase);
					evthread_make_base_notifiable(pSeverThread[i].eventbase);
				}
				i--;
			}

			i = nthreads - 1;
			while (i >= 0)
			{
				if (pSeverThread[i].eventbase)
				{
					event_base_free(pSeverThread[i].eventbase);
                    pSeverThread[i].eventbase = nullptr;
				}
				i--;
			}
			if (nfd > 0)
			{
				shutdown(nfd, SHUT_RDWR);
				close(nfd);
				sleep(100);
			}

			delete[]pSeverThread;
            pSeverThread = nullptr;
        }*/
        try
        {
            if (pSeverThread)
            {
                _debug_to(loger_httpserver, 1, ("stop http sever begin\n"));
                int i = nthreads - 1;
                while (i >= 0)
                {
                    if (pSeverThread[i].eventbase)
                    {
                        event_base_loopbreak(pSeverThread[i].eventbase);
                        evthread_make_base_notifiable(pSeverThread[i].eventbase);
                    }
                    i--;
                }
                i = nthreads - 1;
                _debug_to(loger_httpserver, 1, ("stop http sever base finished\n"));
                while (i >= 0)
                {
                    pthread_join(pSeverThread[i].ths, nullptr);
                    i--;
                }
                _debug_to(loger_httpserver, 1, ("stop http sever thread finished\n"));
                i = nthreads - 1;
                while (i >= 0)
                {
                    if (pSeverThread[i].eventbase)
                    {
                        event_base_free(pSeverThread[i].eventbase);
                        pSeverThread[i].eventbase = nullptr;
                    }
                    i--;
                }
                _debug_to(loger_httpserver, 1, ("stop http sever event base release finished\n"));
                if (nfd > 0)
                {
                    shutdown(nfd, SHUT_RDWR);
                    close(nfd);
                    sleep(100U);
                }
                mainWindow = nullptr;
                delete[]pSeverThread;
                pSeverThread = nullptr;
                _debug_to(loger_httpserver, 1, ("stop http server finish\n"));
            }
        }
        catch (...)
        {
            ;
        }
	}
}
#endif
