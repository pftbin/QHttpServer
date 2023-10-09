#include "shareMemory.h"
#include "public.h"

mmShareMemoryContent::mmShareMemoryContent()
{
    m_Pos = 0;
}

mmShareMemoryContent::mmShareMemoryContent(const mmShareMemoryContent& t)
{
    *this = t;
}

mmShareMemoryContent::mmShareMemoryContent(const mmShareMemoryBuf& t)
{
    m_Pos = t.m_Pos;
	m_Guid = t.m_Guid;
	m_Id = t.m_Id;
	m_Type = t.m_Type;
	m_Param = t.m_Param;
    m_Content = t.m_Content;
}

mmShareMemoryContent::~mmShareMemoryContent()
{
}

mmShareMemoryContent& mmShareMemoryContent::operator = (const mmShareMemoryContent& t)
{
    m_Pos = t.m_Pos;
    m_Guid = t.m_Guid;
    m_Id = t.m_Id;
    m_Type = t.m_Type;
    m_Param = t.m_Param;
    m_Content = t.m_Content;
    return *this;
}

mmShareMemoryBuf::mmShareMemoryBuf()
{
    m_Pos = 0;
    memset(m_Guid, 0, sizeof(char) * SHAREMEMORY_GUID_LEN);
    memset(m_Id, 0, sizeof(char) * SHAREMEMORY_ID_LEN);
    memset(m_Type, 0, sizeof(char) * SHAREMEMORY_TYPE_LEN);
    memset(m_Param, 0, sizeof(char) * SHAREMEMORY_PARAM_LEN);
    memset(m_Content, 0, sizeof(char) * SHAREMEMORY_CONTENT_LEN);
}

mmShareMemoryBuf::mmShareMemoryBuf(const mmShareMemoryBuf& t)
{
    *this = t;
}

mmShareMemoryBuf::~mmShareMemoryBuf()
{
}

mmShareMemoryBuf& mmShareMemoryBuf::operator = (const mmShareMemoryBuf& t)
{
    m_Pos = t.m_Pos;
#if defined WIN32
    strncpy_s(m_Guid, SHAREMEMORY_GUID_LEN, t.m_Guid, SHAREMEMORY_GUID_LEN - 1);
    strncpy_s(m_Id, SHAREMEMORY_ID_LEN, t.m_Guid, SHAREMEMORY_ID_LEN - 1);
    strncpy_s(m_Type, SHAREMEMORY_TYPE_LEN, t.m_Type, SHAREMEMORY_TYPE_LEN - 1);
    strncpy_s(m_Param, SHAREMEMORY_PARAM_LEN, t.m_Param, SHAREMEMORY_PARAM_LEN - 1);
    strncpy_s(m_Content, SHAREMEMORY_CONTENT_LEN, t.m_Content, SHAREMEMORY_CONTENT_LEN - 1);
#else
    int tempLen = SHAREMEMORY_GUID_LEN - 1;    
    strncpy(m_Guid,  t.m_Guid, static_cast<ULONG>(tempLen));
    tempLen = SHAREMEMORY_ID_LEN - 1;
    strncpy(m_Id, t.m_Guid, static_cast<ULONG>(tempLen));
    tempLen = SHAREMEMORY_TYPE_LEN - 1;
    strncpy(m_Type, t.m_Type, static_cast<ULONG>(tempLen));
    tempLen = SHAREMEMORY_PARAM_LEN - 1;
    strncpy(m_Param, t.m_Param, static_cast<ULONG>(tempLen));
    tempLen = SHAREMEMORY_CONTENT_LEN - 1;
    strncpy(m_Content, t.m_Content, static_cast<ULONG>(tempLen));
#endif

    return *this;
}

mmShareMemoryInfo::mmShareMemoryInfo()
{
    m_ReadPos = 0;
    m_WritePos = 0;
    memset(m_More, 0, sizeof(char) * SHAREMEMORY_MORE_LEN);
}

#if defined WIN32
#include <iostream>
namespace nsShareMemory
{
    event_handle event_create(bool manual_reset, bool init_state, const TCHAR* name)
    {
        return CreateEvent(nullptr, manual_reset, init_state, name);
    }

    event_handle event_open(const TCHAR* name)
    {
        return OpenEvent(EVENT_ALL_ACCESS, false, name);
    }

    int event_wait(event_handle hevent)
    {
        DWORD ret = WaitForSingleObject(hevent, INFINITE);
        if (ret == WAIT_OBJECT_0)
        {
            return 0;
        }
        return -1;
    }

    int event_timedwait(event_handle hevent, long milliseconds)
    {
        DWORD ret = WaitForSingleObject(hevent, static_cast<DWORD>(milliseconds));
        if (ret == WAIT_OBJECT_0) {
            return 0;
        }
        if (ret == WAIT_TIMEOUT) {
            return 1;
        }
        return -1;
    }

