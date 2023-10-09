#include "mmRabbitmq.h"
#include "public.h"

#include <iostream>
#include <map>
#define ssize_t mq_ssize_t //ssize_t和hiredis模块冲突
#include "amqp.h"
#undef ssize_t
#include "amqp_tcp_socket.h"
#pragma comment(lib,"librabbitmq.4.lib")

//custom loger
static FileWriter loger_rabbitmq("rabbitmq.log");

namespace nsRabbitmq
{
int rabbitmq_error(amqp_rpc_reply_t x, std::string memo, char const *context)
{
    switch (x.reply_type) {
    case AMQP_RESPONSE_NORMAL:
        return 0;
    case AMQP_RESPONSE_NONE:
        _debug_to(loger_rabbitmq, 1, ("rabbitmq[%s] %s missing RPC reply type\n"), memo.c_str(), context);
        break;
    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
        _debug_to(loger_rabbitmq, 1, ("rabbitmq[%s] [LIBRARY_EXCEPTION] %s:%s\n"), memo.c_str(), context, amqp_error_string2(x.library_error));
        break;
    case AMQP_RESPONSE_SERVER_EXCEPTION:
        switch (x.reply.id) {
        case 0x000A0032 : {//AMQP_CONNECTION_CLOSE_METHOD
            amqp_connection_close_t *m = reinterpret_cast<amqp_connection_close_t*>(x.reply.decoded);
            _debug_to(loger_rabbitmq, 1, ("rabbitmq[%s] [SERVER_EXCEPTION] %s: server connection error %uh, message: %.*s\n"),
                      memo.c_str(),
                      context,
                      m->reply_code,
                      static_cast<int>(m->reply_text.len),
                      reinterpret_cast<char*>(m->reply_text.bytes));
            break;
        }
        case 0x00140028 : { //AMQP_CHANNEL_CLOSE_METHOD
            amqp_channel_close_t *m = reinterpret_cast<amqp_channel_close_t*>(x.reply.decoded);
            _debug_to(loger_rabbitmq, 1, ("rabbitmq[%s][SERVER_EXCEPTION] %s: server channel error %uh, message: %.*s\n"),
                      memo.c_str(),
                      context,
                      m->reply_code,
                      static_cast<int>(m->reply_text.len),
                      reinterpret_cast<char*>(m->reply_text.bytes));
            break;
        }
        default:
            _debug_to(loger_rabbitmq, 1, ("rabbitmq[%s] %s [SERVER_EXCEPTION] unknown server error, method id is 0x%08X\n"), memo.c_str(), context, x.reply.id);
            break;
        }
        break;
    }
    return -1;
}

bool rabbitmq_destory(amqp_connection_state_t rabbitmq_conn, std::string memo)
{
    _debug_to(loger_rabbitmq, 1, ("rabbitmq[%s] try to disconnect\n"), memo.c_str());
    bool returnFlag = true;
    if (nullptr != rabbitmq_conn) {
        if (rabbitmq_error(amqp_connection_close(rabbitmq_conn, AMQP_REPLY_SUCCESS), memo, "Closing connection") != AMQP_STATUS_OK) {
            returnFlag = false;
            _debug_to(loger_rabbitmq, 1, ("rabbitmq[%s] try to disconnect amqp_connection_close error\n"), memo.c_str());
        }
        if (amqp_destroy_connection(rabbitmq_conn) != AMQP_STATUS_OK) {
            returnFlag = false;           
            _debug_to(loger_rabbitmq, 1, ("rabbitmq[%s] try to disconnectamqp_destroy_connection error\n"), memo.c_str());
        }
    }
    return returnFlag;
}

bool rabbitmq_initial(amqp_connection_state_t *rabbitmq_conn, amqp_socket_t **p_rabbitmq_socket, std::string memo,
                      std::string host, int port, std::string user, std::string pwd)
{
    _debug_to(loger_rabbitmq, 1, ("rabbitmq[%s] try to connect\n"), memo.c_str());
    *rabbitmq_conn = amqp_new_connection();
    if (nullptr == *rabbitmq_conn) {
        _debug_to(loger_rabbitmq, 1, ("rabbitmq[%s] new connection failed\n"), memo.c_str());
        return false;
    }
    *p_rabbitmq_socket = amqp_tcp_socket_new(*rabbitmq_conn);
    if (nullptr == *p_rabbitmq_socket) {
        _debug_to(loger_rabbitmq, 1, ("rabbitmq[%s] new socket failed\n"), memo.c_str());

        amqp_destroy_connection(*rabbitmq_conn);
        *rabbitmq_conn = nullptr;
        return false;
    }

    int status = amqp_socket_open(*p_rabbitmq_socket, host.c_str(), port);
    if (status != AMQP_STATUS_OK) {
        _debug_to(loger_rabbitmq, 1, ("rabbitmq[%s] socket open failed,return %d\n"), memo.c_str(), status);

        amqp_destroy_connection(*rabbitmq_conn);
        *rabbitmq_conn = nullptr;
        return false;
    }

    if (0 != rabbitmq_error(amqp_login(*rabbitmq_conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user.c_str(), pwd.c_str()), memo, "logging in")) {
        int nerror = ::GetLastError();
        rabbitmq_destory(*rabbitmq_conn, memo);
        *rabbitmq_conn = nullptr;
        return false;
    }
    _debug_to(loger_rabbitmq, 1, ("rabbitmq[%s] try to connect success\n"), memo.c_str());
    return true;
}

bool rabbitmq_publish_initial(amqp_connection_state_t rabbitmq_conn, amqp_channel_t channelId)
{
    _debug_to(loger_rabbitmq, 1, ("rabbitmq try to init send channel\n"));
    amqp_channel_open(rabbitmq_conn, channelId);
    if(0 != rabbitmq_error(amqp_get_rpc_reply(rabbitmq_conn), ("sender device"), "open channel")) {
        _debug_to(loger_rabbitmq, 1, ("sender device open channel failed\n"));
        amqp_channel_close(rabbitmq_conn, channelId, AMQP_REPLY_SUCCESS);
        return false;
    }
    _debug_to(loger_rabbitmq, 1, ("rabbitmqtry to init send channel success\n"));
    return true;
}

bool rabbitmq_consume_initial(amqp_connection_state_t rabbitmq_conn, amqp_channel_t channelId, std::string exchangename, std::string queuename, std::string routkey)
{
    std::string show_exchangename, show_queuename, show_routkey;
#if defined WIN32
    utf8_to_ansi(exchangename.c_str(), exchangename.length(), show_exchangename);//打印日志时需要anis，转换一下吧
    utf8_to_ansi(queuename.c_str(), queuename.length(), show_queuename);//打印日志时需要anis，转换一下吧
    utf8_to_ansi(routkey.c_str(), routkey.length(), show_routkey);//打印日志时需要anis，转换一下吧
#else
    show_exchangename = exchangename;
    show_queuename = queuename;
    show_routkey = routkey;
#endif
    _debug_to(loger_rabbitmq, 1, ("rabbitmq try to init recv channel\n"));
    amqp_channel_open(rabbitmq_conn, channelId);
    if(0 != rabbitmq_error(amqp_get_rpc_reply(rabbitmq_conn), ("recver device"), "open channel")) {
        _debug_to(loger_rabbitmq, 1, ("recver device open channel failed\n"));
        amqp_channel_close(rabbitmq_conn, channelId, AMQP_REPLY_SUCCESS);
        return false;
    }

    //声明要使用的队列并绑定
    amqp_bytes_t _queue = amqp_cstring_bytes(queuename.c_str());
    amqp_queue_declare(rabbitmq_conn, channelId, _queue, 0, 0, 0, 1, amqp_empty_table);
    if(0 != rabbitmq_error(amqp_get_rpc_reply(rabbitmq_conn), ("recver device"), "queue declare")) {
        _debug_to(loger_rabbitmq, 1, ("recver device define queue %s failed"), show_queuename.c_str());
        amqp_channel_close(rabbitmq_conn, channelId, AMQP_REPLY_SUCCESS);
        return false;
    }

    amqp_bytes_t _exchange = amqp_cstring_bytes(exchangename.c_str());
    amqp_bytes_t _routkey = amqp_cstring_bytes(routkey.c_str());
    amqp_queue_bind(rabbitmq_conn, channelId, _queue, _exchange, _routkey, amqp_empty_table);
    if(0 != rabbitmq_error(amqp_get_rpc_reply(rabbitmq_conn), ("recver device"), "queue bind")) {
        _debug_to(loger_rabbitmq, 1, ("recver device bind queue %s to exchange %s failed，key=%s\n"), show_queuename.c_str(), show_exchangename.c_str(), show_routkey.c_str());
        amqp_channel_close(rabbitmq_conn, channelId, AMQP_REPLY_SUCCESS);
        return false;
    }
    _debug_to(loger_rabbitmq, 1, ("recver device bind queue %s to exchange %s success，key=%s\n"), show_queuename.c_str(), show_exchangename.c_str(), show_routkey.c_str());

    amqp_basic_consume(rabbitmq_conn, channelId, _queue, amqp_empty_bytes, 0, 0, 0, amqp_empty_table);
    if (0 != rabbitmq_error(amqp_get_rpc_reply(rabbitmq_conn), ("recver device"), "consuming")) {
        _debug_to(loger_rabbitmq, 1, ("recver device start run error\n"));
        amqp_channel_close(rabbitmq_conn, channelId, AMQP_REPLY_SUCCESS);
        return false;
    }
    _debug_to(loger_rabbitmq, 1, ("rabbitmq try to init recv channel success\n"));
    return true;
}

cwRabbitmqPublish::cwRabbitmqPublish(std::string ip, int port, std::string user, std::string pwd, send_cb_Func func, void *data)
{
    m_ip = ip;
    m_port = port;
    m_user = user;
    m_pwd = pwd;
    m_pFunc = func;
    m_pData = data;

    m_channelId = 1;

    m_pRun = new cwRabbitmqDealThread();
    m_pRun->m_pThread = new boost::thread(&run, this);
}

cwRabbitmqPublish::~cwRabbitmqPublish()
{
    if (m_pRun){
        m_pRun->stop();
        delete m_pRun;
        m_pRun = nullptr;
    }
}

void cwRabbitmqPublish::send(mmRabbitmqData &data)
{
    mmRabbitmqData *pData = new mmRabbitmqData(data);

    nsShareMemory::mutex_lock(m_pRun->m_mutex);
    m_pRun->m_list.push_back(pData);
    nsShareMemory::mutex_unlock(m_pRun->m_mutex);
    nsShareMemory::semaphore_post(m_pRun->m_handle);
}

void cwRabbitmqPublish::run(cwRabbitmqPublish *pClient)
{
    if (pClient == nullptr || pClient->m_pRun == nullptr) return;
    _debug_to(loger_rabbitmq, 1, ("rabbitmq sender device thread start success\n"));

    bool rabbitmqFlag = false;
    //bool dealFlag = false;
    //尝试连接Rabbitmq
    amqp_connection_state_t rabbitmq_conn = nullptr;
    amqp_socket_t *p_rabbitmq_socket = nullptr;    
    std::map<std::string, int> exchangeMap;
    std::list<mmRabbitmqData*> list;
    amqp_channel_t channelId = static_cast<amqp_channel_t>(pClient->m_channelId);    
    std::chrono::steady_clock::time_point curCheckTime, preCheckTime, curSendTime, preSendTime;
    preSendTime = preCheckTime = std::chrono::steady_clock::now();
    int interval = 5;//每次间隔5秒重连，依次增加，最大1分钟
    std::string guidStr;globalCreateGUID(guidStr);

    try {
        if (rabbitmq_initial(&rabbitmq_conn, &p_rabbitmq_socket, ("sender device"), pClient->m_ip, pClient->m_port, pClient->m_user, pClient->m_pwd)) {
            rabbitmqFlag = rabbitmq_publish_initial(rabbitmq_conn, channelId);
        }
    } catch (...) {
        _debug_to(loger_rabbitmq, 1, ("rabbitmq publish thread initial throw exception\n"));
    }

    while (nsShareMemory::semaphore_timedwait(pClient->m_pRun->m_handle, 5000) >= 0) {
        if (!pClient->m_pRun->m_exitflag) {
            _debug_to(loger_rabbitmq, 1, ("rabbitmq sender device thread will exit\n"));
            break; //判断线程是否退出
        }

        if (!rabbitmqFlag) {
            curCheckTime = std::chrono::steady_clock::now();
            if (std::chrono::duration<double>(curCheckTime - preCheckTime).count() >= interval) {
                preCheckTime = curCheckTime;
                try {
                    if (rabbitmq_initial(&rabbitmq_conn, &p_rabbitmq_socket, ("sender device"), pClient->m_ip, pClient->m_port, pClient->m_user, pClient->m_pwd)) {
                        rabbitmqFlag = rabbitmq_publish_initial(rabbitmq_conn, channelId);
                    }
                } catch (...) {
                    _debug_to(loger_rabbitmq, 1, ("rabbitmq publish thread initial throw exception\n"));
                }
                if (rabbitmqFlag) {
                    interval = 5;
                } else {
                    if (interval < 60) interval +=5;
                    else if (interval > 60) interval = 60;//最多1分钟重连一次
                }
            }
        }

        nsShareMemory::mutex_lock(pClient->m_pRun->m_mutex);
        list.splice(list.begin(), pClient->m_pRun->m_list);
        nsShareMemory::mutex_unlock(pClient->m_pRun->m_mutex);


        if (list.size() < 1U) {
            curSendTime = std::chrono::steady_clock::now();
            if (std::chrono::duration<double>(curSendTime - preSendTime).count() >= 10) {
                preSendTime = curSendTime;
                mmRabbitmqData *pData = new mmRabbitmqData();
                pData->exchange = ("sdn.agent.heartbeat");//发送心跳的交换机和正常用的交换机区分开
                std::string routekey = "heartbeat.agent.";
                pData->routekey = routekey + guidStr;//正式使用，用GUID避免routekey重复产生错误
                //pData->routekey = "heartbeat.agent.test";//测试时使用，网页上查看消息是否成功发送
                pData->commandVector.push_back("heartbeat.agent");
                list.push_back(pData);
            }
        } else {
            preSendTime = std::chrono::steady_clock::now();
        }

        for (std::list<mmRabbitmqData*>::iterator it = list.begin(); it != list.end(); ++it)
        {
            mmRabbitmqData *pData = *it;
            if (pData == nullptr) continue;
            if (!rabbitmqFlag) {
                delete pData;
                continue;
            }

            try {             
                if (pData->exchange.empty()) {
                    if (pClient->m_pFunc) {
                        pClient->m_pFunc(pData, pClient->m_pData, false, ("exchange data is empty"));
                    }
                    delete pData;
                    continue;
                }

                /*amqp_channel_open(rabbitmq_conn, channelId);
                if(0 != rabbitmq_error(amqp_get_rpc_reply(rabbitmq_conn), ("sender device"), "open channel")) {
                    if (pClient->m_pFunc) {
                        pClient->m_pFunc(pData, pClient->m_pData, false, ("发布时打开通道失败"));
                    }
                    amqp_channel_close(rabbitmq_conn, channelId, AMQP_REPLY_SUCCESS);                   
                    if (rabbitmq_conn != nullptr) {
                        rabbitmq_destory(rabbitmq_conn, ("sender device"));
                        rabbitmq_conn = nullptr;
                    }
                    rabbitmqFlag = false;

                    delete pData;
                    continue;
                }*/

                amqp_bytes_t exchange = amqp_cstring_bytes(pData->exchange.c_str());
                std::map<std::string, int>::iterator map_it = exchangeMap.find(pData->exchange);
                if (map_it == exchangeMap.end()) {//添加交换机
                    std::string topic = "topic";
                    amqp_bytes_t amqp_type = amqp_cstring_bytes(topic.c_str());
                    int _passive= 0;
                    int _durable= 1;//交换机是否持久化
                    amqp_exchange_declare(rabbitmq_conn, channelId, exchange, amqp_type, _passive, _durable, 0, 0, amqp_empty_table);
                    if(0 != rabbitmq_error(amqp_get_rpc_reply(rabbitmq_conn), ("sender device"), "exchange declare")) {
                        if (pClient->m_pFunc) {
                            pClient->m_pFunc(pData, pClient->m_pData, false, ("add exchange failed"));
                        }
                        amqp_channel_close(rabbitmq_conn, channelId, AMQP_REPLY_SUCCESS);
                        if (rabbitmq_conn != nullptr) {
                            rabbitmq_destory(rabbitmq_conn, ("sender device"));
                            rabbitmq_conn = nullptr;
                        }
                        rabbitmqFlag = false;

                        delete pData;
                        continue;
                    }
                    exchangeMap[pData->exchange] = 1;
                }

                amqp_bytes_t routekey = amqp_cstring_bytes(pData->routekey.c_str());

                //发布消息
                //dealFlag = true;
                for (std::vector<std::string>::iterator sub_it = pData->commandVector.begin(); sub_it != pData->commandVector.end(); sub_it++)
                {
                    if ((*sub_it).empty()) continue;
                    amqp_bytes_t message_bytes;
                    message_bytes.len = (*sub_it).length();
                    message_bytes.bytes = const_cast<char*>((*sub_it).c_str());
                    if (AMQP_STATUS_OK != amqp_basic_publish(rabbitmq_conn, channelId, exchange, routekey, 0, 0, nullptr, message_bytes)) {
                        if (pClient->m_pFunc) {
                            pClient->m_pFunc(pData, pClient->m_pData, false, ("send messagge failed"));
                        }
                        rabbitmq_error(amqp_get_rpc_reply(rabbitmq_conn), ("sender device"), "amqp_basic_publish");//断开时发送并没有返回错误，所以不能用这个来判断
                        amqp_channel_close(rabbitmq_conn, channelId, AMQP_REPLY_SUCCESS);
                        if (rabbitmq_conn != nullptr) {
                            rabbitmq_destory(rabbitmq_conn, ("sender device"));
                            rabbitmq_conn = nullptr;
                        }
                        rabbitmqFlag = false;
                        break;
                        //if (0 != rabbitmq_error(amqp_get_rpc_reply(rabbitmq_conn), ("sender device"), "amqp_basic_publish")) {
                        //    amqp_channel_close(rabbitmq_conn, channelId, AMQP_REPLY_SUCCESS);
                        //    if (rabbitmq_conn != nullptr) {
                        //        rabbitmq_destory(rabbitmq_conn, ("sender device"));
                        //        rabbitmq_conn = nullptr;
                        //    }
                        //    rabbitmqFlag = false;
                        //    //dealFlag = false;
                        //    break;
                        //}
                    }
                }
                //if (dealFlag) {
                //    amqp_channel_close(rabbitmq_conn, channelId, AMQP_REPLY_SUCCESS);
                //}
                delete pData;
            } catch(...) {
                _debug_to(loger_rabbitmq, 1, ("rabbitmq publish thread deal throw exception\n"));
            }
        }
        list.clear();
    }

    if (rabbitmq_conn != nullptr) {
        amqp_channel_close(rabbitmq_conn, channelId, AMQP_REPLY_SUCCESS);
        rabbitmq_destory(rabbitmq_conn, ("sender device"));
        rabbitmq_conn = nullptr;
    }
}

cwRabbitmqConsume::cwRabbitmqConsume(std::string ip, int port, std::string user, std::string pwd, std::string queueName, std::string queueKey, std::string exchangeName, data_cb_Func func, void *data)
{
    m_ip = ip;
    m_port = port;
    m_user = user;
    m_pwd = pwd;
    m_pFunc = func;
    m_pData = data;
#if defined WIN32
    ansi_to_utf8(queueName.c_str(), queueName.length(), m_queueName);//rabbitmq只支持utf-8，所以要转换
    ansi_to_utf8(queueKey.c_str(), queueKey.length(), m_queueKey);//rabbitmq只支持utf-8，所以要转换
    ansi_to_utf8(exchangeName.c_str(), exchangeName.length(), m_exchangeName);//rabbitmq只支持utf-8，所以要转换
#else
    m_queueName = queueName;
    m_queueKey = queueKey;
    m_exchangeName = exchangeName;
#endif
    m_channelId = 2;

    m_pRun = new cwRabbitmqDealThread();
    m_pRun->m_pThread = new boost::thread(&run, this);
}

cwRabbitmqConsume::~cwRabbitmqConsume()
{
    if (m_pRun){
        m_pRun->stop();
        delete m_pRun;
        m_pRun = nullptr;
    }
}

void cwRabbitmqConsume::run(cwRabbitmqConsume *pClient)
{
    if (pClient == nullptr || pClient->m_pRun == nullptr) return;
    _debug_to(loger_rabbitmq, 1, ("rabbitmq recver device thread start success\n"));

    bool rabbitmqFlag = false;
    std::chrono::steady_clock::time_point curCheckTime, preCheckTime;
    preCheckTime = std::chrono::steady_clock::now();
    int interval = 5;//每次间隔5秒重连，依次增加，最大1分钟
    //尝试连接Rabbitmq
    amqp_connection_state_t rabbitmq_conn = nullptr;
    amqp_socket_t *p_rabbitmq_socket = nullptr;
    amqp_channel_t channelId = static_cast<amqp_channel_t>(pClient->m_channelId);
    try {
        if (rabbitmq_initial(&rabbitmq_conn, &p_rabbitmq_socket, ("recver device"), pClient->m_ip, pClient->m_port, pClient->m_user, pClient->m_pwd)) {
            rabbitmqFlag = rabbitmq_consume_initial(rabbitmq_conn, channelId, pClient->m_exchangeName, pClient->m_queueName, pClient->m_queueKey);
        }
    } catch (...) {
        _debug_to(loger_rabbitmq, 1, ("rabbitmq consume thread initial throw exception\n"));
    }
    struct timeval timeout = { 0, 500000 }; // 500毫秒
    while (1) {
        if (!rabbitmqFlag) {
            curCheckTime = std::chrono::steady_clock::now();
            if (std::chrono::duration<double>(curCheckTime - preCheckTime).count() >= interval) {
                preCheckTime = curCheckTime;
                try {
                    if (rabbitmq_initial(&rabbitmq_conn, &p_rabbitmq_socket, ("recver device"), pClient->m_ip, pClient->m_port, pClient->m_user, pClient->m_pwd)) {
                        rabbitmqFlag = rabbitmq_consume_initial(rabbitmq_conn, channelId, pClient->m_exchangeName, pClient->m_queueName, pClient->m_queueKey);
                    }
                } catch (...) {
                    _debug_to(loger_rabbitmq, 1, ("rabbitmq consume thread initial throw exception\n"));
                }
                if (rabbitmqFlag) {
                    interval = 5;
                } else {
                    if (interval < 60) interval +=5;
                    else if (interval > 60) interval = 60;//最多1分钟重连一次
                }
            }

            if (!rabbitmqFlag) {
                nsShareMemory::semaphore_timedwait(pClient->m_pRun->m_handle, 5000);
                if (!pClient->m_pRun->m_exitflag) {
                    _debug_to(loger_rabbitmq, 1, ("rabbitmq recver device thread will exit 1\n"));
                    break; //判断线程是否退出
                }
                continue;
            }
        }

        amqp_rpc_reply_t res;
        amqp_envelope_t envelope;
        amqp_maybe_release_buffers(rabbitmq_conn);

        res = amqp_consume_message(rabbitmq_conn, &envelope, &timeout, 0);

        if (!pClient->m_pRun->m_exitflag) {
            amqp_destroy_envelope(&envelope);
            _debug_to(loger_rabbitmq, 1, ("rabbitmq recver device thread will exit\n"));
            break; //判断线程是否退出
        }

        try {
            if (AMQP_RESPONSE_NORMAL != res.reply_type) {
                if (res.library_error == AMQP_STATUS_TIMEOUT) {
                    continue;
                } else {
                    amqp_destroy_envelope(&envelope);
                    amqp_channel_close(rabbitmq_conn, channelId, AMQP_REPLY_SUCCESS);
                    if (rabbitmq_conn != nullptr) {
                        rabbitmq_destory(rabbitmq_conn, ("recv device_1"));
                        rabbitmq_conn = nullptr;
                    }
                    rabbitmqFlag = false;
                    continue;
                }
            }

            _debug_to(loger_rabbitmq, 1, ("delivery %u, exchange %.*s routingkey %.*s\n"),
                     static_cast<unsigned>(envelope.delivery_tag),
                     static_cast<int>(envelope.exchange.len),
                     reinterpret_cast<char*>(envelope.exchange.bytes),
                     static_cast<int>(envelope.routing_key.len),
                     reinterpret_cast<char*>(envelope.routing_key.bytes));

            if (envelope.message.properties._flags & AMQP_BASIC_CONTENT_TYPE_FLAG) 
            {
                _debug_to(loger_rabbitmq, 1, ("Content-type: %.*s\n"), static_cast<int>(envelope.message.properties.content_type.len), reinterpret_cast<char *>(envelope.message.properties.content_type.bytes));
            }

            std::string routingStr(reinterpret_cast<char *>(envelope.routing_key.bytes), reinterpret_cast<char *>(envelope.routing_key.bytes) + envelope.routing_key.len);
            std::string messageStr(reinterpret_cast<char *>(envelope.message.body.bytes), reinterpret_cast<char *>(envelope.message.body.bytes) + envelope.message.body.len);

            if (pClient->m_pFunc != nullptr) {
                pClient->m_pFunc(pClient->m_pData, routingStr, messageStr);
            }
            amqp_basic_ack(rabbitmq_conn, channelId, envelope.delivery_tag, 1);
            amqp_destroy_envelope(&envelope);
        } catch (...) {
            _debug_to(loger_rabbitmq, 1, ("rabbitmq recver device thread catch...\n"));
        }
    }

    if (rabbitmq_conn != nullptr) {
        amqp_channel_close(rabbitmq_conn, channelId, AMQP_REPLY_SUCCESS);
        rabbitmq_destory(rabbitmq_conn, ("recver device"));
        rabbitmq_conn = nullptr;
    }
}

}
