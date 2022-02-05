#undef LIBCURL_VERSION_NUM //imma try to ensure there's no cheating builds with at commandline definitions.
#define _DEFAULT_SOURCE
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <new>
#include <mutex>
#include "DownloadManager.hpp"

#define HASH_BUFSIZE 4096
#define UNUSED(i) (void)i
#define TO_READ(rem,bsiz) rem < bsiz ? rem : bsiz

#if LIBCURL_VERSION_NUM < 0x073700
#error "Supporting at minimum version libcurl 7.55.0"
#endif

static std::recursive_mutex global_proxy_lock;
static char* global_proxy = NULL;

void snbytes2hex(char *out, u8 *bytes, int len)
{
	char tmp[3];

	for (int i = 0; i < len; i++)
	{
		snprintf(tmp, 3, "%02X", bytes[i]);
		memcpy(out + i * 2, tmp, 2);
	}
}

int DownloadManager::Downloader::xferinfo(void* p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	UNUSED(ultotal); UNUSED(ulnow); //build warning suppression

	struct progress_data* progress = (struct progress_data*)p;

	if ((dlnow - progress->last_print_value >= 20480 || dlnow == dltotal) && progress->last_print_value != dlnow)
	{
		if (dltotal)
			printf("\rDownloading: %s... %5.01f%% %llu / %llu", progress->filename, (double)dlnow / (double)dltotal * 100, (unsigned long long)dlnow, (unsigned long long)dltotal);

		if (dltotal && dltotal == dlnow)
			puts("\n");

		progress->last_print_value = dlnow;

		fflush(stdout);
	}
	return 0;
}

size_t DownloadManager::Downloader::write_data(void* ptr, size_t size, size_t nmemb, void* data)
{
	Downloader* _this = (Downloader*)data;
	size_t realsize = size * nmemb;
	u64 totalsize = _this->downloaded + realsize;

	if (_this->limiteddownload && totalsize > _this->downloadlimit) return 0; // set limited reached

	_this->downloaded = totalsize;

	if (_this->bufferflag)
	{
		if (_this->buffer_data.size != totalsize) //incase this function is called when realsize = 0
		{
			u8* buffer = (u8*)realloc(_this->buffer_data.buffer, totalsize);

			if (!buffer)
			{
				free(_this->buffer_data.buffer);

				_this->buffer_data.buffer = NULL;

				return 0; //abort
			}

			_this->buffer_data.buffer = buffer;

			memcpy((void*)(&((char*)_this->buffer_data.buffer)[_this->buffer_data.size]), ptr, realsize);

			_this->buffer_data.size = totalsize;

			if (!_this->out) return realsize; //if no file, return now, to continue with download to buffer
		}
	}

	if (_this->out)
	{
		return fwrite(ptr, size, nmemb, _this->out);
	}

	return 0; //this shouldn't happen?? but to suppress warnings and to be sure..
}

size_t DownloadManager::Downloader::headerprint(void* ptr, size_t size, size_t nmemb, void* data)
{
	Downloader* _this = (Downloader*)data;
	size_t realsize = size * nmemb;
	size_t totalsize = _this->header_data.size + realsize;

	if (_this->header_data.size == totalsize) //incase this function is called when realsize = 0
		return 0;

	u8* buffer = (u8*)realloc(_this->header_data.buffer, totalsize);

	if (!buffer)
	{
		free(_this->header_data.buffer);

		_this->header_data.buffer = NULL;

		return 0; //abort
	}

	_this->header_data.buffer = buffer;

	memcpy((void*)(&((char*)_this->header_data.buffer)[_this->header_data.size]), ptr, realsize);

	_this->header_data.size = totalsize;

	return realsize;
}

DownloadManager::Downloader::Downloader() :
	out(NULL),
	outpath(NULL),
	curl_handle(NULL),
	chunk(NULL),
	function(NULL),
	progress({}),
	buffer_data({}),
	header_data({}),
	res((CURLcode)~CURLE_OK),
	downloaded(0LLU),
	downloadlimit(0LLU),
	bufferflag(false),
	printheaders(false),
	printhashes(false),
	limiteddownload(false)
{}

