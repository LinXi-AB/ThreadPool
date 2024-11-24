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
    void worker();  // �̹߳�������

    std::vector<std::thread> threads_;              // �洢�߳�
    const size_t max_thread_count_ = 10;            // ����߳�����
    std::queue<std::function<void()>> tasks_queue_; // �������
    std::mutex queue_mutex_;                        // ����������еĻ�����
    std::condition_variable condition_;             // ����֪ͨ�̻߳�ȡ����
    std::atomic<bool> stop_;                        // �̳߳ص�ֹͣ״̬
};

ThreadPool::ThreadPool(size_t thread_count)
    : stop_(false)
{
    // ʹ��worker��������һ���������̷߳����̳߳���
    for (size_t i = 0; i < thread_count; ++i)
    {
        threads_.emplace_back(&ThreadPool::worker, this);
    }
}

ThreadPool::~ThreadPool()
{
    // ����ֹͣ��־
    stop_.store(true);
    condition_.notify_all();  // ֪ͨ�����߳��˳�

    // �ȴ��߳���������˳�
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

        // 1.���������δ��ʱ������������������
        if (this->tasks_queue_.size() < this->max_thread_count_)
        {
            tasks_queue_.push(task);
        }
        // 2.�������������ʱ�����̳߳��е��߳�����С������߳��������򴴽�һ�����߳�
        else if (this->threads_.size() < this->max_thread_count_)
        {
            threads_.emplace_back([this, task]
                {
                    task(); // ִ��������
                    this->worker(); // ��������ִ���µ�����
                });
        }
        // 3.����������������̳߳��е��߳�������������߳������������ܾ��ź�
        else
        {
            std::cerr << "Task rejected: Queue and thread pool are full!" << std::endl;
            return; // ���񱻾ܾ�
        }
    }
    condition_.notify_one(); // ֪ͨһ���ȴ����߳�ִ������
}

void ThreadPool::worker()
{
    while (true)
    {
        std::function<void()> task;
        {
            // �ȴ�����
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] { return !tasks_queue_.empty() || stop_; });

            // ����̳߳�ֹͣ���������Ϊ�գ����˳�
            if (stop_ && tasks_queue_.empty())
                return;

            // ��ȡ����
            task = std::move(this->tasks_queue_.front());
            this->tasks_queue_.pop();
        }

        // ִ������
        task();
    }
}

// ʾ������
void task_example()
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main()
{
    // �����̳߳�
    ThreadPool pool(5);

    // �ύ���񣬴��ݺ���ָ��
    for (int i = 0; i < 10; ++i)
    {
        pool.addTask(task_example);
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));  // �ȴ���������ִ�����
    return 0;
}
