#include "common.h"
#include <sstream>
#include "errors.h"
#include <stdio.h>

#ifndef __MACH__
#define BOOST_KARMA_NUMERICS_LOOP_UNROLL 6
#include <boost/spirit/include/karma.hpp>
using namespace boost::spirit;
using boost::spirit::karma::int_;
using boost::spirit::karma::lit;
#endif

using namespace es3;

static int global_verbosity_level = 0;
mutex_t logger_lock_;

mutex_t& es3::get_logger_lock()
{
    return logger_lock_;
}

es3::logger::logger(int lvl)
	: verbosity_(lvl), stream_(new std::ostringstream())
{
}

es3::logger::~logger()
{
	guard_t g(logger_lock_);
	std::cerr
			<< static_cast<std::ostringstream*>(stream_.get())->str()
			<< std::endl;
}

void es3::logger::set_verbosity(int lvl)
{
	global_verbosity_level = lvl;
}

bool es3::logger::is_log_on(int lvl)
{
	return lvl<=global_verbosity_level;
}

std::string es3::int_to_string(int64_t in)
{
	char buffer[64];
#ifdef __MACH__
	sprintf(buffer, "%lld", in);
	return buffer;
#else	
	char *ptr = buffer;
	karma::generate(ptr, int_, in);
	*ptr = '\0';
	return std::string(buffer, ptr-buffer);
#endif
}

void es3::append_int_to_string(int64_t in, std::string &out)
{
	char buffer[64];
#ifdef __MACH__
	sprintf(buffer, "%lld", in);
	out.append(buffer);
#else
	char *ptr = buffer;
	karma::generate(ptr, int_, in);
	*ptr = '\0';
	out.append(buffer, ptr);
#endif
}

std::string es3::trim(const std::string &str)
{
	std::string res;
	bool hit_non_ws=false;
	int ws_span=0;
	for(std::string::const_iterator iter=str.begin();iter!=str.end();++iter)
	{
		const char c=*iter;

		if (c==' ' || c=='\n' || c=='\r' || c=='\t')
		{
			ws_span++;
		} else
		{
			if (ws_span!=0 && hit_non_ws)
					res.append(ws_span, ' ');
			ws_span=0;
			hit_non_ws=true;

			res.append(1, c);
		}
	}
	return res;
}

std::string es3::tobinhex(const unsigned char* data, size_t ln)
{
	static const char alphabet[17]="0123456789abcdef";
	std::string res;
	res.resize(ln*2);
	for(size_t f=0;f<ln;++f)
	{
		res[f*2]=alphabet[data[f]/16];
		res[f*2+1]=alphabet[data[f]%16];
	}
	return res;
}

std::string es3::format_time(time_t time)
{
	struct tm * timeinfo = gmtime(&time);
	char date_header[80] = {0};
	strftime(date_header, 80, "%a, %d %b %Y %H:%M:%S +0000", timeinfo);
	return date_header;
}

handle_t::handle_t()
{
	fileno_ = 0;
}

handle_t::handle_t(int fileno)
{
	fileno_ = fileno | libc_die;
}

uint64_t handle_t::size() const
{
	if (fileno_==0)
		return 0;
	int64_t pos=lseek64(fileno_, 0, SEEK_SET) | libc_die;
	int64_t res=lseek64(fileno_, 0, SEEK_END);
	lseek64(fileno_, -pos, SEEK_SET) | libc_die; //Restore file pos
	if (res<0) res | libc_die;
	return res;
}

handle_t::~handle_t()
{
	if (fileno_)
		close(fileno_);
}

bool es3::ci_string_less::operator()(const std::string &lhs,
									 const std::string &rhs) const
{
	return strcasecmp(lhs.c_str(), rhs.c_str()) < 0 ? 1 : 0;
}

#ifndef NDEBUG
	#include <execinfo.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <execinfo.h>
	#include <cxxabi.h>

	static std::string demangle(const char* symbol)
	{
		char temp[256];

		//first, try to demangle a c++ name
		if (1 == sscanf(symbol, "%*[^(]%*[^_]%127[^)+]", temp))
		{
			size_t size;
			int status=0;
			char* demangled=abi::__cxa_demangle(temp, NULL, &size, &status);
			if (demangled)
			{
				std::string result(demangled);
				free(demangled);
				return result;
			}
		}
		return symbol;
	}

	void es3::backtrace_it(void)
	{
		#define MAX_FRAMES 100
		void* addresses[MAX_FRAMES];
		const int size = backtrace(addresses, MAX_FRAMES);
		char** symbols = backtrace_symbols(addresses, size);
		for (int x = 0; x < size; ++x)
			fprintf(stderr, "%s\n", demangle(symbols[x]).c_str());
		free(symbols);
	}
#else
	void es3::backtrace_it(void) {}
#endif