DownloadManager::Downloader::Downloader(DownloadManager& data, u64 expected_size, bool write_opt_files) : Downloader()
{
	data.lock();

	outpath = data.outpath;

	FILE* out = !outpath ? NULL : fopen(outpath, "wb");

	if (outpath && !out)
	{
		data.unlock();

		throw std::bad_alloc();
	}

	if (!out && !data.bufferflag && !data.printheaders)
	{
		data.unlock();

		throw std::runtime_error("No download location or buffer set.");
	}

	if (out)
	{
		fclose(out);

		out = NULL;

		remove(outpath); //remove empty file
	}

	if (data.chunk) chunk = data.SlistClone();

	curl_handle = curl_easy_duphandle(data.curl_handle);

	if ((data.chunk && !chunk) || !curl_handle)
	{
		curl_easy_cleanup(curl_handle);
		curl_slist_free_all(chunk);

		data.unlock();

		throw std::bad_alloc();
	}

	function = !data.function ? &xferinfo : data.function;
	bufferflag = data.bufferflag;
	printheaders = data.printheaders;
	printhashes = data.printhashes;
	progress.filename = data.filename;
	progress.extradata = data.extraprogressdata;
	bool immediate = data.immediate;
	downloadlimit = data.downloadlimit;
	limiteddownload = data.limiteddownload;

	data.outpath = data.filename = NULL;
	data.extraprogressdata = NULL;
	data.bufferflag = data.immediate = false;

	data.unlock();

	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, this);

	if (printheaders)
	{
		curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, headerprint);
		curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, this);
	}

	curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, function);
	curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, &this->progress);

	if (immediate) Download(expected_size, write_opt_files);
}

DownloadManager::Downloader::~Downloader()
{
	free((void*)progress.filename);
	free(outpath);
	free(buffer_data.buffer);

	curl_easy_cleanup(curl_handle);
	curl_slist_free_all(chunk);
}

