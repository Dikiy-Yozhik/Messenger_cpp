#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>

// Структура задачи от клиента
struct ClientTask {
    SOCKET clientSocket;
    std::string message;
    
    ClientTask(SOCKET socket, const std::string& msg) 
        : clientSocket(socket), message(msg) {}
};

// Потокобезопасная очередь
class ThreadSafeQueue {
private:
    std::queue<ClientTask> queue;
    mutable std::mutex mtx;  
    std::condition_variable cv;
    std::atomic<bool> stopFlag{false};
    
public:
    void push(const ClientTask& task) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(task);
        cv.notify_one();  
    }
    
    ClientTask pop() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { 
            return !queue.empty() || stopFlag.load(); 
        });
        
        if (stopFlag.load() && queue.empty()) {
            return ClientTask(INVALID_SOCKET, "");
        }
        
        ClientTask task = queue.front();
        queue.pop();
        return task;
    }
    
    bool empty() const {  
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }
    
    void stop() {
        stopFlag.store(true);
        cv.notify_all();  
    }
    
    size_t size() const {  
        std::lock_guard<std::mutex> lock(mtx);
        return queue.size();
    }
};;

// Сервер
class Server {
private:
    SOCKET serverSocket;
    int port;
    std::atomic<bool> isRunning;
    
    // Очередь задач
    ThreadSafeQueue taskQueue;
    
    // Потоки
    std::thread acceptThread;
    std::vector<std::thread> clientThreads;
    std::vector<std::thread> workerThreads;
    
    // Синхронизация
    std::mutex consoleMutex;
    std::mutex socketMutex;  
    
    bool createSocket();
    bool bindSocket();
    bool startListening();
    void handleClient(SOCKET clientSocket);  // Producer
    void processTasks();                     // Consumer
    void sendResponse(SOCKET clientSocket, const std::string& response);
    
public:
    Server(int port);
    ~Server();
    
    bool startServer();
    void run();
    void closeServer();
    
    // Для отладки
    size_t getQueueSize() const { return taskQueue.size(); }
};

#endif 