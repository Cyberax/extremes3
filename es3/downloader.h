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
#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include "connection.h"
#include "common.h"
#include "agenda.h"

namespace es3 {

	class file_downloader : public sync_task
	{
		const context_ptr conn_;
		const bool delete_dir_;
		const bf::path path_;
		const s3_path remote_;

	public:
		file_downloader(const context_ptr &conn,
					  const bf::path &path,
					  const s3_path &remote,
					  bool delete_dir = false)
			: conn_(conn), path_(path), remote_(remote),
			  delete_dir_(delete_dir)
		{
		}

		virtual void operator()(agenda_ptr agenda);
		virtual void print_to(std::ostream &str)
		{
			str << "Download " << path_ << " from " << remote_;
		}

	private:
	};

	class local_file_deleter : public sync_task
	{
		const bf::path file_;
	public:
		local_file_deleter(const bf::path &file)
			: file_(file)
		{
		}
		virtual void operator()(agenda_ptr agenda);
		virtual void print_to(std::ostream &str)
		{
			str << "Delete " << file_;
		}

	private:
	};

}; //namespace es3

#endif //DOWNLOADER_H
