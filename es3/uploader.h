#ifndef UPLOADER_H
#define UPLOADER_H

#include "connection.h"
#include "common.h"
#include "agenda.h"

namespace es3 {
	struct upload_content;
	typedef boost::shared_ptr<upload_content> upload_content_ptr;
	struct compressed_result;
	typedef boost::shared_ptr<compressed_result> zip_result_ptr;

	class file_uploader : public sync_task,
			public boost::enable_shared_from_this<file_uploader>
	{
		const connection_data conn_;
		const std::string path_;
		const std::string remote_;
		const std::string etag_;
		agenda_ptr compr_agenda_;
	public:
		file_uploader(const connection_data &conn,
					  const std::string &path,
					  const std::string &remote,
					  const std::string &etag,
					  agenda_ptr compr_agenda)
			: conn_(conn), path_(path), remote_(remote), etag_(etag),
			  compr_agenda_(compr_agenda)
		{
		}

		virtual void operator()(agenda_ptr agenda);
	private:
		void start_upload(agenda_ptr ag,
						  upload_content_ptr content, handle_t file,
						  bool compressed);
		void simple_upload(agenda_ptr ag, upload_content_ptr content);
	};

	class file_deleter : public sync_task
	{
		const connection_data conn_;
		const std::string remote_;
	public:
		file_deleter(const connection_data &conn,
					  const std::string &remote)
			: conn_(conn), remote_(remote)
		{
		}

		virtual void operator()(agenda_ptr agenda);
	private:
	};

}; //namespace es3

#endif //UPLOADER_H
