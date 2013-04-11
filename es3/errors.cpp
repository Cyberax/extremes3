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
#include "errors.h"
#include <sstream>
#include <string.h>

using namespace es3;

extern ES3LIB_PUBLIC const die_t es3::die=die_t();
extern ES3LIB_PUBLIC const libc_die_t es3::libc_die=libc_die_t();
extern ES3LIB_PUBLIC const result_code_t es3::sok=result_code_t();

es3_exception::es3_exception(const result_code_t &code) : code_(code)
{
	std::stringstream s;
	std::string lvl;
	if (code.code()==errFatal)
		lvl="ERROR";
	else if (code.code()==errWarn)
		lvl="WARN";
	else
		lvl="INFO";
	s<<lvl<<": '"<<code_.desc()<<"'";
	s.flush();
	what_=s.str();

	//Backtrace exception
	backtrace_it();
}

void es3::throw_libc_err(const std::string &desc)
{
	int cur_err = errno;
	if (cur_err==0)
		return; //No error - we might be here accidentally
//	char buf[1024]={0};
//	strerror_r(cur_err, buf, 1023);
	err(errFatal) << "Error code " << cur_err << ". " << desc;
}