    int event_set(event_handle hevent)
    {
        return !SetEvent(hevent);
    }

    int event_reset(event_handle hevent)
    {
        //ResetEvent 返回非零表示成功
        if (ResetEvent(hevent)) {
            return 0;
        }
        return -1;
    }

    void event_destory(event_handle hevent)
    {
        if (hevent != nullptr) CloseHandle(hevent);
    }

    mutex_handle mutex_create(bool init_state, const TCHAR* name)
    {
        return CreateMutex(nullptr, !init_state, name);
    }

    mutex_handle mutex_open(const TCHAR* name)
    {
        return OpenMutex(MUTEX_ALL_ACCESS, TRUE, name);
    }

    bool mutex_lock(mutex_handle hmutex)
    {
        //互斥锁创建失败
        if (nullptr == hmutex) return false;

        DWORD nRet = WaitForSingleObject(hmutex, INFINITE);
        if (nRet != WAIT_OBJECT_0)
        {
            return false;
        }
        return true;
    }

    bool mutex_unlock(mutex_handle hmutex)
    {
        if (nullptr == hmutex) return false;
        return ReleaseMutex(hmutex);
    }

    bool mutex_destory(mutex_handle hmutex, const TCHAR* name)
    {
        if (hmutex == nullptr) return false;
        if (name != nullptr) CloseHandle(hmutex);
        else CloseHandle(hmutex);
        return true;
    }

    semaphore_handle semaphore_create(int count, const TCHAR* name)
    {
        return CreateSemaphore(nullptr, count, 1024, name);
    }

    semaphore_handle semaphore_create_noname(int count)
    {
        return CreateSemaphore(nullptr, count, 1024, nullptr);
    }

    semaphore_handle semaphore_open(const TCHAR* name)
    {
        return OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, name);
    }

    int semaphore_wait(semaphore_handle hsemaphore)
    {
        if (hsemaphore == nullptr) return - 1;
        DWORD ret = WaitForSingleObject(hsemaphore, INFINITE);
        if (ret == WAIT_OBJECT_0) {
            return 0;
        }
        return -1;
    }

    int semaphore_timedwait(semaphore_handle hsemaphore, long milliseconds)
    {
        if (hsemaphore == nullptr) return - 1;
        DWORD ret = WaitForSingleObject(hsemaphore, static_cast<DWORD>(milliseconds));
        if (ret == WAIT_OBJECT_0) {
            return 0;
        }
        if (ret == WAIT_TIMEOUT) {
            return 1;
        }
        return -1;
    }

    int semaphore_post(semaphore_handle hsemaphore)
    {
        if (hsemaphore == nullptr) return - 1;
        return (ReleaseSemaphore(hsemaphore, 1, nullptr)) ? 0 : -1;
    }

    bool semaphore_destory(semaphore_handle hsemaphore, const TCHAR* name)
    {
        if (hsemaphore == nullptr) return false;
        if (name != nullptr) CloseHandle(hsemaphore);
        else CloseHandle(hsemaphore);
        return true;
    }
}
#else
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

