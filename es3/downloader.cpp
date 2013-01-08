#include "downloader.h"
#include "compressor.h"
#include "context.h"
#include "errors.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include "commands.h"
#include "scope_guard.h"

using namespace es3;
using namespace boost::filesystem;

void local_file_deleter::operator ()(agenda_ptr agenda_)
{
	VLOG(1) << "Removing " << file_;
	bf::remove_all(file_);
}

struct download_content
{
	mutex_t m_;
	context_ptr ctx_;

	time_t mtime_;
	mode_t mode_;
	size_t num_segments_, segments_read_;
	size_t remote_size_, raw_size_;

	s3_path remote_path_;
	bf::path local_file_, target_file_;
	bool delete_temp_file_, compressed_;

	download_content() : mtime_(), num_segments_(), segments_read_(),
		remote_size_(), raw_size_(), delete_temp_file_(true),
		mode_(0664), compressed_() {}
	~download_content()
	{
		if (local_file_!=target_file_ && delete_temp_file_)
			unlink(local_file_.c_str());
	}
};
typedef boost::shared_ptr<download_content> download_content_ptr;

class write_segment_task: public sync_task,
		public boost::enable_shared_from_this<write_segment_task>
{
	download_content_ptr content_;
	size_t cur_segment_;
	segment_ptr seg_;
public:
	write_segment_task(download_content_ptr content,
					   size_t cur_segment, segment_ptr seg) :
		content_(content), cur_segment_(cur_segment), seg_(seg)
	{
	}

	virtual task_type_e get_class() const { return taskIOBound; }

	virtual void print_to(std::ostream &str)
	{
		str << "Write segment " << cur_segment_ << " of "
			<< content_->local_file_;
	}

	virtual void operator()(agenda_ptr agenda)
	{
		context_ptr ctx = content_->ctx_;
		do_write(agenda);

		guard_t lock(content_->m_);
		content_->segments_read_++;
		if (content_->segments_read_==content_->num_segments_)
		{
			//Check if we need to decompress the file
			if (content_->compressed_)
			{
				//Yep, we do need to decompress it
				sync_task_ptr dl(new file_decompressor(ctx,
					content_->local_file_, content_->target_file_,
					content_->mtime_, content_->mode_, true));
				//file decompressor will delete it
				content_->delete_temp_file_=false;
				agenda->schedule(dl);
			} else
			{
				std::string local_nm=content_->local_file_.string();
				std::string tgt_nm=content_->target_file_.string();
				bf::last_write_time(local_nm, content_->mtime_);
				chmod(local_nm.c_str(), content_->mode_)
						| libc_die2("Failed to set mode on "+tgt_nm);
				rename(local_nm.c_str(), tgt_nm.c_str())
						| libc_die2("Failed to replace "+tgt_nm);
			}
		}
	}

	void do_write(agenda_ptr agenda)
	{
		context_ptr ctx = content_->ctx_;
		uint64_t start_offset = agenda->segment_size()*cur_segment_;
		handle_t fl(open(content_->local_file_.c_str(), O_RDWR) | libc_die);
		lseek64(fl.get(), start_offset, SEEK_SET) | libc_die;

		size_t offset = 0;
		while(offset<seg_->data_.size())
		{
			size_t chunk=std::min(seg_->data_.size()-offset, size_t(1024*1024));
			size_t res=write(fl.get(), &seg_->data_[offset],
							 chunk) | libc_die;
			assert(res!=0);
			offset+=res;
		}
	}
};

class download_segment_task: public sync_task,
		public boost::enable_shared_from_this<download_segment_task>
{
	download_content_ptr content_;
	size_t cur_segment_;
public:
	download_segment_task(download_content_ptr content, size_t cur_segment) :
		content_(content), cur_segment_(cur_segment)
	{
	}

	virtual void print_to(std::ostream &str)
	{
		str << "Download segment " << cur_segment_ << " of "
			<< content_->local_file_;
	}

	virtual size_t needs_segments() const { return 1; }

	virtual void operator()(agenda_ptr agenda,
							const std::vector<segment_ptr> &segments)
	{
		segment_ptr seg=segments.at(0);
		size_t segment_size=agenda->segment_size();

		uint64_t start_offset = segment_size*cur_segment_;
		uint64_t size = content_->remote_size_-start_offset;
		if (size>segment_size)
			size=segment_size;

		VLOG(2) << "Downloading part " << cur_segment_ << " out of "
				<< content_->num_segments_ << " of " << content_->remote_path_;

		s3_connection conn(content_->ctx_);
		seg->data_.resize(safe_cast<size_t>(size));
		conn.download_data(content_->remote_path_, start_offset,
						   &seg->data_[0], safe_cast<size_t>(size));
		agenda->add_stat_counter("downloaded", size);

		VLOG(2) << "Finished downloading part " << cur_segment_ << " out of "
				<< content_->num_segments_ << " of " << content_->remote_path_;

		//Now write the resulting segment
		sync_task_ptr dl(new write_segment_task(content_, cur_segment_, seg));
		agenda->schedule(dl);
	}
};

