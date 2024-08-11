#ifndef  THREADPOOL_H
#define  THREADPOOL_H

#include <vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<unordered_map> 
#include<thread>

class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	//������캯��������Any���������������͵�����
	template<typename T>
	Any(T data) :base_(std::make_unique<Derive<T>>(data)) {};

	//���洢��Any�������������data��ȡ����
	template<typename T>
	T cast_()
	{
		//��base_�ҵ���ָ���Derive����,��������ȡ��data��Ա����
		//����ָ��תΪ������ָ��
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw "type is unmatch!";
		}
		return pd->data_;

	}



private:
	//��������
	class Base
	{
	public:
		virtual ~Base() = default;
	};

	//����������
	template<typename T>
	class Derive : public Base
	{
	public:
		Derive(T data) :data_(data) {};
		T data_; //������������������

	};


private:
	//�������ָ��
	std::unique_ptr<Base> base_;

};

//ʵ��һ���ź�����
class Semaphore
{
public:
	Semaphore(int resLimit = 0) :resLimit_(resLimit) {};
	~Semaphore() = default;

	//��ȡһ���ź�����Դ
	void wait()
	{
		std::unique_lock<std::mutex> lock(mutex_);
		//�ȴ��ź�������Դ,û����Դ�Ļ���������ǰ�߳�
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0;});
		resLimit_--;
	}

	//����һ���ź�����Դ
	void post()
	{
		std::unique_lock<std::mutex> lock(mutex_);
		resLimit_++;
		//linux��condition_variable����������ʲôҲû��
		//��������״̬�Ѿ�ʧЧ �޹�����
		cond_.notify_all();
	}

private:
	int resLimit_;
	std::mutex mutex_;
	std::condition_variable cond_;
};

//Task���͵�ǰ������
class Task;

//ʵ�ֽ����ύ���̳߳ص�task����ִ����ɺ�ķ���ֵ����result
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	//����һ:setVal����,��ȡִ��������ɺ�ķ���ֵ
	//������ִ�еķ��ؽ���洢�� Result ������
	void setVal(Any any);

	//�����:get����,�û��������������ȡtask�ķ���ֵ
	//�����û��� Result �����л�ȡ�����ִ�н��
	Any get();

private:
	Any any_; //�洢����ķ���ֵ
	Semaphore sem_; //�߳�ͨ���ź���
	std::shared_ptr<Task> task_; //ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid_;   //�����Ƿ���Ч
};

//����������
//�û�����ͨ���̳�Task���������Լ�����������
class Task
{
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);
	virtual Any run() = 0;
private:
	Result* result_;  //Result���������ڳ���Task����
};

//�̳߳�֧�ֵ�����ģʽ
enum class PoolMode
{
	MODE_FIXED,		//�̶���С�̳߳�
	MODE_CACHED,	//�ɱ��С�̳߳�
};


//�߳�����
class Thread
{
public:
	//�̺߳�����������
	using ThreadFunc = std::function<void(int)>;

	Thread(ThreadFunc func);

	~Thread();
	void start();

	//��ȡ�߳�id
	int getId() const;
private:

	ThreadFunc func_;
	static int generateId_;
	int threadId_;

};

/*
example:
	ThreadPool pool;
	pool.start(4);

	class  MyTask : public Task
	{
	public:
		void run()
		{
			//do something
		}
	};

	pool.submitTask(std::make_shared<Task>());
	pool.stop();
*/


//�̳߳�����
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();
	//�����̳߳�ģʽ
	void setMode(PoolMode mode);
	////���ó�ʼ�߳�����
	//void setInitThreadSize(int size);
	//���������������
	void setTaskQueMaxThreadHold(int threshhold);
	//����cacheģʽ���߳���������ֵ
	void setThreadSizeThreadHold(int threshhold);
	//�ύ����
	Result submitTask(std::shared_ptr<Task> sp);
	//�����̳߳�
	void start(int initThreadSize);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	//�̺߳���
	void threadFunc(int threadid);

	//���pool������״̬
	bool checkRunningState() const;
private:
	//std::vector<std::unique_ptr<Thread>> threads_;//�߳��б�
	std::unordered_map<int,std::unique_ptr<Thread>> threads_;//�߳��б�
	size_t initThreadSize_;//��ʼ�߳�����
	size_t threadSizeThreadHold_;//�߳�����������ֵ
	std::atomic_size_t curThreadSize_; //��¼�̳߳����̵߳�����
	std::atomic_size_t idleThreadSize_;//��¼�����߳�����
	//std::atomic_size_t threadCount_;



	std::queue <std::shared_ptr<Task>> taskQue_;//�������
	std::atomic_size_t taskSize_;		//��������
	size_t taskQueMaxThreadHold_;     //�����������������ֵ

	std::mutex taskQueMtx_; //������л�����
	std::condition_variable notFull_;//���� ���� ��������
	std::condition_variable notEmpty_;//���� ���� ��������
	std::condition_variable exitCond_;//���� ���� ��������

	PoolMode poolMode_;//�̳߳�ģʽ

	std::atomic_int isPoolRunning_; //��ʾ�̵߳�����״̬
	

	

};


#endif // !
