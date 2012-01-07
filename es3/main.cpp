#include <iostream>
#include <boost/program_options.hpp>
#include "common.h"
#include "scope_guard.h"
#include "connection.h"
#include "context.h"
#include "agenda.h"
#include "sync.h"
#include <sys/ioctl.h>
#include <boost/bind.hpp>

using namespace es3;
#include <curl/curl.h>

namespace po = boost::program_options;
typedef std::vector<std::string> stringvec;


std::vector<po::option> subcommands_parser(stringvec& args,
										   const stringvec& subcommands)
{
	std::vector<po::option> result;
	if (args.empty())
		return result;

	stringvec::const_iterator i(args.begin());
	stringvec::const_iterator cmd_idx=std::find(subcommands.begin(),
												subcommands.end(),*i);
	if (cmd_idx!=subcommands.end())
	{
		po::option opt;
		opt.string_key = "subcommand";
		opt.value.push_back(*i);
		opt.original_tokens.push_back(*i);
		result.push_back(opt);

		for (++i; i != args.end(); ++i)
		{
			po::option opt;
			opt.string_key = "subcommand_params";
			opt.value.push_back(*i);
			opt.original_tokens.push_back(*i);
			result.push_back(opt);
		}
		args.clear();
	}
	return result;
}

int main(int argc, char **argv)
{
	int verbosity = 0, thread_num = 0;
	bool quiet=false;

	struct winsize w={0};
	ioctl(0, TIOCGWINSZ, &w);
	int term_width=(w.ws_col==0)? 80 : w.ws_col;

	context_ptr cd(new conn_context());
	cd->use_ssl_ = false;

	po::options_description generic("Generic options", term_width);
	generic.add_options()
		("help", "Display this message")
		("config,c", po::value<std::string>(),
			"Path to a file that contains configuration settings")
		("verbosity,v", po::value<int>(&verbosity)->default_value(1),
			"Verbosity level [0 - the lowest, 9 - the highest]")
		("quiet,q", "Quiet mode (no progress indicator)")
		("scratch-dir,r", po::value<std::string>(
			 &cd->scratch_path_)->default_value("/tmp")->required(),
			"Path to the scratch directory")
	;

	po::options_description access("Access settings", term_width);
	access.add_options()
		("access-key,a", po::value<std::string>(
				 &cd->api_key_)->required(),
			"Amazon S3 API key")
		("secret-key,s", po::value<std::string>(
			 &cd->secret_key)->required(),
			"Amazon S3 secret key")
		("use-ssl,l", po::value<bool>(
			 &cd->use_ssl_)->default_value(false),
			"Use SSL for communications with the Amazon S3 servers")
	;
	generic.add(access);

	po::options_description tuning("Tuning", term_width);
	tuning.add_options()
		("thread-num,n", po::value<int>(&thread_num)->default_value(0),
			"Number of threads used [0 - autodetect]")
		("segment-size", po::value<int>(&cd->segment_size_)->default_value(0),
			"Segment size in bytes [0 - autodetect, 6291456 - minimum]")
		("segments-in-flight", po::value<int>(&cd->max_in_flight_)->default_value(0),
			"Number of segments in-flight [0 - autodetect]")
		("reader-threads", po::value<int>(&cd->max_readers_)->default_value(0),
			"Number of filesystem reader/writer threads [0 - autodetect]")
		("compressor-threads", po::value<int>(&cd->max_compressors_)->default_value(0),
			"Number of compressor threads [0 - autodetect]")
	;
	generic.add(tuning);

//	("do-compression", po::value<bool>(
//		 &cd->do_compression_)->default_value(true)->required(),
//		"Compress files during upload")
//	("do-upload,u", po::value<bool>(&cd->upload_)->default_value(true),
//		"Upload local changes to the server")
//	("delete-missing,d", po::value<bool>(
//		 &cd->delete_missing_)->default_value(false),
//		"Delete missing files from the remote side")
//	("sync-dir,i", po::value<bf::path>(
//		 &cd->local_root_)->required(),
//		"Local directory")
//	("bucket-name,o", po::value<std::string>(
//		 &cd->bucket_)->required(),
//		"Name of Amazon S3 bucket")
//	("remote-path,p", po::value<std::string>(
//		 &cd->remote_root_)->default_value("/")->required(),
//		"Path in the Amazon S3 bucket")
//	("zone-name,z", po::value<std::string>(
//		 &cd->zone_)->default_value("s3")->required(),
//		"Name of Amazon S3 zone")
	po::options_description sub_data("Subcommands");
	sub_data.add_options()
		("subcommand", po::value<std::string>())
		("subcommand_params", po::value<stringvec>()->multitoken());

	stringvec subcommands;
	subcommands.push_back("sync");
	subcommands.push_back("ls");
	subcommands.push_back("cp");
	subcommands.push_back("rm");
	subcommands.push_back("mb");
	subcommands.push_back("rb");

	std::string cur_subcommand;

	po::variables_map vm;
	try
	{
		sub_data.add(generic);
		po::parsed_options parsed = po::command_line_parser(argc, argv)
				.options(sub_data)
				.extra_style_parser(boost::bind(&subcommands_parser, _1,
												subcommands))
			.run();
		po::store(parsed, vm);

		cur_subcommand=vm.count("subcommand")==0?
					"" : vm["subcommand"].as<std::string>();

		if (argc < 2 || vm.count("help"))
		{
			if (cur_subcommand.empty())
			{
				std::cout << "Extreme S3 - fast S3 client\n" << generic
						  << "\nThe following commands are supported:\n\t";
				for(auto iter=subcommands.begin();iter!=subcommands.end();++iter)
					std::cout<< *iter <<" ";
				std::cout << "\nUse --help <command_name> to get more info\n";
			} else
			{
				std::cout << "Extreme S3 - fast S3 client\n" << generic;
			}
			return 1;
		}

		if (cur_subcommand.empty())
		{
			std::cout << "No command specified. Use --help for help\n";
			return 2;
		}

		if (vm.count("config"))
		{
			// Parse the file and store the options
			std::string config_file = vm["config"].as<std::string>();
			po::store(po::parse_config_file<char>(config_file.c_str(),generic), vm);
		}
		quiet = vm.count("quiet");
	} catch(const boost::program_options::error &err)
	{
		std::cerr << "Failed to parse configuration options. Error: "
				  << err.what() << std::endl;
		return 2;
	}

	try
	{
		po::notify(vm);
	} catch(const boost::program_options::required_option &option)
	{
		std::cerr << "Required option " << option.get_option_name()
				  << " is not present." << std::endl;
		return 1;
	}
	cd->validate();

	logger::set_verbosity(verbosity);

	curl_global_init(CURL_GLOBAL_ALL);
	ON_BLOCK_EXIT(&curl_global_cleanup);

	try
	{
		if (cd->zone_=="s3")
		{
			s3_connection conn(cd);
			std::string region=conn.find_region();
			if (!region.empty())
				cd->zone_="s3-"+region;
		}

		if (thread_num==0)
			thread_num=sysconf(_SC_NPROCESSORS_ONLN)+1;

		//This is done to avoid deadlocks if readers grab all the in-flight
		//segments. We have to have at least one thread to be able to drain
		//the upload queue.
		if(thread_num<=(cd->max_compressors_+cd->max_readers_))
			thread_num = cd->max_compressors_+cd->max_readers_+2;

		agenda_ptr ag=agenda::make_new(thread_num);
		synchronizer sync(ag, cd);
		sync.create_schedule();
		size_t failed_num=ag->run(!quiet);

		return failed_num==0 ? 0 : 4;
	} catch(const std::exception &ex)
	{
		VLOG(0) << "An error has been encountered during processing. "
				<< ex.what();
		return 5;
	}
}
