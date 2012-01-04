#include "compressor.h"
#include <zlib.h>
#include "scope_guard.h"
#include "workaround.hpp"
#include "errors.h"
#include <stdio.h>
#include <fcntl.h>

using namespace es3;
using namespace boost::filesystem;

#define MINIMAL_BLOCK (1024*1024)

namespace es3
{
	struct compress_task : public sync_task
	{
		compressor_ptr parent_;
		uint64_t block_num_, offset_, size_, block_total_;

		virtual std::string get_class() const
		{
			return "compression"+int_to_string(get_class_limit());
		}
		virtual int get_class_limit() const
		{
			return parent_->context_->max_compressors_;
		}

		virtual void operator()(agenda_ptr agenda)
		{
			std::pair<std::string,uint64_t> res=do_compress();
			parent_->on_complete(res.first, block_num_, res.second);
		}

		std::pair<std::string,uint64_t> do_compress()
		{
			handle_t src(open(parent_->path_.c_str(), O_RDONLY));
			lseek64(src.get(), offset_, SEEK_SET) | libc_die;

			//Generate the temp name
			path tmp_nm = path(parent_->context_->scratch_path_) /
					unique_path("scratchy-%%%%-%%%%");
			handle_t tmp_desc(open(tmp_nm.c_str(), O_RDWR|O_CREAT));

			VLOG(2) << "Compressing part " << block_num_ << " out of " <<
					   block_total_ << " of " << parent_->path_;

			z_stream stream = {0};
			deflateInit2(&stream, 8, Z_DEFLATED,
							   15|16, //15 window bits | GZIP
							   8,
							   Z_DEFAULT_STRATEGY);
			ON_BLOCK_EXIT(&deflateEnd, &stream);

			char buf[65536*4];
			char buf_out[65536];
			size_t consumed=0;
			size_t raw_consumed=0;
			while(raw_consumed<size_)
			{
				size_t chunk = std::min(uint64_t(sizeof(buf)),
										size_-raw_consumed);
				ssize_t ln=read(src.get(), buf, chunk) | libc_die;
				assert(ln>0);
				raw_consumed+=ln;

				stream.avail_in = ln;
				stream.next_in = (Bytef*)buf;

				do
				{
					stream.avail_out= sizeof(buf_out);
					stream.next_out = (Bytef*)buf_out;
					int c_err=deflate(&stream, Z_NO_FLUSH);
					if (c_err!=Z_OK)
						err(errFatal) << "Failed to compress "
									  << parent_->path_;

					size_t cur_consumed=sizeof(buf_out) - stream.avail_out;
					write(tmp_desc.get(), buf_out, cur_consumed) | libc_die;
					consumed += cur_consumed;
				} while(stream.avail_in!=0);
			}
			assert(raw_consumed==size_);

			//We're writing the epilogue
			stream.avail_out= sizeof(buf_out);
			stream.next_out = (Bytef*)buf_out;
			int c_err=deflate(&stream, Z_FINISH);
			if (c_err!=Z_STREAM_END) //Epilogue must always fit
				err(errFatal) << "Failed to finish compression of "
							  << parent_->path_;
			size_t cur_consumed=sizeof(buf_out) - stream.avail_out;
			consumed += cur_consumed;
			write(tmp_desc.get(), buf_out, cur_consumed) | libc_die;

			return std::pair<std::string,uint64_t>(tmp_nm.c_str(), consumed);
		}
	};
}; //namespace es3

void file_compressor::operator()(agenda_ptr agenda)
{
	uint64_t file_sz=file_size(path_);
	if (file_sz<=MINIMAL_BLOCK)
	{
		handle_t desc(open(path_.c_str(), O_RDONLY));
		on_finish_(zip_result_ptr(new compressed_result(path_, desc.size())));
		return;
	}

	//Start compressing
	uint64_t estimate_num_blocks = file_sz / MINIMAL_BLOCK;
	assert(estimate_num_blocks>0);

	if (estimate_num_blocks> context_->max_compressors_)
		estimate_num_blocks = context_->max_compressors_;
	uint64_t block_sz = file_sz / estimate_num_blocks;
	assert(block_sz>0);
	uint64_t num_blocks = file_sz / block_sz +
			((file_sz%block_sz)==0?0:1);

	result_=zip_result_ptr(new compressed_result(num_blocks));
	num_pending_ = num_blocks;
	for(uint64_t f=0; f<num_blocks; ++f)
	{
		boost::shared_ptr<compress_task> ptr(new compress_task());
		ptr->parent_=shared_from_this();
		ptr->block_num_=f;
		ptr->block_total_=num_blocks;
		ptr->offset_=block_sz*f;
		ptr->size_=file_sz-ptr->offset_;
		if (ptr->size_>block_sz)
			ptr->size_=block_sz;

		agenda->schedule(ptr);
	}
}

void file_compressor::on_complete(const std::string &name, uint64_t num,
				 uint64_t resulting_size)
{
	{
		guard_t lock(m_);

		num_pending_--;
		result_->files_.at(num) = name;
		result_->sizes_.at(num) = resulting_size;
	}
	if (num_pending_==0)
	{
		on_finish_(result_);
	}
}
