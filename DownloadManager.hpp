#ifndef __DOWNLOADMANAGER_HPP__
#define __DOWNLOADMANAGER_HPP__

#include <openssl/sha.h>
#include <openssl/md5.h>
#include <curl/curl.h>
#include <zlib.h>
#include <mutex>

#include "types.h"

class DownloadManager : protected std::recursive_mutex
{
public:
	enum Strtype
	{
		FILENAME = 0, // set out filename for progress info
		OUTPATH, // set full path to file for download
		HEADER, // set a header
		URL, // set the url
		PROXY, // set a proxy
		PROXYUSERPWD // set the proxy user and password (if not added within proxy url)
	};

	enum Flagtype
	{
		BUFFER = 0,
		PROGRESS,
		IMMEDIATE, // start downloading when running GetDownloader
		PRINTHEADER,
		PRINTHASHES
	};
	typedef int (*progress_func)(void* p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
protected:
	char* filename;
	char* outpath;
	struct curl_slist* chunk;
	CURL* curl_handle;
	progress_func function;
	void* extraprogressdata;
	u64 downloadlimit;
	bool bufferflag;
	bool printheaders;
	bool printhashes;
	bool immediate;
	bool limiteddownload;
public:
	class Downloader
	{
	public:
		struct progress_data
		{
			const char* filename;
			curl_off_t last_print_value;
			void* extradata;
		};
	private:
		FILE* out;
		char* outpath;
		CURL* curl_handle;
		struct curl_slist* chunk;
		progress_func function;
		struct progress_data progress;
		struct
		{
			void* buffer;
			size_t size;
		} buffer_data;
		struct
		{
			void* buffer;
			size_t size;
		} header_data;
		CURLcode res;
		u64 status_code;
		u64 downloaded;
		u64 downloadlimit;
		bool bufferflag;
		bool printheaders;
		bool printhashes;
		bool limiteddownload;
		static int xferinfo(void* p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
		static size_t write_data(void* ptr, size_t size, size_t nmemb, void* data);
		static size_t headerprint(void* ptr, size_t size, size_t nmemb, void* data);
		Downloader& operator=(const Downloader& other) { return *this; }
	public:
		Downloader& operator=(Downloader&& other);
	private:
		explicit Downloader();
		explicit Downloader(DownloadManager& data, u64 expected_size = 0, bool write_opt_files = false);
		Downloader(const Downloader& other);
	public:
		Downloader(Downloader&& other) : Downloader() { *this = other; }
		~Downloader();

		bool Download(u64 expected_size = 0, bool write_opt_files = false);

		void* GetBufferAndDetach(size_t& length)
		{
			void* ptr = buffer_data.buffer;
			buffer_data.buffer = NULL;
			length = buffer_data.size;
			buffer_data.size = 0;
			return ptr;
		}

		bool WasLastAttemptSuccess()
		{
			return res == CURLE_OK;
		}

		CURLcode GetCurlResult()
		{
			return res;
		}

		u64 GetStatusCode()
		{
			return status_code;
		}

		friend class DownloadManager;
	};

	DownloadManager& SetAttribute(Strtype type, const char* str, ...);
	DownloadManager& SetAttribute(Flagtype type, bool flag);
	DownloadManager& SetAttribute(progress_func function);
	DownloadManager& SetAttribute(void* extra);
	DownloadManager& SetDownloadLimit(u64 limit);
	DownloadManager& RemoveDownloadLimit();
	DownloadManager& UseGlobalProxy();
	Downloader GetDownloader(u64 expected_size = 0)
	{
		return Downloader(*this, expected_size);
	}
	DownloadManager();
	~DownloadManager();
protected:
	static CURLcode InitLib();
	struct curl_slist* SlistClone();
	friend class Downloader;
public:
	static void SetGlobalProxy(const char* proxy);
};

#endif