namespace nsShareMemory
{
    event_handle event_create(bool manual_reset, bool init_state, const TCHAR* name)
    {
        if (name != nullptr)//命名的，建立一个共享内存，这种情况一般都是针对跨进程使用
        {
            int fd = open(name, O_CREAT|O_RDWR, 00777);
            if (fd < 0) return nullptr;
            ULONG eventSize = sizeof(event_t);
            ftruncate(fd, static_cast<long>(eventSize));
            event_handle hevent = reinterpret_cast<event_handle>(mmap(nullptr, sizeof(event_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0));
            close(fd);
            if (hevent == nullptr || reinterpret_cast<long>(hevent) < 0) return nullptr;
            hevent->state = init_state;
            hevent->manual_reset = manual_reset;

            memset(hevent->path, 0, sizeof(hevent->path));
            //size_t name_len = strlen(name) > (sizeof(hevent->path) - 1) ? (sizeof(hevent->path) - 1) : strlen(name);
            COMMON_STRCPY(hevent->path, name, sizeof(hevent->path) - 1);

            pthread_mutexattr_t mattr;
            pthread_mutexattr_init(&mattr);
            pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
            pthread_mutexattr_setrobust(&mattr, PTHREAD_MUTEX_ROBUST);

            if (pthread_mutex_init(&hevent->mutex, &mattr) != 0)
            {
                    delete hevent;
                    return nullptr;
            }
            if (pthread_cond_init(&hevent->cond, nullptr) != 0)
            {
                    pthread_mutex_destroy(&hevent->mutex);
                    delete hevent;
                    return nullptr;
            }
            return hevent;
        }
        else
        {
            event_handle hevent = new(std::nothrow) event_t;
            if (hevent == nullptr) return nullptr;
            hevent->state = init_state;
            hevent->manual_reset = manual_reset;

            memset(hevent->path, 0, sizeof(hevent->path));

            if (pthread_mutex_init(&hevent->mutex, nullptr) != 0)
            {
                delete hevent;
                return nullptr;
            }
            if (pthread_cond_init(&hevent->cond, nullptr) != 0)
            {
                pthread_mutex_destroy(&hevent->mutex);
                delete hevent;
                return nullptr;
            }
            return hevent;
        }
    }

    event_handle event_open(const TCHAR* name)
    {
        if (name == nullptr) return nullptr;
        int fd = open(name, O_RDWR, 0);
        if (fd < 0) return nullptr;
        event_handle hevent = reinterpret_cast<event_handle>(mmap(nullptr, sizeof(event_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        close(fd);
        if (hevent == nullptr || reinterpret_cast<long>(hevent) < 0) return nullptr;
        return hevent;
    }

    int event_wait(event_handle hevent)
    {
        int ret = pthread_mutex_lock(&hevent->mutex);
        //if (ret == EOWNERDEAD)//多进程异常退出会导致出现该值
        //{
        //	pthread_mutex_consistent(&hevent->mutex);
        //	ret = pthread_mutex_lock(&hevent->mutex);
        //}
        if (ret != 0)
        {
            return -1;
        }
        while (!hevent->state)
        {
            if (pthread_cond_wait(&hevent->cond, &hevent->mutex))
            {
                pthread_mutex_unlock(&hevent->mutex);
                return -1;
            }
        }
        if (!hevent->manual_reset)
        {
            hevent->state = false;
        }
        if (pthread_mutex_unlock(&hevent->mutex))
        {
            return -1;
        }
        return 0;
    }

    int event_timedwait(event_handle hevent, long milliseconds)
    {
        int rc = 0;
        struct timespec abstime;
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        abstime.tv_sec = tv.tv_sec + milliseconds / 1000;
        abstime.tv_nsec = tv.tv_usec * 1000 + (milliseconds % 1000) * 1000000;
        if (abstime.tv_nsec >= 1000000000)
        {
            abstime.tv_nsec -= 1000000000;
            abstime.tv_sec++;
        }

        int ret = pthread_mutex_lock(&hevent->mutex);
        //if (ret == EOWNERDEAD)//多进程异常退出会导致出现该值
        //{
        //	pthread_mutex_consistent(&hevent->mutex);
        //	ret = pthread_mutex_lock(&hevent->mutex);
        //}
        if (ret != 0)
        {
                return -1;
        }
        while (!hevent->state)
        {
            rc = pthread_cond_timedwait(&hevent->cond, &hevent->mutex, &abstime);
            if (rc == ETIMEDOUT || 0 == rc) break;
            pthread_mutex_unlock(&hevent->mutex);
            return -1;
        }
        if (rc == 0 && !hevent->manual_reset)
        {
            hevent->state = false;
        }
        if (pthread_mutex_unlock(&hevent->mutex) != 0)
        {
            return -1;
        }
        if (rc == ETIMEDOUT)
        {
            //timeout return 1
            return 1;
        }
        //wait event success return 0
        return 0;
    }

    int event_set(event_handle hevent)
    {
        int ret = pthread_mutex_lock(&hevent->mutex);
        //if (ret == EOWNERDEAD)//多进程异常退出会导致出现该值
        //{
        //	pthread_mutex_consistent(&hevent->mutex);
        //	ret = pthread_mutex_lock(&hevent->mutex);
        //}
        if (ret != 0)
        {
            return -1;
        }

        hevent->state = true;

        if (hevent->manual_reset)
        {
            if (pthread_cond_broadcast(&hevent->cond))
            {
                pthread_mutex_unlock(&hevent->mutex);
                return -1;
            }
        }
        else
        {
            if (pthread_cond_signal(&hevent->cond))
            {
                pthread_mutex_unlock(&hevent->mutex);
                return -1;
            }
        }

        if (pthread_mutex_unlock(&hevent->mutex) != 0)
        {
            return -1;
        }

        return 0;
    }

    int event_reset(event_handle hevent)
    {
        int ret = pthread_mutex_lock(&hevent->mutex);
        //if (ret == EOWNERDEAD)//多进程异常退出会导致出现该值
        //{
        //	pthread_mutex_consistent(&hevent->mutex);
        //	ret = pthread_mutex_lock(&hevent->mutex);
        //}
        if (ret != 0)
        {
            return -1;
        }

        hevent->state = false;

        if (pthread_mutex_unlock(&hevent->mutex) != 0)
        {
            return -1;
        }
        return 0;
    }

    void event_destory(event_handle hevent)
    {
        if (hevent == nullptr) return;
        pthread_cond_destroy(&hevent->cond);
        pthread_mutex_destroy(&hevent->mutex);
        if (strlen(hevent->path) > 0U) {
            TCHAR tempPath[MAX_PATH];
            memset(tempPath, 0, sizeof(TCHAR) * MAX_PATH);
            COMMON_STRCPY(tempPath, hevent->path, static_cast<ULONG>(MAX_PATH - 1));

            munmap(hevent, sizeof(event_t));
            try {
                if (remove(tempPath) != 0) {
                    _debug_to(1, ("event destory error:%s\n"), strerror(errno));
                }
            } catch (...) {
                ;
            }
        } else {
            delete hevent;
        }
    }

    mutex_handle mutex_create(bool init_state, const TCHAR* name)
    {
        int init_count = (init_state) ? 1 : 0;
        mutex_handle hmutex = sem_open(name, O_RDWR | O_CREAT, 0644, init_count);
        return (hmutex == SEM_FAILED) ? nullptr : hmutex;
    }

    mutex_handle mutex_open(const TCHAR* name)
    {
        if (name == nullptr) return nullptr;
        mutex_handle hmutex = sem_open(name, 0, 0, 0);
        return (hmutex == SEM_FAILED) ? nullptr : hmutex;
    }

    bool mutex_lock(mutex_handle hmutex)
    {
        if (nullptr == hmutex) return false;
        return (sem_wait(hmutex) == 0) ? true : false;
    }

    bool mutex_unlock(mutex_handle hmutex)
    {
        if (nullptr == hmutex) return false;
        return (sem_post(hmutex) == 0) ? true : false;
    }

    bool mutex_destory(mutex_handle hmutex, const TCHAR* name)
    {
        if (hmutex == nullptr) return false;
        int ret = sem_close(hmutex);
        if (0 != ret)
        {
            return false;
        }
        if (name != nullptr)
        {
            return (sem_unlink(name) == -1) ? false : true;
        }
        return true;
    }

    semaphore_handle semaphore_create(int count, const TCHAR* name)
    {
        if (count < 0) count = 0;
        std::string namestr;
        if (name == nullptr)
        {
            globalCreateGUID(namestr);
        }
        else {
            namestr = std::string(name);
        }
        semaphore_handle hsemaphore = sem_open(namestr.c_str(), O_RDWR | O_CREAT, 0644, count);
        return (hsemaphore == SEM_FAILED) ? nullptr : hsemaphore;
    }

    semaphore_handle semaphore_create_noname(int count)
    {
        if (count < 0) count = 0;
        semaphore_handle hsemaphore = new sem_t();
        if (hsemaphore == nullptr) return nullptr;
        if (sem_init(hsemaphore, 0, static_cast<UINT>(count)) == 0) {
            return hsemaphore;
        }
        else {
            sem_close(hsemaphore);
            return nullptr;
        }
    }

    semaphore_handle semaphore_open(const TCHAR* name)
    {
        if (name == nullptr) return nullptr;
        semaphore_handle hsemaphore = sem_open(name, 0, 0, 0);
        return (hsemaphore == SEM_FAILED) ? nullptr : hsemaphore;
    }

    int semaphore_wait(semaphore_handle hsemaphore)
    {
        if (nullptr == hsemaphore) return -1;
        return sem_wait(hsemaphore);
    }

    int semaphore_timedwait(semaphore_handle hsemaphore, long milliseconds)
    {
        if (nullptr == hsemaphore) return -1;
        struct timespec abstime;
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        abstime.tv_sec = tv.tv_sec + milliseconds / 1000;
        abstime.tv_nsec = tv.tv_usec * 1000 + (milliseconds % 1000) * 1000000;
        if (abstime.tv_nsec >= 1000000000)
        {
            abstime.tv_nsec -= 1000000000;
            abstime.tv_sec++;
        }
        int ret = sem_timedwait(hsemaphore, &abstime);
        if (ret == -1 && errno == ETIMEDOUT) return 1;
        else return ret;
    }

    int semaphore_post(semaphore_handle hsemaphore)
    {
        if (nullptr == hsemaphore) return -1;
        return sem_post(hsemaphore);
    }

    bool semaphore_destory(semaphore_handle hsemaphore, const TCHAR* name)
    {
        if (nullptr == hsemaphore) return false;
        int ret = sem_close(hsemaphore);
        if (0 != ret)
        {
            return false;
        }
        if (name != nullptr)
        {
            return (sem_unlink(name) == -1) ? false : true;
        }
        return true;
    }
}
#endif // WIN32
