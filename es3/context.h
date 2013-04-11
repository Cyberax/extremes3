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
#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#define MAX_SEGMENTS 9999

typedef void CURL;

namespace es3 {
	struct s3_path;

	typedef boost::shared_ptr<CURL> curl_ptr_t;

	class conn_context : public boost::enable_shared_from_this<conn_context>
	{
	public:
		bf::path scratch_dir_;
		bool use_ssl_, do_compression_;
        std::string api_key_, secret_key;
        int concurrent_list_req_;

        conn_context() : use_ssl_(), do_compression_(true), concurrent_list_req_(-1) {};
		~conn_context();

		curl_ptr_t get_curl(const std::string &zone,
					   const std::string &bucket);
        void taint(curl_ptr_t ptr);

		void reset();
		char* err_buf_for(curl_ptr_t ptr)
		{
			return error_bufs_[ptr.get()];
		}

	private:
		conn_context(const conn_context &);

		void release_curl(CURL*);
		boost::mutex m_;
		std::map<std::string, std::vector<CURL*> > curls_;
		std::map<CURL*, char*> error_bufs_;
		std::map<CURL*, std::string> borrowed_curls_;
        std::map<CURL*, int> use_counts_;

		friend class curl_deleter;
	};
	typedef boost::shared_ptr<conn_context> context_ptr;
}; //namespace es3

#endif //CONTEXT_H
