#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <new>
#include <string>

#include "epoll.h"

Epoll::Epoll(void) : epoll_loop_(true)
{
#if defined(__APPLE__)
    epfd_ = kqueue();
    // EV_SET(&ev_, epfd_, EVFILT_VNODE, EV_ADD | EV_CLEAR, NOTE_WRITE, 0, NULL);
#elif defined(__linux__)
    epfd_ = ::epoll_create(EPOLL_FD_SETSIZE);
#endif
    assert(epfd_ >= 0);
    readable_map_.clear();
    witable_map_.clear();

    loop_thread_ = std::thread([](Epoll * p_this) { p_this->EpollLoop(); }, this);
}

Epoll::~Epoll(void)
{
    if(loop_thread_.joinable()) {
        loop_thread_.join();
    }
    // EpoolQuit();
    if(epfd_ > 0) {
        close(epfd_);
    }
}

// 添加到epoll事件，默认设置为非阻塞且fd的端口和地址都设为复用
int Epoll::EpollAddRead(int fd, std::function<void()> read_handler)
{
    readable_map_[fd] = read_handler;
#if defined(__unix__) || defined(__APPLE__)
    // 设置为非阻塞
    int sta = fcntl(fd, F_GETFD, 0) | O_NONBLOCK;
    if(fcntl(fd, F_SETFL, sta) < 0) {
        return -1;
    }
#endif
#if defined(__APPLE__)
    struct kevent event;
    EV_SET(&event, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void *)(intptr_t)fd);
    return kevent(epfd_, &event, 1, nullptr, 0, nullptr);
#elif defined(__linux__)
    ev_.data.fd = fd;
    ev_.events  = EPOLLIN;
    // ev_.events  = EPOLLIN | EPOLLET;
    return epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev_);
#elif defined(_WIN32) || defined(_WIN64)
    return 0;
#endif
}

// 添加到epoll事件，默认设置为非阻塞且fd的端口和地址都设为复用
int Epoll::EpollAddWrite(int fd, std::function<void()> write_handler)
{
    witable_map_[fd] = write_handler;
#if defined(__unix__) || defined(__APPLE__)
    // 设置为非阻塞
    int sta = ::fcntl(fd, F_GETFD, 0) | O_NONBLOCK;
    if(::fcntl(fd, F_SETFL, sta) < 0) {
        return -1;
    }
#endif
#if defined(__APPLE__)
    struct kevent event;
    EV_SET(&event, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, (void *)(intptr_t)fd);
    return kevent(epfd_, &event, 1, nullptr, 0, nullptr);
#elif defined(__linux__)
    ev_.data.fd = fd;
    ev_.events  = EPOLLOUT;
    return ::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev_);
#elif defined(_WIN32) || defined(_WIN64)
    return 0;
#endif
}

// 添加到epoll事件，默认设置为非阻塞且fd的端口和地址都设为复用
int Epoll::EpollAddReadWrite(int fd, std::function<void()> read_handler, std::function<void()> write_handler)
{
    readable_map_[fd] = read_handler;
    witable_map_[fd]  = write_handler;
#if defined(__unix__) || defined(__APPLE__)
    // 设置为非阻塞
    int sta = ::fcntl(fd, F_GETFD, 0) | O_NONBLOCK;
    if(::fcntl(fd, F_SETFL, sta) < 0) {
        return -1;
    }
#endif
#if defined(__APPLE__)
    int n = 0;
    struct kevent event[2];
    EV_SET(&event[n++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void *)(intptr_t)fd);
    EV_SET(&event[n++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, (void *)(intptr_t)fd);
    return kevent(epfd_, event, 2, nullptr, 0, nullptr);

#elif defined(__linux__)
    ev_.data.fd = fd;
    ev_.events  = EPOLLIN | EPOLLOUT;
    return ::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev_);
#elif defined(_WIN32) || defined(_WIN64)
    return 0;
#endif
}

int Epoll::EpollDel(int fd)
{
    if(fd < 0) {
        return -1;
    }
#if defined(__APPLE__)
    int n = 0;
    struct kevent event[2];
    EV_SET(&event[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, (void *)(intptr_t)fd);
    EV_SET(&event[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void *)(intptr_t)fd);
    kevent(epfd_, event, 2, nullptr, 0, nullptr);
#elif defined(__linux__)
    ev_.data.fd = fd;
    ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
#endif
    int ret = 0;
    // erase: 0 不存在元素 1 存在元素
    if(witable_map_.count(fd)) {
        witable_map_.erase(fd);
    }
    if(readable_map_.count(fd)) {
        ret |= readable_map_.erase(fd);
    }

    return ret;
}

int Epoll::EpollDelWrite(int fd)
{
    if(fd < 0) {
        return -1;
    }
#if defined(__APPLE__)
    struct kevent event;
    EV_SET(&event, fd, EVFILT_READ, EV_DELETE, 0, 0, (void *)(intptr_t)fd);
    kevent(epfd_, &event, 1, nullptr, 0, nullptr);
#elif defined(__linux__)
    ev_.data.fd = fd;
    ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
#endif
    int ret = 0;
    // erase: 0 不存在元素 1 存在元素
    ret |= witable_map_.erase(fd);
    return ret;
}

bool Epoll::EpoolQuit()
{
    epoll_loop_ = false;
    // for(auto kv:readable_map_) {
    //     EpollDel(kv.first);
    // }
    // for(auto kv:witable_map_) {
    //     EpollDel(kv.first);
    // }
    readable_map_.clear();
    witable_map_.clear();
    return true;
}

int Epoll::EpollLoop()
{
    int nfds        = 0;
    while(epoll_loop_) {
#if defined(__APPLE__)
        struct timespec timeout;
        timeout.tv_sec  = 1;
        timeout.tv_nsec = 0;
        nfds        = kevent(epfd_, nullptr, 0, events_, MAXEVENTS, &timeout);
#elif defined(__linux__)
        nfds = epoll_wait(epfd_, events_, MAXEVENTS, 1000);
#endif

        if(nfds == -1) {
            EpoolQuit();
        }

        if(nfds == 0) {
            // std::cout << "Epoll time out" << std::endl;
            // spdlog::info("{} time out", __FUNCTION__);
            continue;
        }

        for(int i = 0; i < nfds; i++) {
            // 有消息可读取
#if defined(__APPLE__)
            struct kevent event = events_[i];                 // 一个个取出已经就绪的事件
            int fd              = (int)(intptr_t)event.udata; // 从附加数据里面取回文件描述符的值
            if(event.filter == EVFILT_READ) {
                // std::cout << "read data from fd:" << fd << std::endl;
                auto handle_it = readable_map_.find(fd);
                if(handle_it != readable_map_.end()) {
                    handle_it->second();
                } else {
                    std::cout << "can not find the fd:" << fd << std::endl;
                }
            } else if(event.filter == EVFILT_WRITE) {
                std::cout << "write data to fd:" << fd << std::endl;
            } else {
                assert("unknown event");
            }
#elif defined(__linux__)
            if(events_[i].events & EPOLLIN) {
                // 在map中寻找对应的回调函数
                auto handle_it = readable_map_.find(events_[i].data.fd);
                if(handle_it != readable_map_.end()) {
                    handle_it->second();
                }
                // readable_map_[events_[i].data.fd]();
            } else if(events_[i].events & EPOLLOUT) {
                auto handle_it = witable_map_.find(events_[i].data.fd);
                if(handle_it != witable_map_.end()) {
                    handle_it->second();
                }
                // 移除
                witable_map_.erase(events_[i].data.fd);
            }
#endif
        }
    }

    return 0;
}
