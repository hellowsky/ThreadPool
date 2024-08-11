#include "threadpool.h"
#include<functional>
#include<iostream>

const size_t TASK_MAX_THREAD_HOLD = INT32_MAX;

const size_t THREAD_MAX_THREADHOLD = 1024;

const int THREAD_MAX_IDLE_TIME = 60;  //单位是秒

ThreadPool::ThreadPool()
	: initThreadSize_(0)
	, taskSize_(0)
	, idleThreadSize_(0)
	, curThreadSize_(0)
	, taskQueMaxThreadHold_(TASK_MAX_THREAD_HOLD)
	, threadSizeThreadHold_(THREAD_MAX_THREADHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolRunning_(false)
{};


ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;
	
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();

	exitCond_.wait(lock, [&]()->bool {
		return threads_.size() == 0;
		});

}


void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())
	{
		return;
	}
	poolMode_ = mode;
}

//设置task任务队列上的阈值
void ThreadPool::setTaskQueMaxThreadHold(int threshhold)
{
	//不允许启动线程池后修改队列阈值
	if (checkRunningState())
	{
		return;
	}

	taskQueMaxThreadHold_ = threshhold;
}

void ThreadPool::setThreadSizeThreadHold(int threshhold)
{
	if (checkRunningState())
	{
		return;
	}
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadSizeThreadHold_ = threshhold;
	}
	
}

//给线程池提交任务  用户调用该接口,传入任务对象,生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	//线程通信 队列中任务满了,生产者线程阻塞,等待消费者线程消费任务
	//while(taskQue_.size() >= taskQueMaxThreadHold_)
	//{
	//	//notFull_.wait(lock, [&]() {return taskQue_.size() < taskQueMaxThreadHold_; });
	//	notFull_.wait(lock);
	//}
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]() {return taskQue_.size() < taskQueMaxThreadHold_; }))
	{
		//notFull，条件依然不满足
		std::cerr << "task queue is full submit task fail .ThreadPool::submitTask timeout" << std::endl;
		//return task->getResult(); //Task Result 线程执行完task,task对象就被析构了
		return Result(sp,false);
	}

	//如果有队列有空余,生产者线程生产任务并将任务放入队列中
	taskQue_.emplace(sp);
	taskSize_++;


	//因为已经向队列中放入了任务,任务队列不为空,notEmpty_条件变量通知消费者线程赶快分配线程执行任务
	notEmpty_.notify_all();

	//cache模式 任务处理比较紧急 场景:小而快
	//根据任务数量和空闲线程的数量,判断是否需要创建新的线程
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThreadHold_)
	{

		std::cout << "<<< tid " << std::this_thread::get_id() << " create new thread " << curThreadSize_ << std::endl;

		//创建新线程
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		//启动线程
		threads_[threadId]->start();
		//当前线程数量+1 
		curThreadSize_++;
		//记录空闲线程数量
		idleThreadSize_++;
	}


	//返回任务的Result对象
	//return task->getResult();
	return Result(sp);
}

void ThreadPool::start(int initThreadSize = std::thread::hardware_concurrency())
{
	//设置线程池的运行状态
	isPoolRunning_ = true;

	//记录初始线程数量
	initThreadSize_ = initThreadSize;

	//记录当前线程数量
	curThreadSize_ = initThreadSize;

	for (int i = 0; i < initThreadSize_; i++)
	{
		//std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		//threads_.emplace_back(std::move(ptr));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
	}
	for (int i = 0; i < initThreadSize_; i++)
	{ 
		threads_[i]->start();  //
		idleThreadSize_++;	   //记录空闲的线程数量
	}

}

