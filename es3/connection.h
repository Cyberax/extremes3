#ifndef CONNECTION_H
#define CONNECTION_H

#include "common.h"
#include "context.h"
#include <functional>
#include <boost/weak_ptr.hpp>

typedef void CURL;
struct curl_slist;

namespace es3 {
	class conn_context;
	typedef boost::shared_ptr<conn_context> context_ptr;
	typedef std::map<std::string, std::string, ci_string_less> header_map_t;

	struct file_desc
	{
		time_t mtime_;
		uint64_t raw_size_, remote_size_;
		mode_t mode_;
		bool compressed_;
		bool found_;
	};

	struct s3_path
	{
		std::string zone_, bucket_, path_;
		
		inline bool operator<(const s3_path &right) const
		{
			return zone_<right.zone_ || bucket_<right.bucket_ ||
				path_<right.path_;
		}
	};
	inline s3_path derive(const s3_path &left, const std::string &right)
	{
		s3_path res(left);
		if (right.empty())
			return res;

		if (*res.path_.rbegin()!='/' && right.at(0)!='/')
			res.path_.append("/");
		res.path_.append(right);
		return res;
	}
	ES3LIB_PUBLIC s3_path parse_path(const std::string &url);
	inline std::ostream& operator << (std::ostream &out, const s3_path &p)
	{
		out << "s3://" << p.bucket_ << p.path_;
		return out;
	}

	struct s3_file;
	typedef boost::shared_ptr<s3_file> s3_file_ptr;
	typedef std::map<std::string, s3_file_ptr> file_map_t;

	struct s3_directory;
	typedef boost::shared_ptr<s3_directory> s3_directory_ptr;
	typedef boost::weak_ptr<s3_directory> s3_directory_weak_t;
	typedef std::map<std::string, s3_directory_ptr> subdir_map_t;

	struct s3_directory
	{
		std::string name_;
		s3_path absolute_name_;

		mutex_t m_;
		file_map_t files_;
		subdir_map_t subdirs_;
		s3_directory_weak_t parent_;
	};

	struct s3_file
	{
		std::string name_;
		s3_path absolute_name_;
		std::string mtime_str_;

		uint64_t size_;
		s3_directory_weak_t parent_;
	};

	typedef boost::function<void(size_t)> progress_callback_t;

	class s3_connection
	{
		const context_ptr conn_data_;
		struct curl_slist *header_list_;
	public:
		s3_connection(const context_ptr &conn_data);
		~s3_connection();

		std::string read_fully(const std::string &verb,
							   const s3_path &path,
							   const std::string &args="",
							   const header_map_t &opts=header_map_t());
        std::string upload_data(const s3_path &path, const std::string &upload_id, int part_num,
								const char *data, size_t size,
								const header_map_t& opts=header_map_t());
		void download_data(const s3_path &path,
			uint64_t offset, char *data, size_t size,
			const header_map_t& opts=header_map_t());

		s3_directory_ptr list_files_shallow(const s3_path &path,
			s3_directory_ptr target, bool try_to_root);

		std::string initiate_multipart(const s3_path &path,
									   const header_map_t &opts);
		std::string complete_multipart(const s3_path &path,
									   const std::string &upload_id,
									   const std::vector<std::string> &etags);
		file_desc find_mtime_and_size(const s3_path &path);

		std::string find_region(const std::string &bucket);
		
		void set_acl(const s3_path &path, const std::string &acl);
	private:
        bool check_part(const std::string &doc, int part_num);
		void checked(curl_ptr_t curl, int curl_code);
		void check_for_errors(curl_ptr_t curl,
							  const std::string &curl_res);
		void prepare(curl_ptr_t curl,
					 const std::string &verb,
					 const s3_path &path,
					 const header_map_t &opts=header_map_t());

		std::string sign(const std::string &str);
		struct curl_slist* authenticate_req(struct curl_slist *,
				const std::string &verb, const s3_path &path,
				const header_map_t &opts);

		void set_url(curl_ptr_t curl,
					 const s3_path &path, const std::string &args);
	};

}; //namespace es3

#endif //CONNECTION_H
