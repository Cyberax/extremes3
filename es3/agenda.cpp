#include "agenda.h"
#include "errors.h"
#include <unistd.h>
#include <thread>
#include <sstream>
#include <unistd.h>
#include <boost/bind.hpp>
#include <time.h>

using namespace es3;

agenda::agenda(size_t num_unbound, size_t num_cpu_bound, size_t num_io_bound,
			   bool quiet, bool final_quiet,
			   size_t segment_size, size_t max_segments_in_flight) :
	class_limits_ {{taskUnbound, num_unbound},
				   {taskCPUBound, num_cpu_bound},
				   {taskIOBound, num_io_bound}},
	quiet_(quiet), final_quiet_(final_quiet), segment_size_(segment_size),
	max_segments_in_flight_(max_segments_in_flight),
	num_working_(), num_submitted_(), num_done_(), num_failed_(),
	segments_in_flight_()
{
	clock_gettime(CLOCK_MONOTONIC, &start_time_) | libc_die2("Can't get time");
}

namespace es3
{
	struct segment_deleter
	{
		agenda_ptr parent_;

		void operator()(segment *seg)
		{
			delete seg;

			u_guard_t guard(parent_->segment_m_);
			assert(parent_->segments_in_flight_>0);
			parent_->segments_in_flight_--;
			parent_->segment_ready_condition_.notify_all();
		}
	};

	class task_executor
	{
		agenda_ptr agenda_;
	public:
		task_executor(agenda_ptr agenda) : agenda_(agenda) {}

		sync_task_ptr claim_task()
		{
			while(true)
			{
				u_guard_t lock(agenda_->m_);
				if (agenda_->tasks_.empty())
				{
					if (agenda_->num_working_==0)
						return sync_task_ptr();
				}

				for(auto iter=agenda_->tasks_.begin();
					iter!=agenda_->tasks_.end();++iter)
				{
					sync_task_ptr cur_task = *iter;
					//Check if there are too many tasks of this type running
					task_type_e cur_class=cur_task->get_class();
					size_t cur_num=agenda_->classes_[cur_class];
					//Unbound tasks are allowed to exceed their limits and
					//borrow threads from other classes
					if (cur_class!=taskUnbound &&
							agenda_->get_capability(cur_class)<=cur_num)
						continue;

					agenda_->tasks_.erase(iter);
					agenda_->num_working_++;
					agenda_->classes_[cur_class]++;
					return cur_task;
				}

				agenda_->condition_.wait(lock);
			}
		}

		void cleanup(sync_task_ptr cur_task, bool fail)
		{
			u_guard_t lock(agenda_->m_);
			agenda_->num_working_--;
			assert(agenda_->classes_[cur_task->get_class()]>0);
			agenda_->classes_[cur_task->get_class()]--;

			if (agenda_->tasks_.empty() && agenda_->num_working_==0)
				agenda_->condition_.notify_all(); //We've finished our tasks!
			else
				agenda_->condition_.notify_one();

			//Update stats
			guard_t lockst(agenda_->stats_m_);
			agenda_->num_done_++;
			if (fail)
				agenda_->num_failed_++;
		}

		void operator ()()
		{
			while(true)
			{
				sync_task_ptr cur_task=claim_task();
				if (!cur_task)
					break;

				bool fail=true;
				for(int f=0; f<10; ++f)
				{
					try
					{
						(*cur_task)(agenda_);
						fail=false;
						break;
					} catch (const es3_exception &ex)
					{
						const result_code_t &code = ex.err();
						if (code.code()==errNone)
						{
							VLOG(2) << "INFO: " << ex.what();
							sleep(5);
							continue;
						} else if (code.code()==errWarn)
						{
							VLOG(1) << "WARN: " << ex.what();
							sleep(5);
							continue;
						} else
						{
							VLOG(0) << ex.what();
							break;
						}
					} catch(const std::exception &ex)
					{
						VLOG(0) << "ERROR: " << ex.what();
						break;
					} catch(...)
					{
						VLOG(0) << "Unknown exception. Skipping";
						break;
					}
				}

				cleanup(cur_task, fail);
			}
		}
	};
}

segment_ptr agenda::get_segment()
{
	u_guard_t guard(segment_m_);
	while(segments_in_flight_>max_segments_in_flight_)
		segment_ready_condition_.wait(guard);

	segment_deleter del {shared_from_this()};
	segment_ptr res=segment_ptr(new segment(), del);
	segments_in_flight_++;
	return res;
}

void agenda::schedule(sync_task_ptr task)
{
	u_guard_t lock(m_);
	agenda_ptr ptr = shared_from_this();
	tasks_.push_back(task);
	condition_.notify_one();

	guard_t lockst(stats_m_);
	num_submitted_++;
}

size_t agenda::run()
{
	std::vector<std::thread> threads;
	size_t thread_num=0;
	for(auto iter=class_limits_.begin();iter!=class_limits_.end();++iter)
		thread_num+=iter->second;

	for(int f=0;f<thread_num;++f)
		threads.push_back(std::thread(task_executor(shared_from_this())));

	if (!quiet_)
	{
		threads.push_back(std::thread(boost::bind(&agenda::draw_progress,
												  this)));
		for(int f=0;f<threads.size();++f)
			threads.at(f).join();

		//Draw progress the last time
		draw_progress_widget();
		std::cerr<<std::endl;
	} else
	{
		for(int f=0;f<threads.size();++f)
			threads.at(f).join();
	}

	if (!final_quiet_)
		draw_stats();

	return num_failed_;
}

void agenda::draw_progress()
{
	while(true)
	{
		{
			guard_t g(m_);
			if (num_working_==0 && tasks_.empty())
				return;
		}
		draw_progress_widget();
		usleep(500000);
	}
}

void agenda::add_stat_counter(const std::string &stat, uint64_t val)
{
	guard_t lockst(stats_m_);
	cur_stats_[stat]+=val;
}

void agenda::draw_progress_widget()
{
	std::stringstream str;
	{
		guard_t lockst(stats_m_);
		str << "Tasks: [" << num_done_ << "/" << num_submitted_
			<< "]";
		if (num_failed_)
			str << " Failed tasks: " << num_failed_;
		str << "\r";
	}

	std::cerr << str.str(); //No std::endl
	std::cerr.flush();
}

uint64_t agenda::get_elapsed_millis() const
{
	struct timespec cur;
	clock_gettime(CLOCK_MONOTONIC, &cur) | libc_die2("Can't get time");
	uint64_t start_tm = uint64_t(start_time_.tv_sec)*1000 +
			start_time_.tv_nsec/1000000;

	uint64_t cur_tm = uint64_t(cur.tv_sec)*1000+cur.tv_nsec/1000000;
	return cur_tm-start_tm;
}

void agenda::draw_stats()
{
	uint64_t el = get_elapsed_millis();

	std::cerr << "time taken [sec]: " << el/1000 << "." << el%1000 << std::endl;
	for(auto f=cur_stats_.begin();f!=cur_stats_.end();++f)
	{
		std::string name=f->first;
		uint64_t val=f->second;

		uint64_t avg = val*1000/el;

		std::cerr << name << " [B]: " << val
				  << ", average [B/sec]: " << avg
				  << std::endl;
	}
}