void file_downloader::operator()(agenda_ptr agenda)
{
	if (delete_dir_)
		bf::remove_all(path_);

	VLOG(2) << "Checking download of " << path_ << " from "
			  << remote_;

	uint64_t file_sz=0;
	time_t mtime=0;
	try
	{
		file_sz=file_size(path_);
		mtime=last_write_time(path_);
	} catch(const bf::filesystem_error&) {}

	//Check the modification date of the file locally and on the
	//remote side
	s3_connection up(conn_);
	file_desc mod=up.find_mtime_and_size(remote_);
	if (!mod.found_)
		err(errFatal) << "Document not found at: " << remote_;
	
	if(mod.mtime_)
	{
		if (mod.mtime_==mtime && mod.raw_size_==file_sz)
			return; //TODO: add an optional MD5 check?
	}

	//We need to download the file
	download_content_ptr dc(new download_content());
	dc->ctx_=conn_;

	size_t seg_size = agenda->segment_size();
	size_t seg_num = safe_cast<size_t>(mod.remote_size_/seg_size +
				((mod.remote_size_%seg_size)==0?0:1));
	if (seg_num>MAX_SEGMENTS)
		err(errFatal) << "Segment size is too small for " << remote_;
	if (seg_num==0)
	{
		assert(mod.remote_size_==0);
		seg_num=1;
	}

	dc->mtime_=mod.mtime_;
	dc->mode_=mod.mode_;
	dc->num_segments_=seg_num;
	dc->segments_read_=0;
	dc->remote_size_=mod.remote_size_;
	dc->raw_size_=mod.raw_size_;
	dc->compressed_=mod.compressed_;

	dc->remote_path_=remote_;
	dc->target_file_=path_;

	VLOG(2) << "Downloading " << path_ << " from " << remote_;

	if (mod.compressed_)
	{
		path tmp_nm = conn_->scratch_dir_ /
				bf::unique_path("scratchy-%%%%-%%%%-%%%%-%%%%-dl");
		dc->local_file_=tmp_nm.c_str();
	} else
	{
		path tmp_nm = path_.string()+"-%%%%%%%%-es3tmp";
		dc->local_file_=bf::unique_path(tmp_nm);
	}

	{
		unlink(dc->local_file_.c_str()); //Prevent some access right foulups
		handle_t fl(open(dc->local_file_.c_str(), O_RDWR|O_CREAT, 0600)
					| libc_die2("Failed to create file "
							   +dc->local_file_.string()));
#ifndef __MACH__		
		fallocate64(fl.get(), 0, 0, dc->remote_size_);
#else
		fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, (off_t)dc->remote_size_};
		// OK, perhaps we are too fragmented, allocate non-continuous
	    store.fst_flags = F_ALLOCATEALL;
	    int ret = fcntl(fl.get(), F_PREALLOCATE, &store);
	    if (ret!=-1)
			ftruncate(fl.get(), (off_t)dc->remote_size_);
#endif
	}

	for(size_t f=0;f<seg_num;++f)
	{
		sync_task_ptr dl(new download_segment_task(dc, f));
		agenda->schedule(dl);
	}
}

int es3::do_cat(context_ptr context, const stringvec& params,
		 agenda_ptr ag, bool help)
{
	if (help)
	{
		std::cout << "Cat syntax: es3 cat <PATHS>\n"
				  << "where <PATHS> are:\n"
				  << "\t - Amazon S3 storage (in s3://<bucket>/path/fl format)"
				  << std::endl << std::endl;
		return 0;
	}
	if (params.empty())
	{
		std::cerr << "ERR: at least one <PATH> must be specified.\n";
		return 2;
	}

	for(auto iter=params.begin();iter!=params.end();++iter)
	{
		bf::path tmp_nm = context->scratch_dir_ /
				bf::unique_path("scratchy-cat-%%%%-%%%%-%%%%-%%%%-dl");
		ON_BLOCK_EXIT(unlink, tmp_nm.c_str());
		
		s3_path remote=parse_path(*iter);
		s3_connection conn(context);
		std::string region=conn.find_region(remote.bucket_);
		remote.zone_=region;

        size_t failed;
        for (int f=0;f<6;++f)
        {
            sync_task_ptr task(new file_downloader(context, tmp_nm, remote, false));
            ag->schedule(task);
            failed=ag->run();
            if (!failed)
                break;
        }

        if (ag->tasks_count())
        {
            ag->print_epilog(); //Print stats, so they're at least visible
            std::cerr << "ERR: ";
            ag->print_queue();
            return 4;
        }
        if (failed)
            return 6;

		handle_t fl(open(tmp_nm.c_str(), O_RDWR) 
					| libc_die2("Failed to open "+tmp_nm.string()));
		
		fflush(stdout);
		while(true)
		{
			char buf[16385];
			int res=read(fl.get(), buf, 16384);
			if (res<0)
				res | libc_die;
			if (res==0)
				break;
			
			int written=write(1, buf, res);
			if (written!=res)
			{
				std::cerr << "ERR: failed to write "<<res<<" bytes to stdout";
				return 5;
			}
		}
	}
	
	return 0;
}
