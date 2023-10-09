#pragma once
#include <string>
#include <iostream>


#define SHAREMEMORY_COUNT 63
#define SHAREMEMORY_GUID_LEN 40
#define SHAREMEMORY_ID_LEN 40
#define SHAREMEMORY_TYPE_LEN 8
#define SHAREMEMORY_PARAM_LEN 160
//注意：要求是4096的整数倍，去掉前面固定的16个字节,4096-16
#define SHAREMEMORY_MORE_LEN 4080
//注意：要求是4096的整数倍，这里取的是4096，去掉前面固定的256个字节大小，4096-256
#define SHAREMEMORY_CONTENT_LEN 3840


#if defined WIN32
#include <Windows.h>
#define event_handle HANDLE
#define mutex_handle HANDLE
#define semaphore_handle HANDLE
#else
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
//#include <sys/eventfd.h>
//#include <sys/epoll.h>
//#include <unistd.h>
typedef struct
{
	bool state;
	bool manual_reset;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
        TCHAR path[MAX_PATH];
}event_t;
#define event_handle event_t*
#define mutex_handle sem_t*
#define semaphore_handle sem_t*
#endif // WIN32

struct mmShareMemoryBuf
{
    INT64                   m_Pos;//消息在共享内存中传输的序号
	char					m_Guid[SHAREMEMORY_GUID_LEN];//消息的唯一编号，用于反馈时对应
	char					m_Id[SHAREMEMORY_ID_LEN];//处理设备的Id
    char                    m_Type[SHAREMEMORY_TYPE_LEN];//消息的类型
	char					m_Param[SHAREMEMORY_PARAM_LEN];//消息的参数，以&分隔，例如：messageid=1001&overtime=5&paramid=CommonStatus
    char                    m_Content[SHAREMEMORY_CONTENT_LEN];//消息的内容
    mmShareMemoryBuf();
    mmShareMemoryBuf(const mmShareMemoryBuf& t);
    ~mmShareMemoryBuf();
    mmShareMemoryBuf& operator = (const mmShareMemoryBuf& t);
};

struct mmShareMemoryInfo
{
    INT64                   m_ReadPos;
    INT64                   m_WritePos;
    char                    m_More[SHAREMEMORY_MORE_LEN];//为了让结构体大小对齐到4096的整数倍添加
    mmShareMemoryBuf        m_Buf[SHAREMEMORY_COUNT];
    mmShareMemoryInfo();
};

//和mmShareMemoryBuf结构体对应，因为大部分情况下内容并不多，所以不用数组，节约空间
struct mmShareMemoryContent
{
    INT64                   m_Pos;
	std::string				m_Guid;
	std::string				m_Id;
    std::string             m_Type;
	std::string				m_Param;
    std::string             m_Content;
    mmShareMemoryContent();
    mmShareMemoryContent(const mmShareMemoryContent& t);
    mmShareMemoryContent(const mmShareMemoryBuf& t);
    ~mmShareMemoryContent();
    mmShareMemoryContent& operator = (const mmShareMemoryContent& t);
};

namespace nsShareMemory
{
	///////////////////////////////////////事件量跨平台处理函数///////////////////////////////////////
    //注：在linux多进程使用时存在问题，还未解决，暂时不适合跨进程使用，event_create函数不要传入name，不要使用event_open函数
    event_handle event_create(bool manual_reset, bool init_state, const TCHAR* name = nullptr);//创建事件，返回值：nullptr 出错
    event_handle event_open(const TCHAR* name);//打开已有的事件量，返回值：nullptr 出错
	int event_wait(event_handle hevent);//等待事件量，返回值：0 等到事件，-1出错
	int event_timedwait(event_handle hevent, long milliseconds);//等待事件量，超时退出，毫秒值为单位，返回值：0 等到事件，1 超时，-1出错
	int event_set(event_handle hevent);//设置事件量，返回值：0 成功，-1出错
	int event_reset(event_handle hevent);//重置事件量，返回值：0 成功，-1出错
    void event_destory(event_handle hevent);//销毁事件量，返回值：无
    ///////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////互斥量跨平台处理函数///////////////////////////////////////
    mutex_handle mutex_create(bool init_state, const TCHAR* name = nullptr);
	mutex_handle mutex_open(const TCHAR* name);
	bool mutex_lock(mutex_handle hmutex);
	bool mutex_unlock(mutex_handle hmutex);
    bool mutex_destory(mutex_handle hmutex, const TCHAR* name = nullptr);//销毁互斥量，返回值：true - 成功 false - 失败
    ///////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////信号量跨平台处理函数///////////////////////////////////////
    semaphore_handle semaphore_create(int count, const TCHAR* name = nullptr);//创建信号量，返回值：nullptr 出错
    semaphore_handle semaphore_open(const TCHAR* name);//打开已有的信号量，返回值：nullptr 出错
    int semaphore_wait(semaphore_handle hsemaphore);//等待信号量，返回值：0 等到信号，-1出错
    int semaphore_timedwait(semaphore_handle hsemaphore, long milliseconds);//等待信号量，超时退出，毫秒值为单位，返回值：0 等到信号，1 超时，-1出错，在linux下存在修改当前时间后出现异常行为的危险
    int semaphore_post(semaphore_handle hsemaphore);//信号量加1，返回值：0 成功，-1出错
    bool semaphore_destory(semaphore_handle hsemaphore, const TCHAR* name = nullptr);//销毁信号量，返回值：true - 成功 false - 失败
    ///////////////////////////////////////////////////////////////////////////////////////////////

