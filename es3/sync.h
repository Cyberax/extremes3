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
#ifndef SYNC_H
#define SYNC_H

#include <common.h>
#include "agenda.h"
#include "connection.h"
#include <stdint.h>

namespace es3 {
	struct local_file;
	struct local_dir;
	typedef boost::shared_ptr<local_file> local_file_ptr;
	typedef boost::shared_ptr<local_dir> local_dir_ptr;

	class synchronizer
	{
		agenda_ptr agenda_;
		context_ptr ctx_;
		std::vector<s3_path> remote_;
		stringvec local_;
		bool do_upload_;
		bool delete_missing_;
		stringvec included_, excluded_;
	public:
		synchronizer(agenda_ptr agenda, const context_ptr &ctx,
					 std::vector<s3_path> remote, stringvec local,
					 bool do_upload, bool delete_missing,
					 const stringvec &included, const stringvec &excluded);
		bool create_schedule(bool check_mode, bool delete_mode, 
							 bool non_recursive_delete);
	private:
		void process_upload(local_dir_ptr locals, s3_directory_ptr remotes,
							const s3_path &remote_path, bool check_mode);
		void process_downloads(s3_directory_ptr remotes, local_dir_ptr locals,
							   const bf::path &local_path, bool check_mode);

		void delete_possibly_recursive(s3_directory_ptr dir, bool non_recursive);
	};

	s3_directory_ptr schedule_recursive_walk(const s3_path &remote, 
											 context_ptr ctx, agenda_ptr ag);
	void schedule_recursive_publication(const s3_path &remote, 
		context_ptr ctx, agenda_ptr ag, 
		const stringvec &included, const stringvec &excluded, size_t *num);
    void schedule_recursive_list(const s3_path &remote,
        context_ptr ctx, agenda_ptr ag,
        const stringvec &included, const stringvec &excluded, size_t *num_files);

}; //namespace es3

#endif //UPLOADER_H