bool DownloadManager::Downloader::Download(u64 expected_size, bool write_opt_files)
{
	downloaded = 0LLU;

	out = !outpath ? NULL : fopen(outpath, "wb");

	if (outpath && !out)
	{
		return false;
	}

	if (!out && !bufferflag)
	{
		if (!printheaders) return false;

		curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1L);
	}

	res = curl_easy_perform(curl_handle);

	if (out) fclose(out);

	if (res != CURLE_OK)
	{
		if (res == CURLE_HTTP_RETURNED_ERROR)
		{
			curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &status_code);
		}

		if (outpath) remove(outpath);

		free(buffer_data.buffer);
		free(header_data.buffer);

		buffer_data.buffer = NULL;
		buffer_data.size = 0;

		header_data.buffer = NULL;
		header_data.size = 0;

		return false;
	}
	
	curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &status_code);

	if (printheaders)
	{
		if (!header_data.buffer)
		{
			if (outpath) remove(outpath);

			free(buffer_data.buffer);

			buffer_data.buffer = NULL;
			buffer_data.size = 0;

			return false;
		}

		printf("HTTP Response Headers:\n\n");
		fwrite(header_data.buffer, header_data.size, 1, stdout);
		fflush(stdout);

		if (write_opt_files)
		{
			// {path}_headers.txt
			// path length + 1 (underscore) + 7 ("headers") + 1 (period) + 3 ("txt") + 1 (NULL terminator)
			int hdrfile_pathlen = strlen(outpath) + 1 + 7 + 1 + 3 + 1;

			char *headerfile_path = (char *)malloc(hdrfile_pathlen);

			if (!headerfile_path)
			{
				free(header_data.buffer);

				header_data.buffer = NULL;
				header_data.size = 0;

				throw std::bad_alloc();
			}

			snprintf(headerfile_path, hdrfile_pathlen, "%s_headers.txt", outpath);

			FILE* hdrfile = fopen(headerfile_path, "w");
			free(headerfile_path);

			if (!hdrfile || fwrite(header_data.buffer, 1, header_data.size, hdrfile) != header_data.size)
			{
				free(header_data.buffer);

				header_data.buffer = NULL;
				header_data.size = 0;

				return false;
			}

			fclose(hdrfile);
		}

		free(header_data.buffer);

		header_data.buffer = NULL;
		header_data.size = 0;
	}

	curl_off_t dl_bytes = 0;
	curl_easy_getinfo(curl_handle, CURLINFO_SIZE_DOWNLOAD_T, &dl_bytes);

	if (!dl_bytes)  // 0 size??
	{
		if (outpath) remove(outpath);

		free(buffer_data.buffer);

		buffer_data.buffer = NULL;
		buffer_data.size = 0;

		return false;
	}

	if (expected_size && (u64)dl_bytes != expected_size)
	{
		return false;
	}

	if (outpath && printhashes)
	{
		FILE* dlfile = fopen(outpath, "r");

		if (!dlfile)
		{
			printf("Could not open file after downloading.\n");
			return false;
		}

		printf("Hashing... 0%% 0 / %ld", dl_bytes);

		char buf[HASH_BUFSIZE];

		u64 read = 0;
		u64 remaining = dl_bytes;
		u64 to_read = TO_READ(remaining, HASH_BUFSIZE);

		SHA256_CTX sha256ctx;
		SHA_CTX sha1ctx;
		MD5_CTX md5ctx;
		u32 crc = crc32(0, Z_NULL, 0);

		if (!SHA256_Init(&sha256ctx) || !SHA1_Init(&sha1ctx) || !MD5_Init(&md5ctx))
		{
			fprintf(stderr, "SHA256/SHA1/MD5 Context init fail!\n");
			fclose(dlfile);

			return false;
		}

		while (read != (u64)dl_bytes)
		{
			if (fread(buf, 1, to_read, dlfile) != to_read)
			{
				fprintf(stderr, "Could not read from file\n");
				fclose(dlfile);
				return false;
			}

			crc = crc32(crc, (const Bytef*)buf, to_read);

			if (
				!SHA256_Update(&sha256ctx, buf, to_read) ||
				!SHA1_Update(&sha1ctx, buf, to_read) ||
				!MD5_Update(&md5ctx, buf, to_read)
				)
			{
				fprintf(stderr, "Couldn't hash\n");
				fclose(dlfile);
				return false;
			}

			read += to_read;
			remaining -= to_read;
			to_read = TO_READ(remaining, HASH_BUFSIZE);

			printf("\rHashing... %5.01f%% %ld / %ld", (double)read / (double)dl_bytes * 100, read, dl_bytes);
			fflush(stdout);
		}

		fclose(dlfile);

		puts("\n");

		u8 sha256[0x20];
		u8 sha1[0x14];
		u8 md5[0x10];

		if (!SHA256_Final(sha256, &sha256ctx) || !SHA1_Final(sha1, &sha1ctx) || !MD5_Final(md5, &md5ctx))
		{
			fprintf(stderr, "Hash finalize fail\n");
			return false;
		}

		char sha256_str[0x41], sha1_str[0x29], md5_str[0x21], hash_str[0x100];

		memset(sha256_str, 0, 0x41);
		memset(sha1_str, 0, 0x29);
		memset(md5_str, 0, 0x21);
		memset(hash_str, 0, 0x100);

		snbytes2hex(sha256_str, sha256, 0x20);
		snbytes2hex(sha1_str, sha1, 0x14);
		snbytes2hex(md5_str, md5, 0x10);

		snprintf(
			hash_str, 
			0xE0,
			"SHA-256 : %s\n"
			"SHA-1   : %s\n"
			"MD5     : %s\n"
			"CRC32   : %08X\n"
			"Size    : %ld\n",
			sha256_str, sha1_str, md5_str, crc, dl_bytes);

		printf("Hashes: \n\n%s\n", hash_str);

		if (write_opt_files)
		{
			// {path}_hashes.txt
			// path length + 1 (underscore) + 6 ("hashes") + 1 (period) + 3 ("txt") + 1 (NULL terminator)
			int hashfile_pathlen = strlen(outpath) + 1 + 6 + 1 + 3 + 1;

			char *hashfile_path = (char *)malloc(hashfile_pathlen);

			if (!hashfile_path)
				throw std::bad_alloc();

			snprintf(hashfile_path, hashfile_pathlen, "%s_hashes.txt", outpath);

			FILE* hashfile = fopen(hashfile_path, "w");
			free(hashfile_path);

			if (!hashfile)
			{
				fprintf(stderr, "Failed opening hashes file\n");
				return false;
			}

			fwrite(hash_str, 1, strlen(hash_str), hashfile);
			fclose(hashfile);
		}
	}

	return true;
}

