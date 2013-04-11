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
#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include "common.h"
#include "agenda.h"
#include <functional>
#include <boost/filesystem.hpp>

namespace es3 {
	namespace bf = boost::filesystem;

	class conn_context;
	typedef boost::shared_ptr<conn_context> context_ptr;

	struct scattered_files
	{
		std::vector<bf::path> files_;
		std::vector<uint64_t> sizes_;
		bool was_compressed_;

		scattered_files(size_t sz)
		{
			files_.resize(sz);
			sizes_.resize(sz);
			was_compressed_ = true;
		}

		scattered_files(const bf::path &file, uint64_t sz)
		{
			files_.push_back(file);
			sizes_.push_back(sz);
			was_compressed_=false;
		}
		~scattered_files()
		{
			if (was_compressed_)
				for(auto f =files_.begin();f!=files_.end();++f)
					if (!f->empty()) unlink(f->c_str());
		}
	};
	typedef boost::shared_ptr<scattered_files> files_ptr;
	typedef boost::function<void(files_ptr)> files_finished_callback;

	class file_compressor : public sync_task,
			public boost::enable_shared_from_this<file_compressor>
	{
		mutex_t m_;

		context_ptr context_;
		const bf::path path_;
		files_finished_callback on_finish_;

		files_ptr result_;
		volatile size_t num_pending_;

		friend struct compress_task;
	public:
		file_compressor(const bf::path &path,
						context_ptr context,
						files_finished_callback on_finish)
			: path_(path), context_(context), on_finish_(on_finish)
		{
		}
		virtual void operator()(agenda_ptr agenda);
		virtual void print_to(std::ostream &str)
		{
			str << "Compress " << path_;
		}
	private:
		void on_complete(const bf::path &name, uint64_t num,
						 uint64_t resulting_size);
	};
	typedef boost::shared_ptr<file_compressor> compressor_ptr;

	class file_decompressor : public sync_task,
			public boost::enable_shared_from_this<file_decompressor>
	{
		context_ptr context_;
		const bf::path source_;
		const bf::path result_;
		time_t mtime_;
		mode_t mode_;
		bool delete_on_stop_;
	public:
		file_decompressor(context_ptr context, const bf::path &source,
						  const bf::path &result, time_t mtime, mode_t mode,
						  bool delete_on_stop)
			: context_(context), source_(source), result_(result),
			  delete_on_stop_(delete_on_stop), mtime_(mtime), mode_(mode)
		{
		}
		~file_decompressor()
		{
			if (delete_on_stop_)
				unlink(source_.c_str());
		}

		virtual task_type_e get_class() const { return taskCPUBound; }
		virtual void operator()(agenda_ptr agenda);

		virtual void print_to(std::ostream &str)
		{
			str << "Decompress " << source_ << " to " << result_;
		}
	};

	bool should_compress(const bf::path &p, uint64_t sz);
}; //namespace es3

#endif //COMPRESSOR_H
