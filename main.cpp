#include "DirectoryManagement.hpp"
#include "DownloadManager.hpp"
#include "SharedStorage.hpp"
#include "CDN.hpp"

#include <sys/stat.h>

struct arguments
{
	struct entry
	{
		const char* path = NULL;
		u16 version = 0;
		bool latest = true;
	};

	const char* proxy = NULL;

	std::vector<struct entry> files;

	bool printhelp = false;

	bool printresponseheaders = false;
	bool printhashes = false;
	bool writeextrafiles = false;

	bool nodownload = false;
};

static void load_certs(u8*& out_certs, const char* proxy)
{
	// DC153C2B8A0AC874A9DC78610E6A8FE3E6B134D5528873C961FBC795CB47E697
	static const u8 expecteddigest[SHA256_DIGEST_LENGTH] = { 0xDC, 0x15, 0x3C, 0x2B, 0x8A, 0x0A, 0xC8, 0x74, 0xA9, 0xDC, 0x78, 0x61, 0x0E, 0x6A, 0x8F, 0xE3, 0xE6, 0xB1, 0x34, 0xD5, 0x52, 0x88, 0x73, 0xC9, 0x61, 0xFB, 0xC7, 0x95, 0xCB, 0x47, 0xE6, 0x97 };

	u8 digest[SHA256_DIGEST_LENGTH];

	FILE* fp = NULL;
	u8* data = NULL;
	size_t read = 0;

	do
	{
		do
		{
			if (NintendoData::SharedStorage::Load(fp, "CA00000003-XS0000000c.bin"))
				break;

			data = (u8*)malloc(1792);

			if (!data) break;
			if ((read = fread(data, 1, 1792, fp)) == 1792) SHA256(data, 1792, digest);
		} while (0);

		if (fp) fclose(fp);
		if (data && read == 1792 && !memcmp(expecteddigest, digest, SHA256_DIGEST_LENGTH)) break;

		u8* ticket = NULL;

		try
		{
			if (!data) data = (u8*)malloc(1792);
			if (!data) break;

			DownloadManager manager;

			manager
				.SetAttribute(DownloadManager::URL, "http://ccs.cdn.c.shop.nintendowifi.net/ccs/download/0004013800000002/cetk")
				.SetAttribute(DownloadManager::BUFFER, true)
				.SetAttribute(DownloadManager::PROGRESS, false);

			if (proxy)
				manager.SetAttribute(DownloadManager::PROXY, proxy);

			auto downloader = manager.GetDownloader(0);

			if (!downloader.Download(0))
				break;

			size_t tiklen = 0;

			ticket = (u8*)downloader.GetBufferAndDetach(tiklen);

			if (!ticket || !tiklen)
			{
				free(ticket);
				break;
			}

			NintendoData::Ticket tik(ticket, tiklen);

			if (tiklen - tik.TotalSize() != 1792)
			{
				free(ticket);
				break;
			}

			u8* certs = ticket + tik.TotalSize();

			SHA256(certs, 1792, digest);

			if (memcmp(expecteddigest, digest, SHA256_DIGEST_LENGTH))
			{
				free(ticket);
				break;
			}

			NintendoData::SharedStorage::Save(certs, 1792, "CA00000003-XS0000000c.bin");

			memcpy(data, certs, 1792);

			read = 1792;

			free(ticket);

			break;
		}
		catch (...)
		{
			if (fp)
				fclose(fp);

			free(data);
			free(ticket);

			throw;
		}

		throw std::runtime_error("Couldn't load certs.");
	} while (0);

	out_certs = data;
}