DownloadManager::Downloader& DownloadManager::Downloader::operator=(Downloader&& other)
{
	if (this != &other)
	{
		this->~Downloader();

		out = other.out;
		other.out = NULL;

		outpath = other.outpath;
		other.outpath = NULL;

		curl_handle = other.curl_handle;
		other.curl_handle = NULL;

		chunk = other.chunk;
		other.chunk = NULL;

		function = other.function;
		other.function = NULL;

		progress.filename = other.progress.filename;
		progress.last_print_value = other.progress.last_print_value;
		progress.extradata = other.progress.extradata;
		other.progress = { NULL, 0, NULL };

		buffer_data.buffer = other.buffer_data.buffer;
		buffer_data.size = other.buffer_data.size;
		other.buffer_data = { NULL, 0 };

		header_data.buffer = other.header_data.buffer;
		header_data.size = other.header_data.size;
		other.header_data = { NULL, 0 };

		res = other.res;
		other.res = (CURLcode)~CURLE_OK;

		downloaded = other.downloaded;
		downloadlimit = other.downloadlimit;
		other.downloaded = other.downloadlimit = 0LLU;

		bufferflag = other.bufferflag;
		printheaders = other.printheaders;
		printhashes = other.printhashes;
		limiteddownload = other.limiteddownload;
		other.bufferflag = other.printheaders = other.printhashes = other.limiteddownload = false;

		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, this);

		if (printheaders)
		{
			curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, headerprint);
			curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, this);
		}

		curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, function);
		curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, &this->progress);
	}

	return *this;
}

DownloadManager& DownloadManager::SetAttribute(DownloadManager::Strtype type, const char* str, ...)
{
	lock();

	if (!str) str = "";

	va_list vaorig, vacopy;
	va_start(vaorig, str);

	do
	{
		va_copy(vacopy, vaorig);

		int len = vsnprintf(NULL, 0, str, vacopy) + 1;

		va_end(vacopy);

		if (len <= 1) break;

		char* newdata = (char*)calloc(len, 1);

		if (!newdata) break;

		va_copy(vacopy, vaorig);
		vsnprintf(newdata, len, str, vacopy);
		va_end(vacopy);

		switch (type)
		{
		case FILENAME:
			free(filename);
			filename = newdata;
			break;
		case OUTPATH:
			free(outpath);
			outpath = newdata;
			break;
		case HEADER:
		{
			//cases don't create scopes, and I create a var in here, so... scope it is
			curl_slist* newchunk = curl_slist_append(chunk, newdata);
			if (newchunk) chunk = newchunk;
			free(newdata);
		}
		break;
		case URL:
			curl_easy_setopt(curl_handle, CURLOPT_URL, newdata);
			free(newdata);
			break;
		case PROXY:
			curl_easy_setopt(curl_handle, CURLOPT_PROXY, newdata);
			free(newdata);
			break;
		case PROXYUSERPWD:
			curl_easy_setopt(curl_handle, CURLOPT_PROXYUSERPWD, newdata);
			free(newdata);
			break;
		default: //what?
			free(newdata);
			break;
		}
	} while (0);

	va_end(vaorig);

	unlock();

	return *this;
}

