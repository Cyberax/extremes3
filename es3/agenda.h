/*
Copyright (c) 2013, Illumina Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions 
are met:
. Redistributions of source code must retain the above copyright 
notice, this list of conditions and the following disclaimer.
. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the 
documentation and/or other materials provided with the distribution.
. Neither the name of the Illumina, Inc. nor the names of its 
contributors may be used to endorse or promote products derived from 
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef AGENDA_H
#define AGENDA_H

#include "common.h"
#include <boost/enable_shared_from_this.hpp>
//#include <condition_variable>

#define MIN_SEGMENT_SIZE (6*1024*1024)
#define MAX_IN_FLIGHT 200

namespace es3 {
	class agenda;
	typedef boost::shared_ptr<agenda> agenda_ptr;

	struct segment
	{
		std::vector<char> data_;
	};
	typedef boost::shared_ptr<segment> segment_ptr;

	enum task_type_e
	{
		taskUnbound,
		taskCPUBound,
		taskIOBound,
	};

	class sync_task
	{
	public:
		virtual ~sync_task() {}

		virtual task_type_e get_class() const { return taskUnbound; }
		virtual size_t needs_segments() const { return 0; }
		virtual int64_t ordinal() const
		{
			return 0;
		}
		virtual void operator()(agenda_ptr agenda,
								const std::vector<segment_ptr> &segments)
		{
			assert(segments.size()==0 && needs_segments()==0);
			operator ()(agenda);
		}
		virtual void operator()(agenda_ptr agenda){}
		virtual void print_to(std::ostream &str) = 0;
	};
	typedef boost::shared_ptr<sync_task> sync_task_ptr;

	inline bool operator < (sync_task_ptr p1, sync_task_ptr p2)
	{
		return p1->ordinal() < p2->ordinal();
	}

	class agenda : public boost::enable_shared_from_this<agenda>
	{
		std::map<task_type_e, size_t> class_limits_;
		const size_t max_segments_in_flight_, segment_size_;
		const bool quiet_, final_quiet_;
		struct timespec start_time_;

		mutex_t m_; //This mutex protects the following data {
		boost::condition_variable condition_;
		typedef std::multimap<int64_t, sync_task_ptr> task_map_t;
		typedef std::map<task_type_e, task_map_t> task_by_class_t;
		typedef std::map<size_t, task_by_class_t> size_map_t;
		std::map<size_t, task_by_class_t> tasks_;
		std::map<task_type_e, size_t> classes_;
		size_t num_working_;
		size_t segments_in_flight_;
		//}

		mutex_t stats_m_; //This mutex protects the following data {
		size_t num_submitted_, num_done_, num_failed_;
		std::map<std::string, std::pair<uint64_t, uint64_t> > progress_;
		std::map<std::string, uint64_t> cur_stats_;
		//}

		friend struct segment_deleter;
	public:
		agenda(size_t num_unbound, size_t num_cpu_bound,
			   size_t num_io_bound, bool quiet, bool final_quiet,
			   size_t def_segment_size, size_t max_segments_in_flight_);

		size_t get_capability(task_type_e tp) const
		{
			return class_limits_.at(tp);
		}
		void schedule(sync_task_ptr task);
		size_t run();

		void add_stat_counter(const std::string &stat, uint64_t val);

		size_t max_in_flight() const { return max_segments_in_flight_; }
		size_t segment_size() const { return segment_size_; }

		void print_queue();
		void print_epilog();
		size_t tasks_count() const { return tasks_.size(); }
	private:
		std::vector<segment_ptr> get_segments(size_t num);

		void draw_progress();
		void draw_progress_widget();
		void draw_stats();
		uint64_t get_elapsed_millis() const;
		std::pair<std::string, std::string> format_si(uint64_t val,
													  bool per_sec);

		friend class task_executor;
	};

}; //namespace es3

#endif //AGENDA_H
