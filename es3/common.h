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
#ifndef COMMON_H
#define COMMON_H

#if defined _WIN32 || defined __CYGWIN__
  #ifdef LIBES3LIB_EXPORTS
	#ifdef __GNUC__
	  #define ES3LIB_PUBLIC __attribute__ ((dllexport))
	#else
	  #define ES3LIB_PUBLIC __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
  #else
	#ifdef __GNUC__
	  #define ES3LIB_PUBLIC __attribute__ ((dllimport))
	#else
	  #define ES3LIB_PUBLIC __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
  #endif
  #define ES3LIB_LOCAL
#else
  #if __GNUC__ >= 4
	#define ES3LIB_PUBLIC __attribute__ ((visibility ("default")))
	#define ES3LIB_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
	#define ES3LIB_PUBLIC
	#define ES3LIB_LOCAL
  #endif
#endif

#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <string>
#include <vector>
#include <map>
//#include <mutex>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <stdexcept>

#ifdef __MACH__
#define lseek64 lseek
#endif

namespace es3 {
	namespace bf = boost::filesystem;

	void ES3LIB_LOCAL backtrace_it(void);

//	typedef std::lock_guard<std::mutex> guard_t;
//	typedef std::unique_lock<std::mutex> u_guard_t;
	typedef boost::mutex mutex_t;
	typedef boost::lock_guard<boost::mutex> guard_t;
	typedef boost::unique_lock<boost::mutex> u_guard_t;
	
	typedef std::vector<std::string> stringvec;

	ES3LIB_PUBLIC std::string int_to_string(int64_t in);
	ES3LIB_PUBLIC void append_int_to_string(int64_t in, std::string &out);

	ES3LIB_PUBLIC std::string base64_encode(const char *, size_t len);
	ES3LIB_PUBLIC std::string base64_decode(const std::string &s);
	ES3LIB_PUBLIC std::string trim(const std::string &str);

	ES3LIB_PUBLIC std::string tobinhex(const unsigned char* data, size_t ln);
	ES3LIB_PUBLIC std::string format_time(time_t time);

	class logger
	{
		int verbosity_;
		boost::shared_ptr<std::ostream> stream_;
	public:
		ES3LIB_PUBLIC logger(int lvl);
		ES3LIB_PUBLIC ~logger();
		ES3LIB_PUBLIC static bool is_log_on(int lvl);
		ES3LIB_PUBLIC static void set_verbosity(int lvl);

		template<class T> std::ostream& operator << (const T& data)
		{
			return (*stream_) << data;
		}
	};

    mutex_t& get_logger_lock();
	#define VLOG(lev) if(es3::logger::is_log_on(lev)) es3::logger(lev)

	class handle_t
	{
		int fileno_;

		handle_t(const handle_t& other);
		handle_t& operator = (const handle_t &other);
	public:
		handle_t();
		explicit handle_t(int fileno);
		~handle_t();

		uint64_t size() const;
		int get() const {return fileno_;}
	};

	struct ci_string_less :
		public std::binary_function<std::string, std::string, bool>
	{
		ES3LIB_PUBLIC bool operator()(const std::string &lhs,
									  const std::string &rhs) const;
	};

	template<typename K, typename V, typename C, typename A, typename K2>
		V try_get(const std::map<K,V,C,A> &map, const K2 &key, const V &def=V())
	{
		const typename std::map<K,V,C,A>::const_iterator pos=map.find(key);
		if (pos==map.end()) return def;
		return pos->second;
	}

	template<class to, class from> inline to safe_cast(from f)
	{
		to t = static_cast<to>(f);
		if ( t != f )
			throw std::out_of_range("unsafe cast failed");
		return t;
	}

};

#endif // COMMON_H
