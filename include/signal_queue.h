#ifndef __SIGNAL_QUEUE_H__
#define __SIGNAL_QUEUE_H__

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

    void PushSignal(const T& signal) {
        queue_mutex_.lock();
        if (event_queue_.size() >= max_len_)
            event_queue_.pop();
        event_queue_.push(signal);
        queue_mutex_.unlock();
        is_set_flag_ = true;
        condition_var_.notify_all();
    }

    uint32_t Size() {
        uint32_t size = 0;
        queue_mutex_.lock();
        size = (uint32_t)event_queue_.size();
        queue_mutex_.unlock();
        return size;
    }

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
            condition_var_.wait(lck);
        } else {
            if(!is_set_flag_.load()) {
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

#endif
