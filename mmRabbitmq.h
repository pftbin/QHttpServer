//---------------------------------------------------------------------------
#pragma once
#include "shareMemory.h"
#include <boost/thread.hpp>
#include <boost/bind.hpp>

namespace nsData
{
    template <typename Key>
    class cwBaseThread
    {
    public:
        boost::thread* m_pThread;

        std::string m_handle_name;
        semaphore_handle m_handle;

        bool m_exitflag;

        std::string m_mutex_name;
        mutex_handle m_mutex;
        std::list<Key*> m_list;

    public:

        static std::string getTimeId()
        {
            std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            char buf[100] = { 0 };
            std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", std::localtime(&now));//24
            return buf;
        }

        cwBaseThread()
        {
            m_handle = nsShareMemory::semaphore_create_noname(0);
            m_mutex_name = getTimeId();
            m_mutex = nsShareMemory::mutex_create(true, m_mutex_name.c_str());
            m_exitflag = true;
            m_pThread = nullptr;
        }
        ~cwBaseThread()
        {
            if (m_pThread != nullptr)
            {
                //std::this_thread::sleep_for(std::chrono::milliseconds(10000));
                try {
                    m_pThread->join();
                }
                catch (boost::wrapexcept<boost::thread_resource_error>) {
                    ;
                }
                delete m_pThread;
                m_pThread = nullptr;
            }
            //nsShareMemory::semaphore_destory(m_handle, m_handle_name.c_str());
            nsShareMemory::semaphore_destory(m_handle);
            nsShareMemory::mutex_destory(m_mutex, m_mutex_name.c_str());
        }
        bool stop()
        {
            if (m_pThread != nullptr)
            {
                m_exitflag = false;
                return (nsShareMemory::semaphore_post(m_handle) == 0) ? true : false;
            }
            return false;
        }
        void clearList(std::list<Key*>& list)
        {
            if (list.empty())
                return;
            typename std::list<Key*>::iterator it;
            for (it = list.begin(); it != list.end(); ++it)
            {
                Key* pContent = *it;
                delete pContent;
            }
            list.clear();
        }
    };
};

namespace nsRabbitmq {
struct mmRabbitmqData 
{
    signed __int64 index;//命令的唯一序号
    std::string moreStr;
    int moreInt;
    std::string exchange;
    std::string routekey;
    std::vector<std::string> commandVector;//消息内容

    mmRabbitmqData()
    {
        index = 0;
        moreInt = 0;
    }
    mmRabbitmqData(const mmRabbitmqData& t)
    {
        *this = t;
    }
    mmRabbitmqData& operator=(const mmRabbitmqData& t)
    {
        index = t.index;
        moreStr = t.moreStr;
        moreInt = t.moreInt;
        exchange = t.exchange;
        routekey = t.routekey;
        commandVector = t.commandVector;
        return *this;
    }
};
typedef nsData::cwBaseThread<mmRabbitmqData> cwRabbitmqDealThread;
typedef void (*send_cb_Func)(mmRabbitmqData *, void *, bool, std::string); //发送返回的回调函数指针定义

class cwRabbitmqPublish 
{
public:
    cwRabbitmqPublish(std::string ip, int port, std::string user, std::string pwd, send_cb_Func func, void *data);
    ~cwRabbitmqPublish();
    void send(mmRabbitmqData &data); //发送数据
public:
    void                            *m_pData;//自定义数据指针
    send_cb_Func                    m_pFunc;//回调函数指针
private:
    cwRabbitmqDealThread            *m_pRun;//线程对象

    std::string                     m_ip;//rabbitmq的地址
    std::string                    m_user;//访问用户名
    std::string                    m_pwd;//访问密码
    int                             m_port;//rabbitmq的端口

    int                             m_channelId;//通道序号
private:
    static void run(cwRabbitmqPublish *pClient);
};

typedef void (*data_cb_Func)(void*, std::string, std::string); //接收到消息的回调

class cwRabbitmqConsume 
{
public:
    cwRabbitmqConsume(std::string ip, int port, std::string user, std::string pwd, std::string queueName, std::string queueKey, std::string exchangeName, data_cb_Func func, void *data);
    ~cwRabbitmqConsume();
public:
    void                            *m_pData;//自定义数据指针
    data_cb_Func                    m_pFunc;//回调函数指针
private:
    cwRabbitmqDealThread            *m_pRun;//线程对象

    std::string                     m_ip;//rabbitmq的地址
    std::string                    m_user;//访问用户名
    std::string                    m_pwd;//访问密码
    int                             m_port;//rabbitmq的端口

    int                             m_channelId;//通道序号
    std::string                     m_queueName;//队列名称
    std::string                     m_queueKey;//队列键
    std::string                     m_exchangeName;//队列绑定的交换机
private:
    static void run(cwRabbitmqConsume *pClient);
};
}
