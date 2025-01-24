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

	std::vector<std::thread> threads;              /// �洢�̳߳��е��߳�
	const size_t max_thread_count = 10;            /// ����߳����� 
	std::queue<std::function<void()>> tasks_queue; /// ������У��洢Ҫִ�е����� 
	std::mutex queue_mutex;                        /// ����������еĻ����� 
	std::condition_variable condition;             /// ����֪ͨ�̻߳�ȡ���� 
	std::atomic<bool> stop = false;                /// ��־λ��ָʾ�̳߳��Ƿ�ֹͣ 
};

/**
 * @brief ��thread_count��worker�̼߳����̳߳�
 */
ThreadPool::ThreadPool(size_t thread_count)
{
	for (size_t i = 0; i < thread_count; ++i)
	{
		threads.emplace_back(&ThreadPool::worker, this);
	}
}

/**
 * @brief �����̳߳�״̬Ϊstop���ȴ������߳��˳�
 */
ThreadPool::~ThreadPool()
{
	// ����ֹͣ��־
	stop.store(true);
	condition.notify_all();  // ֪ͨ�����߳��˳�

	// �ȴ��߳���������˳�
	for (auto& thread : threads)
	{
		if (thread.joinable())
			thread.join();
	}
}

/**
 * @brief ��������������м�������
 * @details 
 * - ���������δ��ʱ��ֱ�ӽ�����������
 * - �������������ʱ����worker�߳�����δ�ﵽ����ֵʱ���߲���Ӧ���������������¿�һ���̴߳����¼��������
 * - �������������ʱ����worker�߳������ﵽ����ֵʱ��ֱ�Ӿܾ�����������
 */
void ThreadPool::addTask(const std::function<void()>& task)
{
	{
		std::lock_guard<std::mutex> lock(this->queue_mutex);

		// 1. ���������δ��ʱ������������������
		if (this->tasks_queue.size() < this->max_thread_count)
		{
			tasks_queue.push(task);
		}

		// 2. �������������ʱ�����̳߳��е��߳�����С������߳��������򴴽�һ�����߳�
		else if (this->threads.size() < this->max_thread_count)
		{
			threads.emplace_back([this, task]
				{
					task(); 
					this->worker(); // ������¼��������󣬳�Ϊһ��worker���Դ��������¼��������
				});
		}

		// 3. ����������������̳߳��е��߳�������������߳������������ܾ��ź�
		else
		{
			std::cerr << "T�̳߳����߳������Ѵ����ޣ��޷�����������" << std::endl;
			return;
		}
	}
	condition.notify_one(); // ֪ͨһ���ȴ����߳�ִ������
}

/**
 * @brief ִ��������߳�
 * @details
 * - ���������Ϊ�����̳߳�δֹͣʱ��worker����
 * - ���̳߳�ֹͣ���������Ϊ��ʱ(���԰�ȫ�˳�ʱ)��worker�߳�����
 * - ���̳߳������������������������ʱ��ȡͷ���������ִ��
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
					return !tasks_queue.empty() || stop;// ����falseʱ���������������Ϊ�����̳߳�δֹͣʱ�������ȴ�����
				});

			// ����̳߳�ֹͣ������ȫ��������ϣ������˳�
			if (stop && tasks_queue.empty())
				return;

			// �̳߳������������������������
			task = std::move(this->tasks_queue.front());
			this->tasks_queue.pop();
		}

		// ִ������
		task();
	}
}

// ʾ������
void task_example(int id)
{
	std::cout << "���ڽ�������: " << id << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(1));
}

/**
 * @test �̳߳صĲ��Է�����
 * - ���Թ��캯��
 * - ����addTask����
 * - ������������
 * - ...
 */
int main()
{
	// �����̳߳أ�ָ������߳���Ϊ5
	ThreadPool pool(10);

	// �ύ���񣬴��ݺ����Ͳ���
	for (int i = 0; i < 10; ++i)
	{
		pool.addTask(std::bind(task_example, i));
	}

	std::this_thread::sleep_for(std::chrono::seconds(3));  // �ȴ���������ִ�����
	return 0;
}