static void print_usage(const char* executable_path)
{
	printf(
		"Usage: %s [options] tickets [...]\n\n"
		"Options:\n"
		"  -p, --proxy [uri]          To set a proxy before connecting.\n"
		"  -v, --version [version]    To set the version of the title to download.\n"
		"  -r, --response             Print response headers.\n"
		"  -s, --hashes               Print hashes for content/TMD.\n"
		"  -e, --extra-files          Write extra files for headers/hashes (depending on -r and -s) of content/TMD.\n"
		"  -n, --no-download          Doesn't download, implies -r.\n"
		"  -h, --help                 Shows this message.\n"
		"      --usage                Alias for help.\n\n"
		"Proxy format:\n"
		"  http[s]://[user[:password]@]example.org[:port]\n"
		"  socks4[a]://[user[:password]@]example.org[:port]\n"
		"  socks5[h]://[user[:password]@]example.org[:port]\n"
		"  ...and other schemes/formats libcurl supports.\n",
		executable_path);
}

static bool is_uri(const char* ptr)
{
	for (; ptr[0] && ptr[1] && ptr[2]; ptr++)
		if (ptr[0] == ':' && ptr[1] == '/' && ptr[2] == '/')
			return true;
	return false;
}

static bool parse_a_title(std::string& strerr, struct arguments& args, char* arg)
{
	const char* tik_err_reason = NULL;

	struct stat buffer;

	memset(&buffer, 0, sizeof(struct stat));

	if (stat(arg, &buffer) != 0)
		tik_err_reason = "Could not stat file.";

	if ((buffer.st_mode & S_IFMT) != S_IFREG)
		tik_err_reason = "Specified ticket file is not a regular file.";

	if (buffer.st_size < 848)
		tik_err_reason = "Ticket is smaller than expected.";

	if (tik_err_reason)
	{
		strerr += "Argument ignored: ";
		strerr += arg;
		strerr += "\n ";
		strerr += tik_err_reason;
		strerr += "\n";
		return false;
	}

	bool found = false;

	for (size_t foo = 0; foo < args.files.size(); foo++)
	{
		if (strcmp(arg, args.files[foo].path) == 0)
		{
			found = true;
			break;
		}
	}

	struct arguments::entry new_entry;

	new_entry.path = arg;

	if (!found)
		args.files.push_back(new_entry);

	return !found;
}

