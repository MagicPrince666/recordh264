#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <new>
#include <string>

#include "epoll.h"

Epoll::Epoll(void)
{
#ifdef __APPLE__
    epfd_ = kqueue();
#else
    epfd_       = ::epoll_create(EPOLL_FD_SETSIZE);
#endif
    assert(epfd_ >= 0);
    epoll_loop_ = true;
    readable_map_.clear();
    witable_map_.clear();

    loop_thread_ = std::thread([](Epoll *p_this) { p_this->EpollLoop(); }, this);
}

Epoll::~Epoll(void)
{
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
    if (epfd_ > 0) {
        ::close(epfd_);
    }
}

// 添加到epoll事件，默认设置为非阻塞且fd的端口和地址都设为复用
int Epoll::EpollAddRead(int fd, std::function<void()> read_handler)
{
    readable_map_[fd] = read_handler;

    // 设置为非阻塞
    int sta = ::fcntl(fd, F_GETFD, 0) | O_NONBLOCK;
    if (::fcntl(fd, F_SETFL, sta) < 0) {
        return -1;
    }
#ifdef __APPLE__
    int num = 0;
    EV_SET(&ev_[num++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &fd);
    // EV_SET(&ev_[num++], fd, EVFILT_WRITE, EV_ADD|EV_ENABLE, 0, 0, &fd);
    return num;
    // return kevent(epfd_, ev_, n, nullptr, 0, nullptr);

#else
    ev_.data.fd = fd;
    ev_.events  = EPOLLIN;
    // ev_.events  = EPOLLIN | EPOLLET;
    return ::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev_);
#endif
}

// 添加到epoll事件，默认设置为非阻塞且fd的端口和地址都设为复用
int Epoll::EpollAddWrite(int fd, std::function<void()> write_handler)
{
    witable_map_[fd]  = write_handler;

    // 设置为非阻塞
    int sta = ::fcntl(fd, F_GETFD, 0) | O_NONBLOCK;
    if (::fcntl(fd, F_SETFL, sta) < 0) {
        return -1;
    }
#ifdef __APPLE__
    int num = 0;
    EV_SET(&ev_[num++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &fd);
    // EV_SET(&ev_[num++], fd, EVFILT_WRITE, EV_ADD|EV_ENABLE, 0, 0, &fd);
    return num;
    // return kevent(epfd_, ev_, n, nullptr, 0, nullptr);

#else
    ev_.data.fd = fd;
    ev_.events  = EPOLLOUT;
    return ::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev_);
#endif
}

// 添加到epoll事件，默认设置为非阻塞且fd的端口和地址都设为复用
int Epoll::EpollAddReadWrite(int fd, std::function<void()> read_handler, std::function<void()> write_handler)
{
    readable_map_[fd] = read_handler;
    witable_map_[fd]  = write_handler;

    // 设置为非阻塞
    int sta = ::fcntl(fd, F_GETFD, 0) | O_NONBLOCK;
    if (::fcntl(fd, F_SETFL, sta) < 0) {
        return -1;
    }
#ifdef __APPLE__
    int num = 0;
    EV_SET(&ev_[num++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &fd);
    // EV_SET(&ev_[num++], fd, EVFILT_WRITE, EV_ADD|EV_ENABLE, 0, 0, &fd);
    return num;
    // return kevent(epfd_, ev_, n, nullptr, 0, nullptr);

#else
    ev_.data.fd = fd;
    ev_.events  = EPOLLIN | EPOLLOUT;
    return ::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev_);
#endif
}

int Epoll::EpollDel(int fd)
{
    if(fd < 0) {
        return -1;
    }
#ifdef __APPLE__
    int num = 0;
    EV_SET(&ev_[num++], fd, EVFILT_READ, EV_DELETE, 0, 0, &fd);
    // EV_SET(&ev_[num++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, &fd);
#else
    ev_.data.fd = fd;
    ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
#endif
    int ret = 0;
    // erase: 0 不存在元素 1 存在元素
    ret |= readable_map_.erase(fd);
    return ret;
}

int Epoll::EpollDelWrite(int fd)
{
    if(fd < 0) {
        return -1;
    }
#ifdef __APPLE__
    int num = 0;
    EV_SET(&ev_[num++], fd, EVFILT_READ, EV_DELETE, 0, 0, &fd);
    // EV_SET(&ev_[num++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, &fd);
#else
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
    return true;
}

int Epoll::EpollLoop()
{
    while (epoll_loop_) {
#ifdef __APPLE__
        struct timespec timeout;
        timeout.tv_sec  = 1;
        timeout.tv_nsec = 0;
        int nfds = kevent(epfd_, ev_, 2, activeEvs_, MAXEVENTS, &timeout);
#else
        int nfds = ::epoll_wait(epfd_, events_, MAXEVENTS, 1000);
#endif

        if (nfds == -1) {
            EpoolQuit();
        }

        if (nfds == 0) {
            continue;
          // std::cout << "Epoll time out" << std::endl;
        }

        for (int i = 0; i < nfds; i++) {
            // 有消息可读取
#ifdef __APPLE__
            int fd     = (int)(intptr_t)activeEvs_[i].udata;
            int events = activeEvs_[i].filter;
            if (events == EVFILT_READ) {
                std::cout << "read data from fd:" << fd << std::endl;
                auto handle_it = readable_map_.find(fd);
                if (handle_it != readable_map_.end()) {
                    handle_it->second();
                } else {
                    std::cout << "can not find the fd:" << fd << std::endl;
                }
            } else if (events == EVFILT_WRITE) {
                std::cout << "write data to fd:" << fd << std::endl;
            } else {
                assert("unknown event");
            }
#else
            if (events_[i].events & EPOLLIN) {
                // 在map中寻找对应的回调函数
                auto handle_it = readable_map_.find(events_[i].data.fd);
                if (handle_it != readable_map_.end()) {
                    handle_it->second();
                }
                // readable_map_[events_[i].data.fd]();
            } else if (events_[i].events & EPOLLOUT) {
                auto handle_it = witable_map_.find(events_[i].data.fd);
                if (handle_it != witable_map_.end()) {
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
