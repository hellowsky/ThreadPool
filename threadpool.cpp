#include "threadpool.h"
#include<functional>
#include<iostream>

const size_t TASK_MAX_THREAD_HOLD = INT32_MAX;

const size_t THREAD_MAX_THREADHOLD = 1024;

const int THREAD_MAX_IDLE_TIME = 60;  //��λ����

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

//����task��������ϵ���ֵ
void ThreadPool::setTaskQueMaxThreadHold(int threshhold)
{
	//�����������̳߳غ��޸Ķ�����ֵ
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

//���̳߳��ύ����  �û����øýӿ�,�����������,��������
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	//�߳�ͨ�� ��������������,�������߳�����,�ȴ��������߳���������
	//while(taskQue_.size() >= taskQueMaxThreadHold_)
	//{
	//	//notFull_.wait(lock, [&]() {return taskQue_.size() < taskQueMaxThreadHold_; });
	//	notFull_.wait(lock);
	//}
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]() {return taskQue_.size() < taskQueMaxThreadHold_; }))
	{
		//notFull��������Ȼ������
		std::cerr << "task queue is full submit task fail .ThreadPool::submitTask timeout" << std::endl;
		//return task->getResult(); //Task Result �߳�ִ����task,task����ͱ�������
		return Result(sp,false);
	}

	//����ж����п���,�������߳��������񲢽�������������
	taskQue_.emplace(sp);
	taskSize_++;


	//��Ϊ�Ѿ�������з���������,������в�Ϊ��,notEmpty_��������֪ͨ�������̸߳Ͽ�����߳�ִ������
	notEmpty_.notify_all();

	//cacheģʽ ������ȽϽ��� ����:С����
	//�������������Ϳ����̵߳�����,�ж��Ƿ���Ҫ�����µ��߳�
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThreadHold_)
	{

		std::cout << "<<< tid " << std::this_thread::get_id() << " create new thread " << curThreadSize_ << std::endl;

		//�������߳�
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		//�����߳�
		threads_[threadId]->start();
		//��ǰ�߳�����+1 
		curThreadSize_++;
		//��¼�����߳�����
		idleThreadSize_++;
	}


	//���������Result����
	//return task->getResult();
	return Result(sp);
}

void ThreadPool::start(int initThreadSize = std::thread::hardware_concurrency())
{
	//�����̳߳ص�����״̬
	isPoolRunning_ = true;

	//��¼��ʼ�߳�����
	initThreadSize_ = initThreadSize;

	//��¼��ǰ�߳�����
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
		idleThreadSize_++;	   //��¼���е��߳�����
	}

}

//�̺߳��� �̳߳��е������̴߳���������л�ȡ����
//������������û������,�̳߳��е��̻߳ᴦ������״̬
void ThreadPool::threadFunc(int threadid)//�̺߳������� ��Ӧ���߳�Ҳ�ͽ�����
{
	auto lastTime = std::chrono::high_resolution_clock::now();

	//�������������ȫִ����֮��,�̳߳زſ��Ի��������߳���Դ
	for(;;)
	{

		std::shared_ptr<Task> task;
		{
			//�Ȼ�ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout<<"tid "<< std::this_thread::get_id() <<" Attempt to Acquire Task..."<<std::endl;

			//cacheģʽ�� �п��ܴ����˺ܶ��߳� ���ǿ���ʱ�䳬����60s Ӧ�ðѶ�����߳̽������յ�
			//�������յ�(����intiThreadSize_�������߳�Ҫ���л���)
			//��ǰʱ�� - ��һ���߳�ִ�������ʱ��   >  60s
			
			
				//ÿһ���ӷ���һ��  ��ô���ֳ�ʱ���ػ����������ִ�з���
				//����˫���ж�
			while (taskQue_.size() == 0)
			{
				if (!isPoolRunning_)
				{
					threads_.erase(threadid);
					std::cout << "tid" << std::this_thread::get_id() << " Thread Exit..." << std::endl;
					exitCond_.notify_all();
					return;//�̺߳������� �߳��˳�
				}


				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					//����������ʱ����
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock::now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
						{

							//��ʼ���յ�ǰ�߳�
							//��¼�߳���������ر�����ֵ�޸�
							//���̶߳�����̳߳���ɾ�� û�а취ƥ�䵱ǰ��threadFunc ������һ���̶߳���
							//�߳�id ->�̶߳���->ɾ��
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
					 
					//�ȴ�notEmpty����
					//1.���ж���������Ƿ�Ϊ��,Ϊ��,���߳������ȴ�
					notEmpty_.wait(lock);
				}  
				//�̳߳عر�,��ǰ�߳�Ҳ�ر�
			/*	if (isPoolRunning_ == false)
				{
					threads_.erase(threadid);
					std::cout << "tid" << std::this_thread::get_id() << " �߳�ִ�г�ʱ����..." << std::endl;
					exitCond_.notify_all();
					return;
				}*/
			}

			//�̳߳عر�,���������Դ
			//if (isPoolRunning_ == false)
			//{
			///*	threads_.erase(threadid);
			//	std::cout << "tid" << std::this_thread::get_id() << " �߳�ִ�г�ʱ����..." << std::endl;
			//	exitCond_.notify_all();
			//	return;*/
			//	break;
			//}


			
			//������в�Ϊ�գ����̴߳Ӷ�����ȡ����ִ�� �����߳�������һ
			idleThreadSize_--;

			//std::cout << "tid" << std::this_thread::get_id() << " ����ɹ�..." << std::endl;
		
			//2.������в�Ϊ��,���̴߳Ӷ�����ȡ����ִ��
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--; 

			//3.�����������,��֪ͨ�������߳�
			if (taskSize_> 0)
			{
				notEmpty_.notify_all();
			}

			//3.�߳�ִ��������,��֪ͨ�������߳�,�����л�������
			notFull_.notify_all();

		}//�ͷ��� 

		//4.ִ������
		if (task != nullptr)
		{
			//task->run();//ִ������ ������ķ���ֵͨ��serVal���õ�Result������
			task->exec();
		}

		
		idleThreadSize_++;//�߳�ִ��������,�����߳�������һ
		lastTime = std::chrono::high_resolution_clock::now();//�����߳�ִ���������ʱ��

	}
	
}

bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}


////////////////////////�̷߳���ʵ��
int Thread::generateId_ = 0;

Thread::Thread(ThreadFunc func) 
	:func_(func)
	, threadId_(generateId_++)
{}

Thread::~Thread()
{
}

void Thread::start()
{	//�����߳���ִ���̺߳��� func_
	std::thread t(func_,threadId_);

	t.detach();
}

int Thread::getId() const
{
	return threadId_;
}


///////////////////////////     Task������ʵ��
Task::Task()
	:result_(nullptr)
{}


void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run()); //������̬
	}
	
}

void Task::setResult(Result* res)
{
	result_ = res;
}


///////////////////////////     Result������ʵ��
Result::Result(std::shared_ptr<Task> task, bool isValid)
	:isValid_(isValid)
	, task_(task) 
{
	task_->setResult(this);
}


void Result::setVal(Any any)
{
	//�洢task�ķ���ֵ
	this->any_ = std::move(any);
	sem_.post();  // �Ѿ���ȡ����ķ���ֵ,�����ź�����Դ
}

Any Result::get()//�û����õĽӿ�
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait();  //task�������û��ִ����������û����߳�
	return std::move(any_);
}

