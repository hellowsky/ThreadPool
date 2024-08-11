#include <iostream>
#include"threadpool.h"
#include<chrono>
#include<thread>

using ULong = unsigned long long;

class MyTask : public Task
{
public:
	MyTask(int begin, int end)
		: begin_(begin)
		, end_(end) 
	{}
	
	Any run()
	{
		std::cout << "start threadFunc tid" << std::this_thread::get_id()<< std::endl;
		
		std::this_thread::sleep_for(std::chrono::seconds(3));
		ULong sum = 0;
		for (ULong i = begin_; i <= end_; i++)
		{ 
			sum += i;
		}
		std::cout << "end threadFunc tid" << std::this_thread::get_id() << std::endl;

		return sum;
	}

private:
	int begin_;
	int end_;
};


int main()
{
	{

		ThreadPool pool;
		pool.setMode(PoolMode::MODE_CACHED);
		pool.start(2);

		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		ULong sum1 = res1.get().cast_<ULong>();

		std::cout << sum1 << std::endl;
	
		//�����Result����ҲҪ���� ��vs�� �����������ͷ���Ӧ����Դ

	}
	std::cout << "end main" << std::endl;
	getchar();

#if 0
	//�̳߳ض��������� ��ν��̳߳���ص���Դ�ͷŵ���
	{
		ThreadPool pool;

		//�û��Լ����ã����ӳصĹ���ģʽ
		pool.setMode(PoolMode::MODE_CACHED);

		pool.start(4);

		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

		ULong sum1 = res1.get().cast_<ULong>();
		ULong sum2 = res2.get().cast_<ULong>();
		ULong sum3 = res3.get().cast_<ULong>();
		//std::cout << "res:" << res.get() << std::endl;
		std::cout << (sum1 + sum2 + sum3) << std::endl;

		/*pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());*/


		
	}
	getchar();

#endif
    return 0;
}