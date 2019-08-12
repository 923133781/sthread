#include "st_manager.h"

ST_NAMESPACE_USING

int _sendto(int fd, const void *msg, int len, int flags, 
    const struct sockaddr *to, int tolen, int timeout)
{
    Manager *manager = GetInstance<Manager>();
    int64_t start = manager->GetLastClock();
    Thread* thread = (Thread*)(manager->
        m_thread_scheduler_->GetActiveThread());

    LOG_TRACE("---------- [name : %s] -----------", thread->GetName());
    int64_t now = 0;
    timeout = (timeout <= -1) ? 0x7fffffff : timeout;

    int n = 0;
    while ((n = st_sendto(fd, msg, len, flags, to, tolen)) < 0)
    {
        if (n == 0) // 对端关闭
        {
            LOG_ERROR("[n=0]sendto failed, errno: %d, strerr : %s", 
                errno, strerror(errno));
            return 0;
        }

        // 判断是否超时
        now = manager->GetLastClock();
        if ((int)(now - start) > timeout)
        {
            errno = ETIME;
            return -1;
        }

        if (errno == EINTR) 
        {
            continue;
        }

        if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
        {
            LOG_ERROR("sendto failed, errno: %d, strerr : %s", 
                errno, strerror(errno));
            return -1;
        }

        StEventItem* item = manager->m_event_scheduler_->GetEventItem(fd);
        if (item == NULL)
        {
            LOG_ERROR("item is NULL");
            return -2;
        }
        item->EnableOutput();
        item->SetOwnerThread(thread);
        int64_t wakeup_timeout = timeout + manager->GetLastClock();
        if (!(manager->m_event_scheduler_->Schedule(thread, NULL, item, wakeup_timeout)))
        {
            LOG_ERROR("item schedule failed, errno: %d, strerr: %s", 
                    errno, strerror(errno));
            // 释放item数据
            manager->m_item_pool_.FreePtr(item);
            return -3;
        }
    }

    return n;
}

int _recvfrom(int fd, void *buf, int len, int flags, 
    struct sockaddr *from, socklen_t *fromlen, int timeout)
{
    Manager *manager = GetInstance<Manager>();
    int64_t start = manager->GetLastClock();
    Thread* thread = (Thread*)(manager->
        m_thread_scheduler_->GetActiveThread());

    LOG_TRACE("---------- [name : %s] -----------", thread->GetName());
    int64_t now = 0;
    timeout = (timeout <= -1) ? 0x7fffffff : timeout;

    while (true)
    {
        now = manager->GetLastClock();
        if ((int)(now - start) > timeout)
        {
            errno = ETIME;
            return -1;
        }

        StEventItem* item = manager->m_event_scheduler_->GetEventItem(fd);
        if (item == NULL)
        {
            LOG_ERROR("item is NULL");
            return -2;
        }
        item->EnableInput();
        item->SetOwnerThread(thread);
        int64_t wakeup_timeout = timeout + manager->GetLastClock();
        if (!(manager->m_event_scheduler_->Schedule(thread, NULL, item, wakeup_timeout)))
        {
            LOG_ERROR("item schedule failed, errno: %d, strerr: %s", 
                    errno, strerror(errno));
            // 释放item数据
            manager->m_item_pool_.FreePtr(item);
            return -3;
        }

        int n = st_recvfrom(fd, buf, len, flags, from, fromlen);
        LOG_TRACE("recvfrom return n: %d, buf: %s, fd: %d, len: %d, flags: %d", 
            n, buf, fd, len, flags);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
            {
                LOG_ERROR("recvfrom failed, errno: %d", errno);
                return -1;
            }
        }
        else if (n == 0) // 对端关闭
        {
            LOG_ERROR("[n=0]recvfrom failed, errno: %d", errno);
            return 0;
        }
        else
        {
            return n;
        }
    }
}

