#ifndef AGENDA_H
#define AGENDA_H

#include "common.h"
#include <boost/enable_shared_from_this.hpp>
#include <condition_variable>

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
		virtual void operator()(agenda_ptr agenda) = 0;
	};
	typedef boost::shared_ptr<sync_task> sync_task_ptr;

	class agenda : public boost::enable_shared_from_this<agenda>
	{
		const std::map<task_type_e, size_t> class_limits_;
		const size_t max_segments_in_flight_, def_segment_size_;
		const bool quiet_;

		std::mutex m_; //This mutex protects the following data {
		std::condition_variable condition_;
		std::vector<sync_task_ptr> tasks_;
		std::map<task_type_e, size_t> classes_;
		size_t num_working_;
		//}

		std::mutex stats_m_; //This mutex protects the following data {
		size_t num_submitted_, num_done_, num_failed_;
		std::map<std::string, std::pair<uint64_t, uint64_t> > progress_;
		//}

		std::mutex segment_m_; //This mutex protects the following data {
		std::condition_variable segment_ready_condition_;
		size_t segments_in_flight_;
		//}

		friend struct segment_deleter;
	public:
		agenda(size_t num_unbound, size_t num_cpu_bound,
			   size_t num_io_bound, bool quiet,
			   size_t def_segment_size, size_t max_segments_in_flight_);

		size_t get_capability(task_type_e tp) const
		{
			return class_limits_.at(tp);
		}
		void schedule(sync_task_ptr task);
		size_t run();

		void draw_progress();
		void draw_progress_widget();

		segment_ptr get_segment();
		size_t segment_size() const { return def_segment_size_; }

		friend class task_executor;
	};

}; //namespace es3

#endif //AGENDA_H
