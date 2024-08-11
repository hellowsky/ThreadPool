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

	//这个构造函数可以让Any接收其他任意类型的数据
	template<typename T>
	Any(T data) :base_(std::make_unique<Derive<T>>(data)) {};

	//将存储在Any对象里面的数据data提取出来
	template<typename T>
	T cast_()
	{
		//从base_找到它指向的Derive对象,从它里面取出data成员变量
		//基类指针转为派生类指针
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw "type is unmatch!";
		}
		return pd->data_;

	}



private:
	//基类类型
	class Base
	{
	public:
		virtual ~Base() = default;
	};

	//派生类类型
	template<typename T>
	class Derive : public Base
	{
	public:
		Derive(T data) :data_(data) {};
		T data_; //保存了任意其它类型

	};


private:
	//定义基类指针
	std::unique_ptr<Base> base_;

};

//实现一个信号量类
class Semaphore
{
public:
	Semaphore(int resLimit = 0) :resLimit_(resLimit) {};
	~Semaphore() = default;

	//获取一个信号量资源
	void wait()
	{
		std::unique_lock<std::mutex> lock(mutex_);
		//等待信号量有资源,没有资源的话会阻塞当前线程
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0;});
		resLimit_--;
	}

	//增加一个信号量资源
	void post()
	{
		std::unique_lock<std::mutex> lock(mutex_);
		resLimit_++;
		//linux下condition_variable的析构函数什么也没做
		//导致这里状态已经失效 无故阻塞
		cond_.notify_all();
	}

private:
	int resLimit_;
	std::mutex mutex_;
	std::condition_variable cond_;
};

//Task类型的前置声明
class Task;

//实现接收提交到线程池的task任务执行完成后的返回值类型result
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	//问题一:setVal方法,获取执行任务完成后的返回值
	//将任务执行的返回结果存储在 Result 对象中
	void setVal(Any any);

	//问题二:get方法,用户调用这个方法获取task的返回值
	//允许用户从 Result 对象中获取任务的执行结果
	Any get();

private:
	Any any_; //存储任务的返回值
	Semaphore sem_; //线程通信信号量
	std::shared_ptr<Task> task_; //指向对应获取返回值的任务对象
	std::atomic_bool isValid_;   //任务是否有效
};

//任务抽象基类
//用户可以通过继承Task类来定义自己的任务类型
class Task
{
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);
	virtual Any run() = 0;
private:
	Result* result_;  //Result的生命周期长于Task对象
};

//线程池支持的两种模式
enum class PoolMode
{
	MODE_FIXED,		//固定大小线程池
	MODE_CACHED,	//可变大小线程池
};


//线程类型
class Thread
{
public:
	//线程函数对象类型
	using ThreadFunc = std::function<void(int)>;

	Thread(ThreadFunc func);

	~Thread();
	void start();

	//获取线程id
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


//线程池类型
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();
	//设置线程池模式
	void setMode(PoolMode mode);
	////设置初始线程数量
	//void setInitThreadSize(int size);
	//设置任务队列上限
	void setTaskQueMaxThreadHold(int threshhold);
	//设置cache模式下线程数量的阈值
	void setThreadSizeThreadHold(int threshhold);
	//提交任务
	Result submitTask(std::shared_ptr<Task> sp);
	//启动线程池
	void start(int initThreadSize);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	//线程函数
	void threadFunc(int threadid);

	//检查pool的运行状态
	bool checkRunningState() const;
private:
	//std::vector<std::unique_ptr<Thread>> threads_;//线程列表
	std::unordered_map<int,std::unique_ptr<Thread>> threads_;//线程列表
	size_t initThreadSize_;//初始线程数量
	size_t threadSizeThreadHold_;//线程数量上限阈值
	std::atomic_size_t curThreadSize_; //记录线程池里线程的数量
	std::atomic_size_t idleThreadSize_;//记录空闲线程数量
	//std::atomic_size_t threadCount_;



	std::queue <std::shared_ptr<Task>> taskQue_;//任务队列
	std::atomic_size_t taskSize_;		//任务数量
	size_t taskQueMaxThreadHold_;     //任务队列数量上限阈值

	std::mutex taskQueMtx_; //任务队列互斥锁
	std::condition_variable notFull_;//队列 不满 条件变量
	std::condition_variable notEmpty_;//队列 不空 条件变量
	std::condition_variable exitCond_;//队列 不空 条件变量

	PoolMode poolMode_;//线程池模式

	std::atomic_int isPoolRunning_; //表示线程的启动状态
	

	

};


#endif // !
