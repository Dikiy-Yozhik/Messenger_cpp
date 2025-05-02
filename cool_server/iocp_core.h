#pragma once

#include <winsock2.h>
#include <windows.h>
#include <memory>
#include <functional>
#include <atomic>
#include <vector>
#include <thread>
#include <mutex>

class IOCPCore {
public:
    enum class CompletionKeyType {
        CONNECTION = 0xAAAA,  // Changed from 0xDEADBEEF for better readability
        READ,
        WRITE
    };

    using CompletionCallback = std::function<void(DWORD, ULONG_PTR, LPOVERLAPPED)>;

    IOCPCore();
    ~IOCPCore();

    bool setup();
    void associateSocket(SOCKET socket, ULONG_PTR key);
    void runWorkerThreads(int count);
    void stop();
    void postCompletion(DWORD bytes, ULONG_PTR key, LPOVERLAPPED overlapped);

    void setConnectionCallback(CompletionCallback cb) { 
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        connection_cb_ = cb; 
    }
    
    void setReadCallback(CompletionCallback cb) { 
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        read_cb_ = cb; 
    }

    void setWriteCallback(CompletionCallback cb) { 
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        write_cb_ = cb; 
    }

private:
    HANDLE iocp_handle_;
    std::atomic<bool> is_running_;
    std::vector<std::thread> workers_;
    std::mutex callbacks_mutex_;
    
    CompletionCallback connection_cb_;
    CompletionCallback read_cb_;
    CompletionCallback write_cb_;  // Новый коллбэк
};