int _connect(int fd, const struct sockaddr *addr, int addrlen, int timeout)
{
    Manager *manager = GetInstance<Manager>();
    int64_t start = manager->GetLastClock();
    Thread* thread = (Thread*)(manager->
        m_thread_scheduler_->GetActiveThread());

    LOG_TRACE("---------- [name : %s] -----------", thread->GetName());
    int64_t now = 0;
    timeout = (timeout <= -1) ? 0x7fffffff : timeout;

    int n = 0;
    while ((n = st_connect(fd, addr, addrlen)) < 0)
    {
        LOG_TRACE("connect n : %d, errno : %d, strerror : %s", n, errno, strerror(errno));
        now = manager->GetLastClock();
        LOG_TRACE("now : %ld, start : %ld", now, start);
        if ((int)(now - start) > timeout)
        {
            errno = ETIME;
            return -1;
        }

        if (errno == EISCONN)
        {
            LOG_WARN("errno = EISCONN");
            return 0;
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (!((errno == EAGAIN) || (errno == EINPROGRESS)))
        {
            LOG_ERROR("connect failed, errno: %d, strerr: %s", 
                errno, strerror(errno));
            return -1;
        }

        StEventItem* item = manager->m_event_scheduler_->GetEventItem(fd);
        if (item == NULL)
        {
            LOG_ERROR("item is NULL");
            return -2;
        }
        item->EnableOutput();
        item->SetOwnerThread(thread);
        int64_t wakeup_timeout = timeout + manager->GetLastClock();
        if (!(manager->m_event_scheduler_->Schedule(thread, NULL, item, wakeup_timeout)))
        {
            LOG_ERROR("item schedule failed, errno: %d, strerr: %s", 
                    errno, strerror(errno));
            // 释放item数据
            manager->m_item_pool_.FreePtr(item);
            return -3;
        }
    }

    return n;
}

ssize_t _read(int fd, void *buf, size_t nbyte, int timeout)
{
    Manager *manager = GetInstance<Manager>();
    int64_t start = manager->GetLastClock();
    Thread* thread = (Thread*)(manager->
        m_thread_scheduler_->GetActiveThread());

    LOG_TRACE("---------- [name : %s] -----------", thread->GetName());
    int64_t now = 0;
    timeout = (timeout <= -1) ? 0x7fffffff : timeout;

    ssize_t n = 0;
    while ((n = st_read(fd, buf, nbyte)) < 0)
    {
        if (n == 0) // 句柄关闭
        {
            LOG_ERROR("[n=0]read failed, errno: %d", errno);
            return 0;
        }

        now = manager->GetLastClock();
        if ((int)(now - start) > timeout)
        {
            errno = ETIME;
            return -1;
        }

        if (errno == EINTR)
        {
            continue;
        }

        if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
        {
            LOG_ERROR("read failed, errno: %d", errno);
            return -1;
        }

        StEventItem* item = manager->m_event_scheduler_->GetEventItem(fd);
        if (item == NULL)
        {
            LOG_ERROR("item is NULL");
            return -2;
        }
        item->EnableInput();
        item->SetOwnerThread(thread);
        int64_t wakeup_timeout = timeout + manager->GetLastClock();
        if (!(manager->m_event_scheduler_->Schedule(thread, NULL, item, wakeup_timeout)))
        {
            LOG_ERROR("item schedule failed, errno: %d, strerr: %s", 
                    errno, strerror(errno));
            // 释放item数据
            manager->m_item_pool_.FreePtr(item);
            return -3;
        }
    }

    return n;
}

ssize_t _write(int fd, const void *buf, size_t nbyte, int timeout)
{
    Manager *manager = GetInstance<Manager>();
    int64_t start = manager->GetLastClock();
    Thread* thread = (Thread*)(manager->
        m_thread_scheduler_->GetActiveThread());

    LOG_TRACE("---------- [name : %s] -----------", thread->GetName());
    int64_t now = 0;
    timeout = (timeout <= -1) ? 0x7fffffff : timeout;

    ssize_t n = 0;
    size_t send_len = 0;
    while (send_len < nbyte)
    {
        now = manager->GetLastClock();
        if ((int)(now - start) > timeout)
        {
            errno = ETIME;
            return -1;
        }

        n = st_write(fd, (char*)buf + send_len, nbyte - send_len);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
            {
                LOG_ERROR("write failed, errno: %d", errno);
                return -1;
            }
        }
        else if (n == 0) // 已经关闭句柄
        {
            LOG_ERROR("[n=0]write failed, errno: %d", errno);
            return 0;
        }
        else
        {
            send_len += n;
            if (send_len >= nbyte)
            {
                return nbyte;
            }
        }

        StEventItem* item = manager->m_event_scheduler_->GetEventItem(fd);
        if (item == NULL)
        {
            LOG_ERROR("item is NULL");
            return -2;
        }
        item->EnableOutput();
        item->SetOwnerThread(thread);
        int64_t wakeup_timeout = timeout + manager->GetLastClock();
        if (!(manager->m_event_scheduler_->Schedule(thread, NULL, item, wakeup_timeout)))
        {
            LOG_ERROR("item schedule failed, errno: %d, strerr: %s", 
                    errno, strerror(errno));
            // 释放item数据
            manager->m_item_pool_.FreePtr(item);
            return -3;
        }
    }

    return nbyte;
}

