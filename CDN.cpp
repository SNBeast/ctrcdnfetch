#include "CDN.hpp"
#include "TMD.hpp"

static const char* baseurl = "http://ccs.cdn.c.shop.nintendowifi.net/ccs/download";

void NintendoData::CDN::Download(const char* outdir, bool write_opt_files)
{
	char* b64_encticket = NULL;
	char* b64_encticketkey = NULL;
	u8* tmdbuffer = NULL;
	size_t tmdlen = 0;

	tik.GetWrappedTicket(b64_encticket, b64_encticketkey);

	manager
		.SetAttribute(DownloadManager::HEADER, "X-Authentication-Key: %s", b64_encticketkey)
		.SetAttribute(DownloadManager::HEADER, "X-Authentication-Data: %s", b64_encticket);

	try
	{
		manager
			.SetAttribute(DownloadManager::BUFFER, true)
			.SetAttribute(DownloadManager::PROGRESS, true);

		if (set_version)
		{
			manager
				.SetAttribute(DownloadManager::FILENAME, "tmd.%d", version)
				.SetAttribute(DownloadManager::URL, "%s/%016llX/tmd.%d", baseurl, GetTitleId(), version);

			if (!nodownload)
				manager.SetAttribute(DownloadManager::OUTPATH, "%s/tmd.%d", outdir, version);
		}
		else
		{
			manager
				.SetAttribute(DownloadManager::FILENAME, "tmd")
				.SetAttribute(DownloadManager::URL, "%s/%016llX/tmd", baseurl, GetTitleId());
		}

		DownloadManager::Downloader tmddownloader = manager.GetDownloader();

		if (!tmddownloader.Download(0, set_version ? write_opt_files : false))
			throw std::runtime_error("Failed to download TMD");

		tmdbuffer = (u8 *)tmddownloader.GetBufferAndDetach(tmdlen);

		if (!tmdbuffer || !tmdlen)
			throw std::runtime_error("Failed to download TMD to a buffer");

		TMD tmd(tmdbuffer, tmdlen);

		if (!set_version)
		{
			free(tmdbuffer);
			tmdbuffer = nullptr;

			version = tmd.GetTitleVersion();

			printf("%d", version);
			exit(0);

			manager
				.SetAttribute(DownloadManager::PROGRESS, true)
				.SetAttribute(DownloadManager::BUFFER, true)
				.SetAttribute(DownloadManager::FILENAME, "tmd.%d", version)
				.SetAttribute(DownloadManager::URL, "%s/%016llX/tmd.%d", baseurl, GetTitleId(), version);

			if (!nodownload)
				manager.SetAttribute(DownloadManager::OUTPATH, "%s/tmd.%d", outdir, version);

			tmddownloader = manager.GetDownloader();

			if (!tmddownloader.Download(0, write_opt_files))
				throw std::runtime_error("Failed to download TMD");

			tmdbuffer = (u8 *)tmddownloader.GetBufferAndDetach(tmdlen);

			if (!tmdbuffer || !tmdlen)
				throw std::runtime_error("Failed to download TMD to a buffer");
		}

		u16 content_count = tmd.GetContentCount();
		int tries = 0;

		for (u16 i = 0; i < content_count; i++)
		{
			u64 title_id = GetTitleId();
			u32 id = tmd.ChunkRecord(i).GetContentId();
			u64 content_size = tmd.ChunkRecord(i).GetContentSize();

			if (nodownload)
				printf("Content %08x - Not downloading\n\n", id);

			manager
				.SetAttribute(DownloadManager::FILENAME, "%08x", id)
				.SetAttribute(DownloadManager::URL, "%s/%016llX/%08X", baseurl, title_id, id);

			if (!nodownload)
				manager.SetAttribute(DownloadManager::OUTPATH, "%s/%08x", outdir, id);

			DownloadManager::Downloader downloader = manager.GetDownloader(content_size);

			for (int j = 0; j < 3; j++)
			{
				if (!downloader.Download(content_size, write_opt_files) && !nodownload)
				{
					if (downloader.GetCurlResult() == CURLE_HTTP_RETURNED_ERROR)
						printf("Failed downloading content [HTTP Status Code %ld] %08x (try %u / 3)\n", downloader.GetStatusCode(), id, ++tries);
					else 
						printf("Failed downloading content [CURL Result \"%s\"] %08x (try %u / 3)\n", curl_easy_strerror(downloader.GetCurlResult()), id, ++tries);
					
					continue;
				}

				break;
			}

			if (tries == 3)
			{
				printf(
					"\nAfter 3 attempts, the download failed. The file may not be available on the CDN at this time.\n"
					"A file named %08x_unavailable will be created to indicate the absence of the file.\n\n",
					id);

				// outdir len + "/" (1) + content id (8) + "_" (1) + "unavailable" (11) + \0 (1)
				size_t len = strlen(outdir) + 1 + 8 + 1 + 11 + 1;

				char *unavail_path = (char *)malloc(len);

				if (!unavail_path)
					throw std::bad_alloc();

				snprintf(unavail_path, len, "%s/%08x_unavailable", outdir, id);

				FILE* unavail_file = fopen(unavail_path, "w");
				free(unavail_path);
				fclose(unavail_file);
			}

			tries = 0;
		}

		if (write_opt_files)
		{
			time_t t = time(NULL);
			struct tm *tm = gmtime(&t);

			//                     outdir len     + / + info + . + txt + \0
			int infofile_pathlen = strlen(outdir) + 1 + 4    + 1 + 3   + 1;

			char *infofile_path = (char *)malloc(infofile_pathlen);

			if (!infofile_path)
				throw std::runtime_error("Could not allocate memory for info.txt file path");

			snprintf(infofile_path, infofile_pathlen, "%s/info.txt", outdir);

			FILE *f = fopen(infofile_path, "w");
			free(infofile_path);

			if (!f)
				throw std::runtime_error("Failed to open info.txt file in output directory");

			fprintf(
				f, 
				"%016llX\n"
				"%04d-%02d-%02d\n", 
				(unsigned long long)GetTitleId(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
			fclose(f);
		}
	}
	catch (...)
	{
		free(b64_encticket);
		free(b64_encticketkey);
		if (tmdbuffer) free(tmdbuffer);
		throw;
	}
	
	free(b64_encticket);
	free(b64_encticketkey);
	if (tmdbuffer) free(tmdbuffer);
}