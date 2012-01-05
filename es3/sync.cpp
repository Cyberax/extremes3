#include "sync.h"
#include "uploader.h"
#include "downloader.h"
#include "context.h"
#include "errors.h"
#include <set>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>

using namespace es3;

synchronizer::synchronizer(agenda_ptr agenda, const context_ptr &to)
	: agenda_(agenda), to_(to)
{
}

void synchronizer::create_schedule()
{
	//Retrieve the list of remote files
	s3_connection conn(to_);
	file_map_t remotes = conn.list_files(to_->remote_root_, "");
	process_dir(&remotes, to_->remote_root_, to_->local_root_);
}

void synchronizer::process_dir(file_map_t *remote_list,
	const std::string &remote_dir, const bf::path &local_dir)
{
	assert(remote_dir.at(remote_dir.size()-1)=='/');
	if(bf::status(local_dir).type()!=bf::directory_file)
		err(errFatal) << "Directory "<<local_dir<<" can't be accessed";

	file_map_t cur_remote_copy = remote_list ?
				*remote_list : file_map_t();

	for(bf::directory_iterator iter=bf::directory_iterator(local_dir);
		iter!=bf::directory_iterator(); ++iter)
	{
		const bf::directory_entry &dent = *iter;
		bf::path cur_local_path=bf::absolute(dent.path());
		std::string cur_local_filename=cur_local_path.filename().string();
		std::string cur_remote_path = remote_dir+cur_local_filename;

		remote_file_ptr cur_remote_child=try_get(
					cur_remote_copy, cur_local_filename, remote_file_ptr());
		cur_remote_copy.erase(cur_local_filename);

		if (dent.status().type()==bf::directory_file)
		{
			//Recurse into subdir
			if (cur_remote_child)
			{
				if (!cur_remote_child->is_dir_)
				{
					if (to_->delete_missing_)
					{
						if (to_->upload_)
						{
							sync_task_ptr task(new remote_file_deleter(to_,
								 cur_remote_child->full_name_));
							agenda_->schedule(task);
							process_dir(0, cur_remote_path+"/", cur_local_path);
						} else
						{
							sync_task_ptr task(new file_downloader(to_,
								cur_local_path, cur_remote_path, true));
							agenda_->schedule(task);
						}
					} else
					{
						VLOG(0) << "Local file "<< dent.path() << " "
								<< "has become a directory, but we're "
								<< "not allowed to remove it on the "
								<< "remote side";
					}
				} else
					process_dir(&cur_remote_child->children_,
						cur_remote_path+"/", cur_local_path);
			} else
			{
				if (to_->upload_)
				{
					process_dir(0, cur_remote_path+"/", cur_local_path);
				}
				else
				{
					sync_task_ptr task(new local_file_deleter(cur_local_path));
					agenda_->schedule(task);
				}
			}
		} else if (dent.status().type()==bf::regular_file)
		{
			//Regular file
			if (to_->upload_)
			{
				if (!cur_remote_child)
				{
					sync_task_ptr task(new file_uploader(
						to_, cur_local_path, cur_remote_path));
					agenda_->schedule(task);
				}
				else
				{
					sync_task_ptr task(new file_uploader(
						to_, cur_local_path, cur_remote_path));
					agenda_->schedule(task);
				}
			} else
			{
				if (!cur_remote_child)
				{
					sync_task_ptr task(new local_file_deleter(cur_local_path));
					agenda_->schedule(task);
				}
				else
				{
					sync_task_ptr task(new file_downloader(
						to_, cur_local_path, cur_remote_path));
					agenda_->schedule(task);
				}
			}
		} else
		{
			VLOG(0) << "Unknown local file type "<< cur_local_path.string();
		}
	}

	if (to_->delete_missing_ || !to_->upload_)
		process_missing(cur_remote_copy, local_dir);
}

void synchronizer::process_missing(const file_map_t &cur,
								   const bf::path &cur_local_dir)
{
	for(auto f = cur.begin(); f!=cur.end();++f)
	{
		std::string filename = f->first;
		remote_file_ptr file = f->second;

		if (file->is_dir_)
		{
			bf::path new_dir = cur_local_dir / filename;
			mkdir(new_dir.c_str(), 0755) |
					libc_die2("Failed to create "+new_dir.string());
			process_missing(file->children_, new_dir);
		} else
		{
			if (to_->upload_)
			{
				if (to_->delete_missing_)
					agenda_->schedule(sync_task_ptr(
						new remote_file_deleter(to_, file->full_name_)));
			} else
			{
				//Missing local file just means that we need to download it
				agenda_->schedule(sync_task_ptr(
					new file_downloader(to_, cur_local_dir / filename,
										file->full_name_)));
			}
		}
	}
}