int _recv(int fd, void *buf, int len, int flags, int timeout)
{
    Manager *manager = GetInstance<Manager>();
    int64_t start = manager->GetLastClock();
    Thread* thread = (Thread*)(manager->
        m_thread_scheduler_->GetActiveThread());

    LOG_TRACE("---------- [name: %s] -----------", thread->GetName());
    int64_t now = 0;
    timeout = (timeout <= -1) ? 0x7fffffff : timeout;

    while (true)
    {
        now = manager->GetLastClock();
        LOG_TRACE("now time: %ld, start time: %ld", now, start);
        if ((int)(now - start) > timeout)
        {
            errno = ETIME;
            return -1;
        }

        StEventItem* item = manager->m_event_scheduler_->GetEventItem(fd);
        if (item == NULL)
        {
            LOG_ERROR("item is NULL");
            return -2;
        }
        item->EnableInput();
        item->SetOwnerThread(thread);
        int64_t wakeup_timeout = timeout + manager->GetLastClock();
        if (!(manager->m_event_scheduler_->Schedule(thread, NULL, item, wakeup_timeout)))
        {
            LOG_ERROR("item schedule failed, errno: %d, strerr: %s", 
                    errno, strerror(errno));
            // 释放item数据
            manager->m_item_pool_.FreePtr(item);
            return -3;
        }

        int n = st_recv(fd, buf, len, flags);
        LOG_TRACE("recv return n: %d, buf: %s, fd: %d, len: %d, flags: %d", 
            n, buf, fd, len, flags);
        if (n < 0)
        {
            LOG_ERROR("recv failed, errno: %d, strerr: %s", errno, strerror(errno));
            if (errno == EINTR)
            {
                continue;
            }
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
            {
                return -1;
            }
        }
        else if (n == 0) // 对端关闭连接
        {
            LOG_ERROR("[n=0]recv failed, errno: %d, strerr: %s", errno, strerror(errno));
            return 0;
        }
        else
        {
            return n;
        }
    }
}

ssize_t _send(int fd, const void *buf, size_t nbyte, int flags, int timeout)
{
    Manager *manager = GetInstance<Manager>();
    int64_t start = manager->GetLastClock();
    Thread* thread = (Thread*)(manager->
        m_thread_scheduler_->GetActiveThread());

    LOG_TRACE("---------- [name : %s] -----------", thread->GetName());
    int64_t now = 0;
    timeout = (timeout <= -1) ? 0x7fffffff : timeout;

    ssize_t n = 0;
    size_t send_len = 0;
    while (send_len < nbyte)
    {
        now = manager->GetLastClock();
        if ((int)(now - start) > timeout)
        {
            errno = ETIME; // 超时请求
            return -1;
        }

        n = st_send(fd, (char*)buf + send_len, nbyte - send_len, flags);
        LOG_TRACE("send fd: %d, nbyte: %d, send_len: %d, flags: %d, n: %d", 
            fd, nbyte, send_len, flags, n);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
            {
                LOG_ERROR("write failed, errno: %d, strerr: %s", errno, strerror(errno));
                return -1;
            }
        }
        else if (n == 0) // 对端关闭连接
        {
            LOG_ERROR("[n=0]write failed, errno: %d, strerr: %s", errno, strerror(errno));
            return 0;
        }
        else
        {
            send_len += n;
            if (send_len >= nbyte)
            {
                LOG_TRACE("send_len : %d", send_len);
                return nbyte;
            }
        }

        StEventItem* item = manager->m_event_scheduler_->GetEventItem(fd);
        if (item == NULL)
        {
            LOG_ERROR("item is NULL");
            return -2;
        }
        item->EnableOutput();
        item->SetOwnerThread(thread);
        int64_t wakeup_timeout = timeout + manager->GetLastClock();
        if (!(manager->m_event_scheduler_->Schedule(thread, NULL, item, wakeup_timeout)))
        {
            LOG_ERROR("item schedule failed, errno: %d, strerr: %s", 
                    errno, strerror(errno));
            // 释放item数据
            manager->m_item_pool_.FreePtr(item);
            return -3;
        }
    }

    return nbyte;
}

void _sleep(int ms)
{
    Manager *manager = GetInstance<Manager>();
    Thread* thread = (Thread*)(manager->
        m_thread_scheduler_->GetActiveThread());
    if (thread != NULL)
    {
        thread->Sleep(ms);
    }
}

int _accept(int fd, struct sockaddr *addr, socklen_t *addrlen, int timeout)
{
    Manager *manager = GetInstance<Manager>();
    Thread* thread = (Thread*)(manager->
        m_thread_scheduler_->GetActiveThread());

    int connfd = -1;
    while ((connfd = st_accept(fd, addr, addrlen)) < 0)
    {
        if (errno == EINTR)
        {
            continue;
        }

        if (!((errno == EAGAIN) || (errno == EINPROGRESS)))
        {
            LOG_ERROR("accept failed, errno: %d, strerr: %s", 
                errno, strerror(errno));
            return -1;
        }

        StEventItem* item = manager->m_event_scheduler_->GetEventItem(fd);
        if (item == NULL)
        {
            LOG_ERROR("item is NULL");
            return -2;
        }
        item->EnableInput();
        item->SetOwnerThread(thread);
        int64_t wakeup_timeout = timeout + manager->GetLastClock();
        if (!(manager->m_event_scheduler_->Schedule(thread, NULL, item, wakeup_timeout)))
        {
            LOG_ERROR("item schedule failed, errno: %d, strerr: %s", 
                    errno, strerror(errno));
            // 释放item数据
            manager->m_item_pool_.FreePtr(item);
            return -3;
        }
    }

    return connfd;
}