DownloadManager& DownloadManager::SetAttribute(DownloadManager::Flagtype type, bool flag) {
	lock();

	switch (type)
	{
	case BUFFER:
		bufferflag = flag;
		break;
	case PROGRESS:
		curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, flag ? 0L : 1L);
		break;
	case IMMEDIATE:
		immediate = flag;
		break;
	case PRINTHEADER:
		printheaders = flag;
		break;
	case PRINTHASHES:
		printhashes = flag;
		break;
	}

	unlock();

	return *this;
}

DownloadManager& DownloadManager::SetAttribute(DownloadManager::progress_func function)
{
	lock();

	this->function = function;

	unlock();

	return *this;
}

DownloadManager& DownloadManager::SetAttribute(void* extra)
{
	lock();

	this->extraprogressdata = extra;

	unlock();

	return *this;
}

DownloadManager& DownloadManager::SetDownloadLimit(u64 limit)
{
	lock();

	this->downloadlimit = limit;
	this->limiteddownload = true;

	unlock();

	return *this;
}

DownloadManager& DownloadManager::RemoveDownloadLimit()
{
	lock();

	this->limiteddownload = false;

	unlock();

	return *this;
}

DownloadManager& DownloadManager::UseGlobalProxy()
{
	global_proxy_lock.lock();

	if (!global_proxy)
	{
		global_proxy_lock.unlock();

		return *this;
	}

	this->SetAttribute(DownloadManager::PROXY, global_proxy);

	global_proxy_lock.unlock();

	return *this;
}

DownloadManager::DownloadManager() : filename(NULL), outpath(NULL), chunk(NULL), function(NULL), extraprogressdata(NULL), bufferflag(false), printheaders(false), printhashes(false), immediate(false), limiteddownload(false)
{
	if (InitLib() != CURLE_OK) throw std::runtime_error("Couldn't init libcurl or an instance of it.");

#ifndef CURL_STATICLIB
	static bool versionchecked = false;

	if (!versionchecked)
	{
		curl_version_info_data* data = curl_version_info(CURLVERSION_NOW);

		if (data->version_num < 0x073700)
		{
			std::string message("Supporting at minimum version libcurl 7.55.0\nDynamic library version is ");

			message += data->version;

			throw std::runtime_error(message);
		}

		versionchecked = true;
	}
#endif

	if ((curl_handle = curl_easy_init()) == NULL)
		throw std::runtime_error("Couldn't init libcurl or an instance of it.");

	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_URL, "https://example.com");
}

DownloadManager::~DownloadManager()
{
	free(filename);
	free(outpath);
	curl_easy_cleanup(curl_handle);
	curl_slist_free_all(chunk);
}

CURLcode DownloadManager::InitLib()
{
	static std::recursive_mutex lock;

	lock.lock();

	static int init = 0;

	CURLcode ret = CURLE_OK;

	if (!init)
	{
		CURLcode res = curl_global_init(CURL_GLOBAL_ALL);

		if (!res) init = 1;

		ret = res;
	}

	lock.unlock();

	return ret;
}

//slightly changed copy of Curl_slist_duplicate from libcurl code, since libcurl doesn't provide me one on normal lib usage, as of this writing
//correct me if wrong
struct curl_slist* DownloadManager::SlistClone()
{
	struct curl_slist* outlist = NULL;
	struct curl_slist* tmp;
	struct curl_slist* inlist = chunk;

	while (inlist)
	{
		tmp = curl_slist_append(outlist, inlist->data);

		if (!tmp)
		{
			curl_slist_free_all(outlist);

			return NULL;
		}

		outlist = tmp;
		inlist = inlist->next;
	}

	return outlist;
}

void DownloadManager::SetGlobalProxy(const char* proxy)
{
	char* tmp = NULL;

	if (proxy)
	{
		tmp = (char*)malloc(strlen(proxy) + 1);

		if (!tmp) throw std::bad_alloc();

		strcpy(tmp, proxy);
	}

	global_proxy_lock.lock();

	free(global_proxy);

	global_proxy = tmp;

	global_proxy_lock.unlock();
}
