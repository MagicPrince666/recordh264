/**
 * @file epoll.h
 * @author 黄李全 (846863428@qq.com)
 * @brief 单例类 观察者模式
 * @version 0.1
 * @date 2022-11-18
 * @copyright Copyright (c) {2021} 个人版权所有
 */
#ifndef __XEPOLL_H__
#define __XEPOLL_H__

#if defined(__APPLE__)
#include <sys/event.h>
#elif defined(__linux__)
#include <sys/epoll.h>
#endif
#if defined(__unix__)
#include <sys/socket.h>
#endif

#include <functional>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>

#define MAXEVENTS 50
#define EPOLL_FD_SETSIZE 1024

#define MY_EPOLL Epoll::Instance()

class Epoll
{
public:
    static Epoll& Instance() {
        static Epoll instance;
        return instance;
    }

    ~Epoll();

    /**
     * @brief 注册句柄与读回调函数
     * @param fd 
     * @param read_handler 读回调
     * @return int 
     */
    int EpollAddRead(int fd, std::function<void()> read_handler);

    /**
     * @brief 注册句柄与写回调函数
     * @param fd 
     * @param write_handler 写回调
     * @return int 
     */
    int EpollAddWrite(int fd, std::function<void()> write_handler);

    /**
     * @brief 注册句柄与读写回调函数
     * @param fd 
     * @param read_handler 读回调
     * @param write_handler 写回调
     * @return int 
     */
    int EpollAddReadWrite(int fd, std::function<void()> read_handler, std::function<void()> write_handler);

    /**
     * @brief 将读句柄移出epoll监听列表
     * @param fd 
     * @return int 
     */
    int EpollDel(int fd);

    /**
     * @brief 将读回调移除
     * @param fd 
     * @return int 
     */
    int EpollDelWrite(int fd);

    /**
     * @brief 外部控制退出epoll监听循环
     * @return true 
     * @return false 
     */
    bool EpoolQuit();

    bool EpollLoopRunning() {
        return epoll_loop_;
    }

private:
    Epoll();

    /**
     * @brief 循环监听事件
     * @return int 
     */
    int EpollLoop();

#if defined(__APPLE__)
    struct kevent events_[MAXEVENTS]; // kevent返回的事件列表（参考后面的kevent函数）
#elif defined(__linux__)
    struct epoll_event ev_, events_[MAXEVENTS];
#endif
    int epfd_;
    bool epoll_loop_{true};
    std::thread loop_thread_;

    // 读回调列表
    std::unordered_map<int, std::function<void()>> readable_map_;
    // 写回调列表
    std::unordered_map<int, std::function<void()>> witable_map_;
};

#endif
