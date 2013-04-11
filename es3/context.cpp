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
#include "context.h"
#include <curl/curl.h>
#include "errors.h"

#define MAX_CURL_REUSE_COUNT 4
using namespace es3;

namespace es3
{
	struct curl_deleter
    {
		conn_context *parent_;

		void operator()(CURL *curl)
		{
			parent_->release_curl(curl);
		}
	};
};

void conn_context::reset()
{
	assert(borrowed_curls_.empty());
	for(auto iter=curls_.begin();iter!=curls_.end();++iter)
		for(auto citer=iter->second.begin();citer!=iter->second.end();++citer)
		{
			curl_easy_cleanup(*citer);
			assert(error_bufs_.count(*citer));
			free(error_bufs_.at(*citer));
		}
	curls_.clear();
	error_bufs_.clear();
}

conn_context::~conn_context()
{
	reset();
}

void conn_context::taint(curl_ptr_t ptr)
{
    guard_t lock(m_);
    use_counts_[ptr.get()]=MAX_CURL_REUSE_COUNT+1;
}

curl_ptr_t conn_context::get_curl(const std::string &zone,
							 const std::string &bucket)
{
	guard_t lock(m_);
    std::vector<CURL*> &cur = curls_[zone+"/"+bucket];
    if (!cur.empty())
	{
		CURL* res=cur.back();
		cur.pop_back();
        assert(!borrowed_curls_.count(res));
        use_counts_[res]++;

        if (use_counts_[res]>=MAX_CURL_REUSE_COUNT)
        {
            curl_easy_cleanup(res);
            assert(error_bufs_.count(res));
            free(error_bufs_.at(res));
            error_bufs_.erase(res);
            use_counts_.erase(res);
        } else
        {
            borrowed_curls_[res]=zone+"/"+bucket;
            return curl_ptr_t(res, curl_deleter{this});
        }
    }

    CURL* res=curl_easy_init();
    if (!res)
        err(errFatal) << "can't init CURL";

    char *err_buf=(char*)malloc(CURL_ERROR_SIZE+1);
    memset(err_buf, 0, CURL_ERROR_SIZE+1);

    error_bufs_[res]=err_buf;
    use_counts_[res]=1;
    curl_easy_setopt(res, CURLOPT_ERRORBUFFER, err_buf);

    assert(!borrowed_curls_.count(res));
    borrowed_curls_[res]=zone+"/"+bucket;
    return curl_ptr_t(res, curl_deleter{this});
}

void conn_context::release_curl(CURL* curl)
{
	guard_t lock(m_);

	assert(borrowed_curls_.count(curl));
	std::string key=borrowed_curls_.at(curl);
	borrowed_curls_.erase(curl);
	curls_[key].push_back(curl);
}
