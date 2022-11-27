#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

template <class T>
class SignalQueue {
 public:
    explicit SignalQueue(uint32_t max_len = 128) {
        max_len_     = max_len;
        is_set_flag_ = false;
    }

    /**
     * @brief 设置信号，需要线程安全的设置，所设置的event_bit是整形的任意数据
     * @param event_bit
     */
    void PushSignal(const T& signal) {
        queue_mutex_.lock();
        if (event_queue_.size() >= max_len_)
            event_queue_.pop();
        event_queue_.push(signal);
        queue_mutex_.unlock();
        is_set_flag_ = true;           // 设置 lambada表达式中的值
        condition_var_.notify_all();   // 唤醒 condition_var_
    }

    /**
     * @brief 获取信号队列长度
     * @return 返回信号队列的长度
     */
    uint32_t Size() {
        uint32_t size = 0;
        queue_mutex_.lock();
        size = (uint32_t)event_queue_.size();
        queue_mutex_.unlock();
        return size;
    }

    /**
     * @brief Wait signal
     *
     * @param signal_out: signal output
     * @param timeout: wait time ms, when timeout=-1, wait time is forever
     * @return >=0 has signal
     *          -1 timeout
     */
    int32_t PopSignal(T& signal_out, int32_t timeout = -1) {
        queue_mutex_.lock();
        if (event_queue_.size() > 0) {
            signal_out = event_queue_.front();
            event_queue_.pop();
            queue_mutex_.unlock();
            is_set_flag_ = false;
            return 1;
        }
        queue_mutex_.unlock();

        bool signal_status = false;
        std::unique_lock<std::mutex> lck(cond_mutex_);
        if (timeout == -1) {
            // 无超时时间，永久等待
            condition_var_.wait(lck);
        } else {
            if(!is_set_flag_.load()) {
                // 等待新的消息,直到超时
                // 注: 只有当lanbada 表达式中的返回值为true时,wait才会提前退出;
                // 陷阱：当在进入等待状态前，通知发生，会导致通知丢失了，即条件变量仅在接收方处于等待状态时才发送通知。
                signal_status = condition_var_.wait_for(lck, std::chrono::milliseconds(timeout),
                                                        [&] { return is_set_flag_ == true; });                
            } else {
                signal_status = true;
            }
        }

        if (signal_status == true) {
            queue_mutex_.lock();
            if (event_queue_.size() > 0) {
                signal_out = event_queue_.front();
                event_queue_.pop();
                queue_mutex_.unlock();
                is_set_flag_ = false;
                return 1;
            }
            queue_mutex_.unlock();
        }
        is_set_flag_ = false;
        return -1;
    }

 private:
    std::mutex queue_mutex_;
    std::mutex cond_mutex_;
    std::condition_variable condition_var_;
    std::queue<T> event_queue_;
    std::atomic<bool> is_set_flag_;
    uint32_t max_len_;
};