//Clean? Nope. Works? yup.
//Enjoy indentation mess.
static void parse_args(struct arguments& args, int argc, char** argv)
{
	std::string strerr;

	for (int i = 1; i < argc; i++)
	{
		if (!*argv[i])
			continue; //?? shouldn't happen but I'll leave it in as a fail safe..
		if (*argv[i] != '-')
		{
			parse_a_title(strerr, args, argv[i]);
			continue;
		}
		if (!argv[i][1])
		{
			strerr += "Argument ignored: ";
			strerr += argv[i];
			strerr += "\n";
			continue;
		}
		if (argv[i][1] != '-')
		{
			int current = i;
			for (int j = 1; argv[current][j]; j++)
			{
				switch (argv[current][j])
				{
				case 'h':
					args.printhelp = true;

					return;
				case 'p':
					if (!argv[i + 1])
					{
						strerr += "Argument ignored: p\n No proxy url argument found.\n";
						continue;
					}

					if (!is_uri(argv[i + 1]))
					{
						strerr += "Argument ignored: p\n No valid proxy url scheme found.\n";
						continue;
					}

					args.proxy = argv[++i];
					
					break;
				case 'r':
					args.printresponseheaders = true;
					break;
				case 's':
					if (args.nodownload)
					{
						strerr += "Argument ignored: s\n Cannot print hashes if not downloading.";
						continue;
					}

					args.printhashes = true;
					break;
				case 'e':
					if (args.nodownload)
					{
						strerr += "Argument ignored: e\n Cannot write extra files if not downloading.";
						continue;
					}

					args.writeextrafiles = true;
					break;
				case 'n':
					if (args.printhashes || args.writeextrafiles)
					{
						strerr += "Argument ignored: n\n Cannot print hashes / write extra files if not downloading.";
						continue;
					}

					args.nodownload = true;
					args.printresponseheaders = true;
					break;
				case 'v':
					{
						if (!argv[i + 1] || !argv[i + 2])
						{
							strerr += "Argument ignored: v\n No more arguments.\n";
							continue;
						}

						if (!parse_a_title(strerr, args, argv[i + 2]))
						{
							i += 2;
							
							strerr += "Argument ignored: v\n Bad ticket path.\n";
							continue;
						}
						
						u32 version = strtoul(argv[i + 1], NULL, 0);

						if (version > 0xffff)
						{
							args.files.pop_back();

							strerr += "Argument ignored: v\n Bad Version.\n Won't process:\n ";
							strerr += argv[i + 2];
							strerr += "\n";

							i += 2;
							continue;
						}

						u32 idx = args.files.size() - 1;

						args.files[idx].version = version;
						args.files[idx].latest = false;

						i += 2;

						break;
					}
				default:
					strerr += "Argument ignored: ";
					strerr += argv[current][j];
					strerr += "\n Doesn't exist.\n";

					break;
				}
			}
			continue;
		}
		//last but not least, -- arguments
		if (!argv[i][2])
		{
			strerr += "Argument ignored: ";
			strerr += argv[i];
			strerr += "\n";
			continue;
		}

		if (strcmp(&argv[i][2], "proxy") == 0)
		{
			if (!argv[i + 1])
			{
				strerr += "Argument ignored: p\n No proxy url argument found.\n";
				continue;
			}

			if (!is_uri(argv[i + 1]))
			{
				strerr += "Argument ignored: p\n No proxy url scheme found.\n";
				continue;
			}

			args.proxy = argv[++i];
			continue;
		}

		if (strcmp(&argv[i][2], "response") == 0)
		{
			args.printresponseheaders = true;
			continue;
		}

		if (strcmp(&argv[i][2], "hashes") == 0)
		{
			if (args.nodownload)
			{
				strerr += "Argument ignored: s\n Cannot print hashes if not downloading.";
				continue;
			}

			args.printhashes = true;
			continue;
		}

		if (strcmp(&argv[i][2], "extra-files") == 0)
		{
			if (args.nodownload)
			{
				strerr += "Argument ignored: e\n Cannot write extra files if not downloading.";
				continue;
			}

			args.writeextrafiles = true;
			continue;
		}

		if (strcmp(&argv[i][2], "no-download") == 0)
		{
			if (args.printhashes || args.writeextrafiles)
			{
				strerr += "Argument ignored: n\n Cannot print hashes / write extra files if not downloading.";
				continue;
			}

			args.nodownload = true;
			args.printresponseheaders = true;
			continue;
		}

		if (strcmp(&argv[i][2], "version") == 0)
		{
			if (!argv[i + 1] || !argv[i + 2])
			{
				strerr += "Argument ignored: v\n No more arguments.\n";
				continue;
			}

			if (!parse_a_title(strerr, args, argv[i + 2]))
			{
				i += 2;

				strerr += "Argument ignored: v\n Bad ticket path.\n";
				continue;
			}

			u32 version = strtoul(argv[i + 1], NULL, 0);

			if (version > 0xffff)
			{
				args.files.pop_back();

				strerr += "Argument ignored: v\n Bad Version.\n Won't process:\n ";
				strerr += argv[i + 2];
				strerr += "\n";

				i += 2;
				continue;
			}

			size_t index = args.files.size() - 1;

			args.files[index].version = version;
			args.files[index].latest = false;

			i += 2;
			continue;
		}

		if (strcmp(&argv[i][2], "help") == 0 || strcmp(&argv[i][2], "usage") == 0)
		{
			args.printhelp = true;

			return;
		}

		strerr += "Argument ignored: ";
		strerr += argv[i];
		strerr += "\n Doesn't exist.\n";
	}

	if (!strerr.empty())
		fprintf(stderr, "%s\n", strerr.c_str());
}

