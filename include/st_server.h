/*
 * Copyright (C) zhoulv2000@163.com
 */

#ifndef _ST_SERVER_H_INCLUDED_
#define _ST_SERVER_H_INCLUDED_

#include "st_util.h"
#include "st_manager.h"
#include "st_sys.h"
#include "st_netaddr.h"

ST_NAMESPACE_BEGIN

class StServerConnection : public StConnectionItem
{ 
public:
    virtual int Process()
    {
        int ret = RecvData();
        LOG_TRACE("ret: %d", ret);
        if (ret < 0)
        {
            return -1;
        }

        char buf[1024];
        sprintf(buf, "HTTP/1.1 200 OK\r\n"
            "Content-Length: 1\r\n"
            "Content-Type: text/html\r\n"
            "Date: Mon, 12 Aug 2019 15:48:13 GMT\r\n"
            "Server: sthread/1.0.1\r\n\r\n1");

        m_sendbuf_->SetBuffer(buf, strlen(buf) + 1);
        ret = SendData();
        LOG_TRACE("ret: %d", ret);

        if (ret < 0)
        {
            return -2;
        }

        return 0;
    }
}; 

template<typename ThreadT, 
    typename StEventItemT, 
    typename ConnetionT,
    int ServerT = TCP_SERVER>
class StServer
{
public:
    StServer() : 
        m_osfd_(-1), 
        m_item_(NULL)
    {
        m_osfd_ = -1;
        m_manager_ = GetInstance< Manager<ThreadT, StEventItemT> >();
    }

    ~StServer()
    {
        UtilPtrPoolFree(m_item_);
    }

    inline void SetHookFlag()
    {
        m_manager_->SetHookFlag();
    }

    int32_t CreateSocket(const StNetAddress &addr)
    {
        m_addr_ = addr;

        int protocol = SOCK_STREAM;
        if (ServerT == UDP_SERVER)
        {
            protocol = SOCK_DGRAM;
        }
        m_osfd_ = st_socket(addr.IsIPV6() ? AF_INET6 : AF_INET, protocol, 0);
        LOG_TRACE("m_osfd_: %d", m_osfd_);
        if (m_osfd_ < 0)
        {
            LOG_ERROR("create socket failed, ret[%d]", m_osfd_);
            return -1;
        }

        m_item_ = m_manager_->AllocStEventItem();
        ASSERT(m_item_ != NULL);

        m_item_->SetOsfd(m_osfd_);
        m_item_->EnableOutput();
        m_item_->DisableInput();
        GetEventScheduler()->Add(m_item_);

        struct sockaddr *servaddr;
        m_addr_.GetSockAddr(servaddr);
        if (::bind(m_osfd_, (struct sockaddr*)servaddr, sizeof(struct sockaddr)) < 0)
        {
            LOG_ERROR("bind socket error: %s(errno: %d)", strerror(errno), errno);
            return -2;
        }

        LOG_TRACE("addr: %s", m_addr_.IPPort());
        
        return m_osfd_;
    }

    bool Listen(int backlog = 128)
    {
        int r = ::listen(m_osfd_, backlog);
        return (r < 0) ? false : true;
    }

    void Loop()
    {
        ASSERT(m_manager_ != NULL);

        int connfd = -1;
        while (true)
        {
            struct sockaddr clientaddr;
            socklen_t addrlen = sizeof(struct sockaddr);
            if ((connfd = ::_accept(m_osfd_, (struct sockaddr*)&clientaddr, &addrlen)) <= 0)
            {
                LOG_TRACE("connfd: %d, errno: %d, errmsg: %s", 
                    connfd, errno, strerror(errno));
                continue;
            }

            StNetAddress addr(*((struct sockaddr_in*)&clientaddr));
            StServerConnection *conn = (StServerConnection*)(
                GetInstance< UtilPtrPool<ConnetionT> >()->AllocPtr()
            );
            conn->SetOsfd(connfd);
            conn->SetDestAddr(addr);
            
            LOG_TRACE("connfd: %d", connfd);
            m_manager_->CreateThread(NewClosure(CallBack, conn, this));
        }
    }

    static void CallBack(StServerConnection *conn,
        StServer<ThreadT, 
            StEventItemT, 
            ConnetionT, 
            ServerT> *server)
    {
        Manager<ThreadT, StEventItemT> *manager = server->m_manager_;
        ASSERT(manager != NULL);

        StEventItem *item = manager->AllocStEventItem();
        ASSERT(item != NULL);

        item->SetOsfd(conn->GetOsfd());

        do
        {
            item->EnableInput();
            item->DisableOutput();
            GetEventScheduler()->Add(item);
            LOG_TRACE(" ========== %p", item);
        } while (conn->Process() > 0);

        // 清理句柄数据
        GetEventScheduler()->Close(item);
        st_close(conn->GetOsfd());
        // UtilPtrPoolFree(item);
    }

private:
    Manager<ThreadT, StEventItemT>  *m_manager_;
    int             m_osfd_;
    StNetAddress    m_addr_;
    StEventItem     *m_item_;
};

ST_NAMESPACE_END

#endif