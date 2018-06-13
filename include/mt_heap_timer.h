/*
 * Copyright (C) zhoulv2000@163.com
 */

#ifndef _MT_HEAP_TIMER_H_INCLUDED_
#define _MT_HEAP_TIMER_H_INCLUDED_

#include "mt_utils.h"
#include "mt_heap.h"

MTHREAD_NAMESPACE_BEGIN

class TimerCtrl;
class TimerNotify;

// 时间通知
class TimerNotify : public HeapEntry
{
public:
    virtual void Notify() 
    { 
        return ;
    }
    virtual unsigned long long HeapValue()
    {
        return (unsigned long long)m_time_expired_;
    }
    TimerNotify() : m_time_expired_(0)
    { }
    virtual ~TimerNotify()
    { 
        m_time_expired_ = 0;
    }
    void SetExpiredTime(uint64_t expired)
    {
        m_time_expired_ = expired;
    }
    uint64_t GetExpiredTime()
    {
        return m_time_expired_;
    }

private:
    uint64_t m_time_expired_;
};

// 时间控制器
class TimerCtrl
{
public:
    explicit TimerCtrl(uint32_t max_item = 100000)
    {
        m_heap_ = new HeapList<TimerNotify>(max_item);
    }
    ~TimerCtrl()
    {
        safe_delete(m_heap_);
    }
    bool StartTimer(TimerNotify* timerable, uint32_t interval)
    {
        if (!m_heap_ || !timerable)
        {
            return false;
        }

        utime64_t now_ms = Utils::system_ms();
        LOG_TRACE("now_ms + interval = %llu", now_ms + interval);
        timerable->SetExpiredTime(now_ms + interval);
        int32_t ret = m_heap_->HeapPush(timerable);
        if (ret < 0)
        {
            LOG_ERROR("HeapPush ret = %d", ret);
            return false;
        }

        // TODO : 打印测试
        // m_heap_->HeapForeach();

        return true;
    }
    void StopTimer(TimerNotify* timerable)
    {
        if (!m_heap_ || !timerable)
        {
            return;
        }

        m_heap_->HeapDelete(timerable);
        return;
    }
    // 检查是否过期
    void CheckExpired()
    {
        if (!m_heap_)
        {
            return ;
        }

        utime64_t now = Utils::system_ms();
        LOG_TRACE("now_ms = %llu", now);
        TimerNotify* timer = any_cast<TimerNotify>(m_heap_->HeapTop());
        while (timer && (timer->GetExpiredTime() <= now))
        {
            m_heap_->HeapDelete(timer); // 删除对应的过期时间
            timer->Notify();
            timer = any_cast<TimerNotify>(m_heap_->HeapTop());
        }

        LOG_TRACE("size = %d", m_heap_->HeapSize());
    }

private:
    HeapList<TimerNotify>* m_heap_;
};

MTHREAD_NAMESPACE_END

#endif