int main(int argc, char** argv)
{
	struct arguments args;

	try
	{
		parse_args(args, argc, argv);
	}
	catch (std::exception& e)
	{
		fprintf(stderr, "Something prevented the program to parse arguments.\n"
			"Can't continue.\n"
			"Caught exception message: %s\n",
			e.what());
		return 1;
	}

	if (args.printhelp || argc < 2)
	{
		print_usage(argv[0]);
		return 0;
	}

	if (args.writeextrafiles && !args.printhashes && !args.printresponseheaders)
	{
		fprintf(stderr, "Cannot write extra files if printing hashes/response headers is not enabled.\n");
		return 1;
	}

	if (args.proxy)
		printf("[INFO] Using proxy: %s\n", args.proxy);

	std::vector<u64> processedtids;

	u8* tikbuffer = (u8*)malloc(848);
	u8* certs = NULL;

	try
	{
		load_certs(certs, args.proxy);
	}
	catch (std::exception& e)
	{
		fprintf(stderr, "Something prevented the obtain ticket certificates.\n"
			"Can't continue.\n"
			"Caught exception message: %s\n",
			e.what());
		return 1;
	}

	if (!tikbuffer)
	{
		fprintf(stderr, "Something prevented the program to allocate memory to start.\n"
			"Can't continue.\n");
		return 1;
	}

	for (size_t i = 0; i < args.files.size(); i++)
	{
		//this only supports RSA_2048_SHA256 type and Root-CA00000003-XS0000000c issuer tickets.
		FILE* fp = fopen(args.files[i].path, "rb");

		if (!fp)
		{
			fprintf(stderr, "Failed to open \"%s\". Skipping...\n", args.files[i].path);
			continue;
		}

		size_t read_tik_bytes = fread(tikbuffer, 1, 848, fp);
		fclose(fp);

		if (read_tik_bytes != 848)
		{
			fprintf(stderr, "Failed to read 848 bytes from \"%s\". Skipping...\n", args.files[i].path);
			continue;
		}

		char* outpath = NULL;

		try
		{
			NintendoData::Ticket tik(tikbuffer, 848, true);
			u64 current_title_id = tik.TitleID();

			bool skip = false;

			for (size_t j = 0; j < processedtids.size(); j++)
			{
				if (processedtids[j] == current_title_id)
				{
					skip = true;
					break;
				}
			}

			if (skip)
			{
				printf("Skipping \"%s\", already downloaded title from ticket.\n", args.files[i].path);
				continue;
			}

			printf("Downloading Title ID %016llX...\n", (unsigned long long)current_title_id);

			NintendoData::CDN cdnaccess(tik);

			if (args.proxy)
				cdnaccess.SetProxy(args.proxy);

			cdnaccess.SetHeaderPrint(args.printresponseheaders);
			cdnaccess.SetHashesPrint(args.printhashes);

			if (!args.nodownload)
			{
				outpath = (char*)malloc(16 + 1);

				if (!outpath)
					throw std::bad_alloc();

				snprintf(outpath, 16 + 1, "%016llX", (unsigned long long)current_title_id);

				if (Utils::DirectoryManagement::MakeDirectory(outpath))
					throw std::runtime_error("Failed to create directory");
			}
			else
			{
				cdnaccess.SetNoDownload(true);
			}

			if (!args.files[i].latest)
				cdnaccess.SetVersion(args.files[i].version);

			cdnaccess.Download(outpath, args.writeextrafiles);

			free(outpath);
			outpath = NULL;
			processedtids.push_back(current_title_id);
		}
		catch (std::exception& e)
		{
			fflush(stdout);
			fprintf(stderr, "Something prevented the program to download target.\n"
				"Skipping \"%s\"...\n"
				"Caught exception message: %s\n\n",
				args.files[i].path, e.what());

			if (outpath)
				free(outpath);

			outpath = NULL;
			continue;
		}
	}

	free(tikbuffer);
	free(certs);

	return 0;
}