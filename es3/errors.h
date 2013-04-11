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
#ifndef ERRORS_H
#define ERRORS_H

#include "common.h"
#include <sstream>

namespace es3 {
	class die_t {};
	extern ES3LIB_PUBLIC const die_t die;

	struct libc_die_t
	{
		std::string reason_;
	};
	extern ES3LIB_PUBLIC const libc_die_t libc_die;
	inline libc_die_t libc_die2(const std::string &reason)
	{
		libc_die_t res;
		res.reason_=reason;
		return res;
	}

	enum code_e
	{
		errFatal,
		errWarn,
		errNone,
	};

	class result_code_t
	{
	public:
		result_code_t() : code_(errNone), desc_()
		{
		}
		result_code_t(const result_code_t &other) :
			code_(other.code_), desc_(other.desc_)
		{
		}
		result_code_t(code_e code, const std::string &desc="None") :
			code_(code), desc_(desc)
		{

		}

		virtual ~result_code_t(){}

		code_e code() const { return code_; }
		std::string desc() const { return desc_; }
		bool ok() const {return code_==errNone;}
	private:
		code_e code_;
		std::string desc_;
	};
	extern ES3LIB_PUBLIC const result_code_t sok;

	class es3_exception : public virtual boost::exception,
			public std::exception
	{
		const result_code_t code_;
		std::string what_;
	public:
		ES3LIB_PUBLIC es3_exception(const result_code_t &code);
		virtual ~es3_exception() throw() {}

		const result_code_t & err() const
		{
			return code_;
		}

		virtual const char* what() const throw()
		{
			return what_.c_str();
		}
	};

	/**
	  Usage - err(sOk) << "This is a description"
	  */
	struct err : public std::stringstream
	{
		err(code_e code) : code_(code) {}

		~err()
		{
			//Yes, we're throwing from a destructor, but that's OK since
			//if there's an exception already started then we have other
			//big problems.
			if (code_!=errNone)
			{
				boost::throw_exception(es3_exception(
										   result_code_t(code_, str())));
			}
		}

	private:
		code_e code_;
	};

	inline void operator | (const result_code_t &code, const die_t &)
	{
		if (!code.ok())
			boost::throw_exception(es3_exception(code));
	}

	ES3LIB_PUBLIC void throw_libc_err(const std::string &desc);
	template<class T> T operator | (const T res, const libc_die_t &l)
	{
		if (res>=0)
			return res;
		throw_libc_err(l.reason_);
		return res;
	}

#define TRYIT(expr) try{ expr; } catch(const es3_exception &ex) { \
		return ex.err(); \
	}

}; //namespace es3

#endif //ERRORS_H
