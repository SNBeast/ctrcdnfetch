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

			if (!nodownload)
				manager.SetAttribute(DownloadManager::OUTPATH, "%s/tmd", outdir);
		}

		DownloadManager::Downloader tmddownloader = manager.GetDownloader(0);

		if (!tmddownloader.Download(0, write_opt_files))
			throw std::runtime_error("Failed to download TMD");

		tmdbuffer = (u8*)tmddownloader.GetBufferAndDetach(tmdlen);

		if (!tmdbuffer || !tmdlen)
			throw std::runtime_error("Failed to download TMD to a buffer");

		TMD tmd(tmdbuffer, tmdlen);

		u16 tmdcontentcount = tmd.GetContentCount();

		int tries = 0;

		for (u16 i = 0; i < tmdcontentcount; i++)
		{
			u64 title_id = GetTitleId();
			u32 id = tmd.ChunkRecord(i).GetContentId();
			u64 content_size = tmd.ChunkRecord(i).GetContentSize();

			if (nodownload)
				printf("%08x\n\n", id);

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
					{
						printf("Failed downloading content [HTTP Status Code %ld] %08x (try %u / 3)\n", downloader.GetStatusCode(), id, ++tries);
					}
					else 
					{
						printf("Failed downloading content [CURL Result \"%s\"] %08x (try %u / 3)\n", curl_easy_strerror(downloader.GetCurlResult()), id, ++tries);
					}
					
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
				char unavail_path[len]; //base path len + content id len + 1
				snprintf(unavail_path, len, "%s/%08x_unavailable", outdir, id);

				FILE* unavail_file = fopen(unavail_path, "w");
				fclose(unavail_file);
			}

			tries = 0;
		}
	}
	catch (...)
	{
		free(b64_encticket);
		free(b64_encticketkey);
		free(tmdbuffer);
		throw;
	}
	free(b64_encticket);
	free(b64_encticketkey);
	free(tmdbuffer);
}