//线程函数 线程池中的所有线程从任务队列中获取任务
//如果任务队列中没有任务,线程池中的线程会处于阻塞状态
void ThreadPool::threadFunc(int threadid)//线程函数返回 相应的线程也就结束了
{
	auto lastTime = std::chrono::high_resolution_clock::now();

	//所有任务必须完全执行完之后,线程池才可以回收所有线程资源
	for(;;)
	{

		std::shared_ptr<Task> task;
		{
			//先获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout<<"tid "<< std::this_thread::get_id() <<" Attempt to Acquire Task..."<<std::endl;

			//cache模式下 有可能创建了很多线程 但是空闲时间超过了60s 应该把多余的线程结束回收掉
			//结束回收掉(超过intiThreadSize_数量的线程要进行回收)
			//当前时间 - 上一次线程执行任务的时间   >  60s
			
			
				//每一秒钟返回一次  怎么区分超时返回还是有任务待执行返回
				//锁加双重判断
			while (taskQue_.size() == 0)
			{
				if (!isPoolRunning_)
				{
					threads_.erase(threadid);
					std::cout << "tid" << std::this_thread::get_id() << " Thread Exit..." << std::endl;
					exitCond_.notify_all();
					return;//线程函数结束 线程退出
				}


				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					//条件变量超时返回
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock::now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
						{

							//开始回收当前线程
							//记录线程数量的相关变量的值修改
							//把线程对象从线程池中删除 没有办法匹配当前的threadFunc 属于哪一个线程对象
							//线程id ->线程对象->删除
							threads_.erase(threadid);
							curThreadSize_--;
							idleThreadSize_--;
							std::cout << "tid" << std::this_thread::get_id() << " Thread Execution Timeout Reclamation..." << std::endl;
							return;

						}

					}
				}
				else
				{
					 
					//等待notEmpty条件
					//1.先判断任务队列是否为空,为空,则线程阻塞等待
					notEmpty_.wait(lock);
				}  
				//线程池关闭,则当前线程也关闭
			/*	if (isPoolRunning_ == false)
				{
					threads_.erase(threadid);
					std::cout << "tid" << std::this_thread::get_id() << " 线程执行超时回收..." << std::endl;
					exitCond_.notify_all();
					return;
				}*/
			}

			//线程池关闭,回收相关资源
			//if (isPoolRunning_ == false)
			//{
			///*	threads_.erase(threadid);
			//	std::cout << "tid" << std::this_thread::get_id() << " 线程执行超时回收..." << std::endl;
			//	exitCond_.notify_all();
			//	return;*/
			//	break;
			//}


			
			//任务队列不为空，则线程从队列中取任务执行 空闲线程数量减一
			idleThreadSize_--;

			//std::cout << "tid" << std::this_thread::get_id() << " 任务成功..." << std::endl;
		
			//2.任务队列不为空,则线程从队列中取任务执行
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--; 

			//3.如果还有任务,则通知消费者线程
			if (taskSize_> 0)
			{
				notEmpty_.notify_all();
			}

			//3.线程执行完任务,则通知生产者线程,队列中还有任务
			notFull_.notify_all();

		}//释放锁 

		//4.执行任务
		if (task != nullptr)
		{
			//task->run();//执行任务 把任务的返回值通过serVal设置到Result对象中
			task->exec();
		}

		
		idleThreadSize_++;//线程执行完任务,空闲线程数量加一
		lastTime = std::chrono::high_resolution_clock::now();//更新线程执行完任务的时间

	}
	
}

bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}


////////////////////////线程方法实现
int Thread::generateId_ = 0;

Thread::Thread(ThreadFunc func) 
	:func_(func)
	, threadId_(generateId_++)
{}

Thread::~Thread()
{
}

void Thread::start()
{	//创建线程来执行线程函数 func_
	std::thread t(func_,threadId_);

	t.detach();
}

int Thread::getId() const
{
	return threadId_;
}


///////////////////////////     Task方法的实现
Task::Task()
	:result_(nullptr)
{}


void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run()); //发生多态
	}
	
}

void Task::setResult(Result* res)
{
	result_ = res;
}


///////////////////////////     Result方法的实现
Result::Result(std::shared_ptr<Task> task, bool isValid)
	:isValid_(isValid)
	, task_(task) 
{
	task_->setResult(this);
}


void Result::setVal(Any any)
{
	//存储task的返回值
	this->any_ = std::move(any);
	sem_.post();  // 已经获取任务的返回值,增加信号量资源
}

Any Result::get()//用户调用的接口
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait();  //task任务如果没有执行完会阻塞用户的线程
	return std::move(any_);
}

