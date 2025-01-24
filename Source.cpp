#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool
{
public:
	ThreadPool(size_t thread_count);
	~ThreadPool();
	void addTask(const std::function<void()>& task);

private:
	void worker();

	std::vector<std::thread> threads;              /// 存储线程池中的线程
	const size_t max_thread_count = 10;            /// 最大线程数量 
	std::queue<std::function<void()>> tasks_queue; /// 任务队列，存储要执行的任务 
	std::mutex queue_mutex;                        /// 保护任务队列的互斥锁 
	std::condition_variable condition;             /// 用于通知线程获取任务 
	std::atomic<bool> stop = false;                /// 标志位，指示线程池是否停止 
};

/**
 * @brief 将thread_count个worker线程加入线程池
 */
ThreadPool::ThreadPool(size_t thread_count)
{
	for (size_t i = 0; i < thread_count; ++i)
	{
		threads.emplace_back(&ThreadPool::worker, this);
	}
}

/**
 * @brief 设置线程池状态为stop并等待所有线程退出
 */
ThreadPool::~ThreadPool()
{
	// 设置停止标志
	stop.store(true);
	condition.notify_all();  // 通知所有线程退出

	// 等待线程完成任务并退出
	for (auto& thread : threads)
	{
		if (thread.joinable())
			thread.join();
	}
}

/**
 * @brief 尝试向任务队列中加入任务
 * @details 
 * - 当任务队列未满时，直接将任务加入队列
 * - 当任务队列已满时，但worker线程数量未达到上限值时（高并发应急方案），可以新开一个线程处理新加入的任务
 * - 当任务队列已满时，但worker线程数量达到上限值时，直接拒绝加入新任务
 */
void ThreadPool::addTask(const std::function<void()>& task)
{
	{
		std::lock_guard<std::mutex> lock(this->queue_mutex);

		// 1. 当任务队列未满时，将任务加入任务队列
		if (this->tasks_queue.size() < this->max_thread_count)
		{
			tasks_queue.push(task);
		}

		// 2. 当任务队列已满时，且线程池中的线程数量小于最大线程数量，则创建一个新线程
		else if (this->threads.size() < this->max_thread_count)
		{
			threads.emplace_back([this, task]
				{
					task(); 
					this->worker(); // 完成了新加入的任务后，成为一般worker可以处理任意新加入的任务
				});
		}

		// 3. 当任务队列已满且线程池中的线程数量等于最大线程数量，发出拒绝信号
		else
		{
			std::cerr << "T线程池中线程数量已达上限，无法加入新任务" << std::endl;
			return;
		}
	}
	condition.notify_one(); // 通知一个等待的线程执行任务
}

/**
 * @brief 执行任务的线程
 * @details
 * - 当任务队列为空且线程池未停止时，worker阻塞
 * - 当线程池停止且任务队列为空时(可以安全退出时)，worker线程销毁
 * - 当线程池运行中且任务队列中有任务时，取头部任务进行执行
 */
void ThreadPool::worker()
{
	while (true)
	{
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			condition.wait(lock, [this]
				{
					return !tasks_queue.empty() || stop;// 返回false时阻塞，当任务队列为空且线程池未停止时，阻塞等待任务
				});

			// 如果线程池停止且任务全部处理完毕，允许退出
			if (stop && tasks_queue.empty())
				return;

			// 线程池运行中且任务队列中有任务
			task = std::move(this->tasks_queue.front());
			this->tasks_queue.pop();
		}

		// 执行任务
		task();
	}
}

// 示例任务
void task_example(int id)
{
	std::cout << "正在进行任务: " << id << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(1));
}

/**
 * @test 线程池的测试方案：
 * - 测试构造函数
 * - 测试addTask函数
 * - 测试析构函数
 * - ...
 */
int main()
{
	// 创建线程池，指定最大线程数为5
	ThreadPool pool(10);

	// 提交任务，传递函数和参数
	for (int i = 0; i < 10; ++i)
	{
		pool.addTask(std::bind(task_example, i));
	}

	std::this_thread::sleep_for(std::chrono::seconds(3));  // 等待所有任务执行完成
	return 0;
}
