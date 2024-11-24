#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <atomic>
#include <condition_variable>

class ThreadPool
{
public:
    ThreadPool(size_t thread_count);
    ~ThreadPool();
    void addTask(const std::function<void()>& task);

private:
    void worker();  // 线程工作函数

    std::vector<std::thread> threads_;              // 存储线程
    const size_t max_thread_count_ = 10;            // 最大线程数量
    std::queue<std::function<void()>> tasks_queue_; // 任务队列
    std::mutex queue_mutex_;                        // 保护任务队列的互斥锁
    std::condition_variable condition_;             // 用于通知线程获取任务
    std::atomic<bool> stop_;                        // 线程池的停止状态
};

ThreadPool::ThreadPool(size_t thread_count)
    : stop_(false)
{
    // 使用worker函数创建一定数量的线程放入线程池中
    for (size_t i = 0; i < thread_count; ++i)
    {
        threads_.emplace_back(&ThreadPool::worker, this);
    }
}

ThreadPool::~ThreadPool()
{
    // 设置停止标志
    stop_.store(true);
    condition_.notify_all();  // 通知所有线程退出

    // 等待线程完成任务并退出
    for (auto& thread : threads_)
    {
        if (thread.joinable())
            thread.join();
    }
}

void ThreadPool::addTask(const std::function<void()>& task)
{
    {
        std::lock_guard<std::mutex> lock(this->queue_mutex_);

        // 1.当任务队列未满时，将任务加入任务队列
        if (this->tasks_queue_.size() < this->max_thread_count_)
        {
            tasks_queue_.push(task);
        }
        // 2.当任务队列已满时，且线程池中的线程数量小于最大线程数量，则创建一个新线程
        else if (this->threads_.size() < this->max_thread_count_)
        {
            threads_.emplace_back([this, task]
                {
                    task(); // 执行新任务
                    this->worker(); // 完成任务后执行新的任务
                });
        }
        // 3.当任务队列已满且线程池中的线程数量等于最大线程数量，发出拒绝信号
        else
        {
            std::cerr << "Task rejected: Queue and thread pool are full!" << std::endl;
            return; // 任务被拒绝
        }
    }
    condition_.notify_one(); // 通知一个等待的线程执行任务
}

void ThreadPool::worker()
{
    while (true)
    {
        std::function<void()> task;
        {
            // 等待任务
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] { return !tasks_queue_.empty() || stop_; });

            // 如果线程池停止且任务队列为空，则退出
            if (stop_ && tasks_queue_.empty())
                return;

            // 获取任务
            task = std::move(this->tasks_queue_.front());
            this->tasks_queue_.pop();
        }

        // 执行任务
        task();
    }
}

// 示例任务
void task_example()
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main()
{
    // 创建线程池
    ThreadPool pool(5);

    // 提交任务，传递函数指针
    for (int i = 0; i < 10; ++i)
    {
        pool.addTask(task_example);
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));  // 等待所有任务执行完成
    return 0;
}
