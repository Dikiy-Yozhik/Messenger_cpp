#include "iocp_core.h"
#include <iostream>

IOCPCore::IOCPCore() 
    : iocp_handle_(INVALID_HANDLE_VALUE), 
      is_running_(false) {}

IOCPCore::~IOCPCore() { 
    stop(); 
}

bool IOCPCore::setup() {
    iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (iocp_handle_ == NULL) {
        std::cerr << "Failed to create IOCP: " << GetLastError() << "\n";
        return false;
    }
    return true;
}

void IOCPCore::associateSocket(SOCKET socket, ULONG_PTR key) {
    if (CreateIoCompletionPort((HANDLE)socket, iocp_handle_, key, 0) == NULL) {
        std::cerr << "Failed to associate socket: " << GetLastError() << "\n";
    }
}

void IOCPCore::runWorkerThreads(int count) {
    is_running_ = true;
    workers_.reserve(count);
    
    for (int i = 0; i < count; ++i) {
        workers_.emplace_back([this]() {
            while (is_running_) {
                DWORD bytes = 0;
                ULONG_PTR key = 0;
                LPOVERLAPPED overlapped = nullptr;

                BOOL ok = GetQueuedCompletionStatus(
                    iocp_handle_, &bytes, &key, &overlapped, INFINITE);

                if (!is_running_) break;

                if (!ok) {
                    DWORD error = GetLastError();
                    std::lock_guard<std::mutex> lock(callbacks_mutex_);
                    if (overlapped) {
                        // Определяем тип операции по ключу
                        if (key == static_cast<ULONG_PTR>(CompletionKeyType::READ) && read_cb_) {
                            read_cb_(error, key, overlapped);
                        } else if (key == static_cast<ULONG_PTR>(CompletionKeyType::WRITE) && write_cb_) {
                            write_cb_(error, key, overlapped);
                        }
                    }
                    continue;
                }

                if (!overlapped) continue;

                std::lock_guard<std::mutex> lock(callbacks_mutex_);
                switch (static_cast<CompletionKeyType>(key)) {
                    case CompletionKeyType::CONNECTION:
                        if (connection_cb_) connection_cb_(bytes, key, overlapped);
                        break;
                    case CompletionKeyType::READ:
                        if (read_cb_) read_cb_(bytes, key, overlapped);
                        break;
                    case CompletionKeyType::WRITE:
                        if (write_cb_) write_cb_(bytes, key, overlapped);
                        break;
                }
            }
        });
    }
}

void IOCPCore::stop() {
    if (!is_running_) return;
    
    is_running_ = false;
    for (size_t i = 0; i < workers_.size(); ++i) {
        PostQueuedCompletionStatus(iocp_handle_, 0, 0, nullptr);
    }
    
    for (auto& thread : workers_) {
        if (thread.joinable()) thread.join();
    }
    
    if (iocp_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(iocp_handle_);
        iocp_handle_ = INVALID_HANDLE_VALUE;
    }
}

void IOCPCore::postCompletion(DWORD bytes, ULONG_PTR key, LPOVERLAPPED overlapped) {
    PostQueuedCompletionStatus(iocp_handle_, bytes, key, overlapped);
}