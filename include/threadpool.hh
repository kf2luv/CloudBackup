#pragma once
#include <iostream>
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include "log/ckflog.hpp"

namespace ckf
{
    class ThreadPool
    {
    public:
        enum TaskPriority
        {
            LV1,
            LV2,
            LV3
        };

    private:
        static const size_t thread_num = 3; // 工作线程个数
        using Task = std::function<void()>;
        using TaskPair = std::pair<TaskPriority, Task>; // 带优先级的任务
        class priComparison
        {
        public:
            bool operator()(const TaskPair &p1, const TaskPair &p2)
            {
                return p1.first > p2.first; // 小根堆，优先级高（TaskPriority小）在前面
            }
        };

        using Threads = std::vector<std::thread *>;
        using TaskQueue = std::priority_queue<TaskPair, std::vector<TaskPair>, priComparison>;

    public:
        static ThreadPool &getInstance(); // 获取单例对象
        void start();                     // 线程池开始工作
        template <typename F, typename... Args>
        auto submit(const TaskPriority &priLevel, F &&f, Args &&...args) // 提交一个任务到线程池
            -> std::future<decltype(f(args...))>;

    private:
        ThreadPool();
        ~ThreadPool();
        ThreadPool(const ThreadPool &other) = delete;
        ThreadPool& operator=(const ThreadPool &other) = delete;

        void stop();       // 线程池结束工作
        Task take();       // 从任务队列中取出队列（线程安全）
        void threadLoop(); // 工作线程执行函数

    private:
        Threads _threads;              // 工作线程组
        TaskQueue _task_queue;         // 任务队列
        std::mutex _mutex;             // 保护任务队列线程安全
        std::condition_variable _cond; // 条件变量
        std::atomic<bool> _isRunning;  // 线程池“工作中”标识 (原子)
    };

}

ckf::ThreadPool &ckf::ThreadPool::getInstance() // C++11之后的单例模式
{
    static ThreadPool inst;
    return inst;
}

ckf::ThreadPool::ThreadPool()
{
    start();
}

ckf::ThreadPool::~ThreadPool()
{
    if (_isRunning)
    {
        this->stop();
        std::cout << "线程池停止工作" << std::endl;
    }
}

void ckf::ThreadPool::start()
{
    // 线程池开始运行
    _isRunning = true;
    // 初始化工作线程组
    for (int i = 0; i < thread_num; i++)
    {
        std::thread *thr = new std::thread(&ckf::ThreadPool::threadLoop, this);
        _threads.push_back(thr);
    }
}

void ckf::ThreadPool::stop()
{
    _isRunning = false;
    _cond.notify_all(); // 通知所有线程，不再等待
    // 等待工作线程的任务都执行完
    for (auto thr : _threads)
    {
        thr->join();
        delete thr;
    }
    _threads.clear();
}

void ckf::ThreadPool::threadLoop()
{
    // 工作线程不断地从任务队列中取出任务，若队列为空，则阻塞等待
    while (_isRunning)
    {
        Task task = take();
        if (task)
        {
            std::cout << "线程id: " << std::this_thread::get_id() << " 获取到任务";
            task();
        }
    }
    std::cout << "线程id: " << std::this_thread::get_id() << " 退出";
}

ckf::ThreadPool::Task ckf::ThreadPool::take() // 从任务队列中取出任务
{
    // 1.加锁保护
    std::unique_lock<std::mutex> lockguard(_mutex);

    // 2.如果任务队列为空，阻塞等待
    while (_isRunning && _task_queue.empty())
    {
        _cond.wait(lockguard);
    }

    // 3.线程被唤醒，阻塞等待结束，此时有两种情况
    // (1)任务队列里新增任务
    // (2)线程池stop了, 如果任务队列里还有任务，则将其取出，进行最后的工作
    // 基于情况(2)，此时任务队列中还不一定有任务，因此要判断一下

    Task task;
    if (!_task_queue.empty())
    {
        task = _task_queue.top().second;
        _task_queue.pop();
    }
    return task;
}

template <typename F, typename... Args>
auto ckf::ThreadPool::submit(const TaskPriority &priLevel, F &&f, Args &&...args)
    -> std::future<decltype(f(args...))>
{
    using RetType = decltype(f(args...)); // 返回类型

    std::function<RetType()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

    auto task_ptr = std::make_shared<std::packaged_task<RetType()>>(func);
    // std::packaged_task<RetType()> ptask(func);//err

    Task task = [task_ptr]()
    {
        (*task_ptr)();
    };

    // 对于lambda赋值给std::function，后者会拷贝捕获的变量(值传递)。如果这些变量是局部变量，
    // 并且它们没有在 std::function 之外的地方存活下来，那么超出作用域后，这些拷贝的对象也会销毁。

    std::unique_lock<std::mutex> lockguard(_mutex);
    TaskPair taskPair(priLevel, task);
    _task_queue.push(taskPair);
    _cond.notify_one();

    return task_ptr->get_future();
}