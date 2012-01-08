#ifndef AGENDA_H
#define AGENDA_H

#include "common.h"
#include <boost/enable_shared_from_this.hpp>
#include <condition_variable>

namespace es3 {
	struct conn_context;
	struct remote_file;
	typedef boost::shared_ptr<remote_file> remote_file_ptr;
	class agenda;
	typedef boost::shared_ptr<agenda> agenda_ptr;

	class sync_task
	{
	public:
		virtual ~sync_task() {}

		virtual std::string get_class() const { return "def"; }
		virtual size_t get_class_limit() const { return 0; }
		virtual void operator()(agenda_ptr agenda) = 0;
	};
	typedef boost::shared_ptr<sync_task> sync_task_ptr;

	class agenda : public boost::enable_shared_from_this<agenda>
	{
		agenda(size_t thread_num, bool quiet);
		const size_t thread_num_;
		const bool quiet_;

		std::mutex m_; //This mutex protects the following data {
		std::condition_variable condition_;
		std::vector<sync_task_ptr> tasks_;
		std::map<std::string, size_t> classes_;
		size_t num_working_;
		//}

		std::mutex stats_m_; //This mutex protects the following data {
		size_t num_submitted_, num_done_, num_failed_;
		std::map<std::string, std::pair<uint64_t, uint64_t> > progress_;
		//}
	public:
		static boost::shared_ptr<agenda> make_new(size_t thread_num,
												  bool quiet);

		int advise_capability() const { return thread_num_;}
		void schedule(sync_task_ptr task);
		size_t run();

		void draw_progress();
		void draw_progress_widget();

		friend class task_executor;
	};

}; //namespace es3

#endif //AGENDA_H
