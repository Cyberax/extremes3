#include "commands.h"
#include "connection.h"
#include "sync.h"
#include <iostream>
#include <boost/program_options.hpp>

using namespace es3;
namespace po = boost::program_options;

int es3::do_rsync(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help)
{
	po::options_description opts("Sync options", term_width);
	stringvec included, excluded;
	opts.add_options()
		("delete-missing,D", "Delete missing files from the sync destination")
		("exclude-path,E", po::value<stringvec>(&excluded),
			"Exclude the paths matching the pattern from synchronization. "
			"If set, all matching files will be excluded even if they match "
			"one of the 'include-path' rules.")
		("include-path,I", po::value<stringvec>(&included),
			"Include the paths matching the pattern for synchronization. "
			"If set, only the matching paths will be included in "
			"synchronization.")
	;

	if (help)
	{
		std::cout << "Sync syntax: es3 sync [OPTIONS] <SOURCES> <DESTINATION>\n"
				  << "where <SOURCES> and <DESTINATION> are either:\n"
				  << "\t - Local directory\n"
				  << "\t - Amazon S3 storage (in s3://<bucket>/path/ format)"
				  << std::endl << std::endl;
		std::cout << opts;
		return 0;
	}

	po::positional_options_description pos;
	pos.add("<ARGS>", -1);

	stringvec args;
	opts.add_options()
			("<ARGS>", po::value<stringvec>(&args)->multitoken()->required())
	;

	po::variables_map vm;
	try
	{
		po::store(po::command_line_parser(params)
			.options(opts).positional(pos).run(), vm);
		po::notify(vm);
	} catch(const boost::program_options::error &err)
	{
		std::cerr << "ERR: Failed to parse configuration options. Error: "
				  << err.what() << "\n"
				  << "Use --help for help\n";
		return 2;
	}
	if (args.size()<2)
	{
		std::cerr << "ERR: At least one <SOURCE> "
				  << "and exactly one <DESTINATION> must be specified.\n";
		return 2;
	}

	bool delete_missing=vm.count("delete-missing");

	s3_connection conn(context);
	std::vector<s3_path> remotes;
	stringvec locals;
	bool do_upload;

	std::string tgt = args.back();
	args.pop_back();
	if (tgt.find("s3://")==0)
	{
		//Upload!
		s3_path path = parse_path(tgt);
		std::string region=conn.find_region(path.bucket_);
		if (!region.empty())
			path.zone_="s3-"+region;
		remotes.push_back(path);

		//Check that local sources exist
		for(auto iter=args.begin();iter!=args.end();++iter)
		{
			if (!bf::exists(*iter))
			{
				std::cerr << "ERR: Non-existing path " << *iter << std::endl;
				return 3;
			}
		}
		locals = args;
		do_upload=true;
	} else
	{
		locals.push_back(tgt);
		if (!bf::exists(tgt))
		{
			std::cerr << "ERR: Non-existing path " << tgt << std::endl;
			return 3;
		}
		for(auto iter=args.begin();iter!=args.end();++iter)
		{
			s3_path path = parse_path(*iter);
			std::string region=conn.find_region(path.bucket_);
			if (!region.empty())
				path.zone_="s3-"+region;
			remotes.push_back(path);
		}

		do_upload = false;
	}

	synchronizer sync(ag, context, remotes, locals, do_upload, delete_missing,
					  included, excluded);
	sync.create_schedule();
	return ag->run();
}