    semaphore_handle semaphore_create_noname(int count);//创建无名信号

    ///////////////////////////////////////等待多个事件量的处理///////////////////////////////////////

/*#if defined WIN32
    class cwMultiEvent
    {
    private:
        HANDLE m_Event[2];
    public:
        cwMultiEvent(int count)
        {

        }
        ~cwMultiEvent()
        {

        }
        int waitfor();
        int waitfortimed(long milliseconds);
        void setEvent();
        void setSemaphore();
    };
#else
    class cwMultiEvent
    {
    private:
        int m_Event[2];
        int m_epoll_fd;
    public:
        cwMultiEvent(int count)
        {
            m_Event[0] = eventfd(0, EFD_SEMAPHORE);
            m_Event[1] = eventfd(count, EFD_SEMAPHORE);

            m_epoll_fd = epoll_create(16);

            struct epoll_event read_event;
            read_event.events = EPOLLIN;
            read_event.data.fd = m_Event[0];
            int ret = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_Event[0], &read_event);

            struct epoll_event read_event1;
            read_event1.events = EPOLLIN;
            read_event1.data.fd = m_Event[1];
            ret = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_Event[1], &read_event1);
            //if (ret < 0) {
            //    perror("epoll_ctl failed:");
            //}
        }
        ~cwMultiEvent()
        {
            if (m_epoll_fd > 0)
            {
                close(m_epoll_fd);
            }
            if (m_Event[0] != -1)
            {
                close(m_Event[0]);
            }
            if (m_Event[1] != -1)
            {
                close(m_Event[1]);
            }
        }
        int waitfor()
        {
            struct epoll_event events[16];
            int ret = epoll_wait(m_epoll_fd, events, 16, -1);
            if (ret > 0)
            {
                for (int i = 0; i < ret; i++)
                {
                    if (events[i].data.fd == m_Event[0])
                    {

                    }
                    else if (events[i].data.fd == m_Event[1])
                    {

                    }
                }
            }
        }
        //int waitfortimed(long milliseconds)
        //{
        //
        //}
        void setEvent()
        {
            if (m_Event[0] != -1)
            {
                write();
            }
        }
        void setSemaphore()
        {

        }
    };
#endif*/
    ////

#if defined WIN32
	template< typename KeyT > class CShareMem_WinX
	{
	public:
		CShareMem_WinX()
		{
            hMapping = nullptr;
            pAddress = nullptr;
			dwAccess = 0;
		}

		~CShareMem_WinX()
		{
			Delete();
		}

		inline int Create(KeyT Key, size_t Length)
		{
			//    检查参数
            if (nullptr != pAddress || nullptr != hMapping) return -1;

			//    调整长度为页的整数倍 ( 4096 * N )
			CONST ULONGLONG AlignToPage = 0x00000000000003FF;
			ULONGLONG Length64 = static_cast<ULONGLONG>(Length);
			Length64 = ((Length64 + AlignToPage) & (~AlignToPage));

			//    创建共享内存，在 64 位中大小可以超过 4 GB 
			HANDLE hFile = static_cast<HANDLE>(INVALID_HANDLE_VALUE);
			DWORD dwSizeLow = static_cast<DWORD>(Length64 & 0xFFFFFFFF);
			DWORD dwSizeHigh = static_cast<DWORD>((Length64 >> 32) & 0xFFFFFFFF);
            hMapping = CreateFileMapping(hFile, nullptr, PAGE_READWRITE, dwSizeHigh, dwSizeLow, Key);
            if (hMapping != nullptr && GetLastError() == ERROR_ALREADY_EXISTS)
			{
				// 已经存在
				CloseHandle(hMapping);
                hMapping = nullptr;
				return -2;
			}
            else if (nullptr == hMapping)
			{
				// 创建失败
				return -3;
			}

			//    映射内存
            pAddress = MapViewOfFileEx(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, Length, nullptr);
            if (nullptr != pAddress) { dwAccess = INT_MAX; return ERROR_SUCCESS; }

			CloseHandle(hMapping);
            hMapping = nullptr;
			return -4;
		}

		inline int Open(KeyT Key, bool Write = false)
		{
			//    检查参数
            if (nullptr != pAddress || nullptr != hMapping) return -1;

			//    打开共享内存
			dwAccess = Write ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;
			hMapping = OpenFileMapping(dwAccess, FALSE, Key);
            if (nullptr == hMapping) { dwAccess = 0; return -2; }

			//    映射内存
            pAddress = MapViewOfFileEx(hMapping, dwAccess, 0, 0, 0, nullptr);
            if (nullptr != pAddress) { dwAccess = INT_MAX; return ERROR_SUCCESS; }

			CloseHandle(hMapping);
            hMapping = nullptr;
			return -3;
		}

		inline int Delete()
		{
			//    检查参数
            if (nullptr == pAddress || nullptr == hMapping) return -1;

			//    删除共享
			UnmapViewOfFile(pAddress);
			CloseHandle(hMapping);
            pAddress = nullptr;
            hMapping = nullptr;
			dwAccess = 0;
			return 0;
		}

		inline int Close()
		{
			return Delete();
		}

		inline void * Address()
		{
			return pAddress;
		}

	public:
		static int Exists(KeyT Key)
		{
			CShareMem_WinX< KeyT > ShareMem;
			return ShareMem.Open(Key);
		}

		static int Remove(KeyT Key)
		{
            CShareMem_WinX< KeyT > ShareMem;
            if (ShareMem.Open(Key) == 0) return ShareMem.Close();
            else return -1;
		}

	protected:
        HANDLE      hMapping;
        LPVOID      pAddress;
        DWORD       dwAccess;
	};

#define SHARE_MEM_KEY_TYPE LPCTSTR
#define SHARE_MEM_TEMPLATE CShareMem_WinX
#define SHARE_MEM_KEY( name , key_1 , key_2 ) SHARE_MEM_KEY_TYPE name = TEXT( key_2 );
#else
    template< typename KeyT > class CShareMem_SysV
	{
    public:
		CShareMem_SysV()
		{
            pAddress = nullptr;
			hMapping = -1;
			nAccess = 0;
		}

		~CShareMem_SysV()
		{
			Delete();
		}

	public:
        inline int Create(KeyT Key, size_t nLength)//测试发现如果Key是固定值出现异常，容易导致共享内存无法创建
		{
			//    Check parameter
            if (nullptr != pAddress || -1 != hMapping)
            {
                return -1;
            }

			//    Get share memory id
			//int iFlags = SHM_R|SHM_W|IPC_CREAT|IPC_EXCL;
            hMapping = shmget(Key, nLength, 0644|IPC_CREAT);
            if (-1 == hMapping)
            {
                return -2;
            }

			// Set access mode
			//struct shmid_ds shmds;
			//shmctl(hMapping , IPC_STAT , &shmds );
			//shmds.shm_perm.mode |= 0x1B4; /* rw-rw-r-- */
			//shmctl(hMapping , IPC_SET , &shmds );

			// Attach share memory
            pAddress = static_cast<char*>(shmat(hMapping, nullptr, 0));

            if (reinterpret_cast<long>(pAddress) != -1 && nullptr != pAddress)
			{
				nAccess = -1;
				return 0;
			}
			//    Remove share memory id
            int hShare = hMapping;
            hMapping = -1;
            return shmctl(hShare, IPC_RMID, nullptr);
		}

		inline int Delete()
		{
			//    Check parameter
            if (nullptr == pAddress || -1 == hMapping)
            {
                return -1;
            }
            if (-1 != nAccess)
            {
                return Close();
            }

			// Detech and remove share memory
            shmdt(pAddress);
            pAddress = nullptr;

            shmctl(hMapping, IPC_RMID, nullptr);
            hMapping = -1;
            nAccess = 0;
			return 0;
		}

		inline int Open(KeyT Key, bool Write = false)
		{
			//    Check parameter
            if (nullptr != pAddress || -1 != hMapping)
            {
                return -1;
            }

			//    Open share memory
			nAccess = Write ? SHM_R : SHM_R | SHM_W;
			hMapping = shmget(Key, 0, nAccess);
			if (-1 == hMapping) { return -2; }

			//    Attach share memory
            pAddress = reinterpret_cast<char*>(shmat(hMapping, nullptr, 0));
            if (reinterpret_cast<long>(pAddress) != -1 && nullptr != pAddress)
            {
                return 0;
            }
            else
            {
                return -3;
            }
		}

		inline int Close()
		{
			//    Check Parameter
            if (nullptr == pAddress || -1 == hMapping)
            {
                return -1;
            }
            if (-1 == nAccess)
            {
                return Delete();
            }

			//    Share memory detech
            void * pAddr = pAddress;
            pAddress = nullptr;
            hMapping = -1;
            nAccess = 0;
			return shmdt(pAddr);
		}

		inline void* Address()
		{
            return static_cast<void*>(pAddress);
		}

	public:
		static int Exists(KeyT Key)
		{
			return shmget(Key, 0, SHM_R);
		}

		static int Remove(KeyT Key)
		{
			int hShare = shmget(Key, 0, SHM_R | SHM_W);
			if (-1 == hShare) { return 0; }

            return shmctl(hShare, IPC_RMID, nullptr);
		}

	protected:
		int        nAccess;
		int        hMapping;
        char*      pAddress;
	};
#define SHARE_MEM_KEY_TYPE key_t
#define SHARE_MEM_TEMPLATE nsShareMemory::CShareMem_SysV
#define SHARE_MEM_KEY( name , key_1 , key_2 ) enum { name = key_1 };
#endif

