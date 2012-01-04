#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include "common.h"
#include "agenda.h"
#include "context.h"
#include <functional>

namespace es3 {

	struct compressed_result
	{
		std::vector<std::string> files_;
		std::vector<uint64_t> sizes_;
		bool was_compressed_;

		compressed_result(size_t sz)
		{
			files_.resize(sz);
			sizes_.resize(sz);
			was_compressed_ = true;
		}

		compressed_result(const std::string &file, uint64_t sz)
		{
			files_.push_back(file);
			sizes_.push_back(sz);
			was_compressed_=false;
		}
		~compressed_result()
		{
			if (was_compressed_)
				for(auto f =files_.begin();f!=files_.end();++f)
					if (!f->empty()) unlink(f->c_str());
		}
	};
	typedef boost::shared_ptr<compressed_result> zip_result_ptr;
	typedef std::function<void(zip_result_ptr)> zipped_callback;

	class file_compressor : public sync_task,
			public boost::enable_shared_from_this<file_compressor>
	{
		std::mutex m_;

		context_ptr context_;
		const std::string path_;
		zipped_callback on_finish_;

		zip_result_ptr result_;
		volatile size_t num_pending_;

		friend struct compress_task;
	public:
		file_compressor(const std::string &path,
						context_ptr context,
						zipped_callback on_finish)
			: path_(path), context_(context), on_finish_(on_finish)
		{
		}
		virtual void operator()(agenda_ptr agenda);
	private:
		void on_complete(const std::string &name, uint64_t num,
						 uint64_t resulting_size);
	};

	typedef boost::shared_ptr<file_compressor> compressor_ptr;

}; //namespace es3

#endif //COMPRESSOR_H