	typedef SHARE_MEM_TEMPLATE<SHARE_MEM_KEY_TYPE> textShareMemory;

	struct mmShareMemory
	{
		textShareMemory			m_Memory;
		size_t					m_Size;
        int                     m_Ret;//
        int                     m_Key;
		mutex_handle			m_Mutex;//用于共享内存同步保护
		semaphore_handle        m_Notify;//用于共享内存写通知
		bool					m_MainFlag;//如果是主程序，要负责删除，子程序不需要删除

        mmShareMemory(size_t size, const TCHAR* name, int key, bool Flag)
		{
			m_Size = size;
			m_MainFlag = Flag;
            m_Mutex = nullptr;
            m_Notify = nullptr;
            m_Ret = 0;
            m_Key = key;
			if (Flag)
			{               
#if defined WIN32
				m_Memory.Remove(name);
                m_Ret = m_Memory.Create(name, m_Size);
#else
				m_Memory.Remove(key);
                m_Ret = m_Memory.Create(static_cast<key_t>(key), m_Size);
#endif // WIN32			
                if (m_Ret == 0)
                {
                    char * pMemory = reinterpret_cast<char*>(m_Memory.Address());
                    memset(pMemory, 0, m_Size);
                }
				std::string mutexName = std::string(name) + ("_mutex");
				m_Mutex = mutex_create(true, mutexName.c_str());
				std::string notifyName = std::string(name) + ("_notify");
				m_Notify = semaphore_create(0, notifyName.c_str());
			}
			else
			{               
#if defined WIN32
                m_Ret = m_Memory.Open(name, true);
#else
                m_Ret = m_Memory.Open(static_cast<key_t>(key), true);
#endif // WIN32
				std::string mutexName = std::string(name) + ("_mutex");
				m_Mutex = mutex_open(mutexName.c_str());
				std::string notifyName = std::string(name) + ("_notify");
				m_Notify = semaphore_open(notifyName.c_str());
			}
            
		}

		~mmShareMemory()
		{			
            if (m_Mutex != nullptr)
			{
				mutex_destory(m_Mutex);
				m_Mutex = nullptr;
			}
            if (m_Notify != nullptr)
            {
				semaphore_destory(m_Notify);
                m_Notify = nullptr;
            }

            if (m_Ret == 0)
            {
                if (m_MainFlag) m_Memory.Delete();
                else m_Memory.Close();
            }
		}
	};
}


