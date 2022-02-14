#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include "SharedStorage.hpp"
#include "DirectoryManagement.hpp"

namespace {
	struct storage
	{
		const char* const path;

		int getfullpath(char*& fullpath) const noexcept
		{
			const char* home_dir = getenv("HOME");

			if (!home_dir) return errno = EADDRNOTAVAIL;

			size_t length = snprintf(NULL, 0, path, home_dir) + 1;

			fullpath = (char*)malloc(length);

			if (!fullpath) return errno = ENOMEM;

			snprintf(fullpath, length, path, home_dir);

			return errno = 0;
		}

		int checkpath() const noexcept
		{
			char* fullpath = NULL;

			int ret = 0;

			if ((ret = getfullpath(fullpath)) != 0) return errno = ret;

			ret = Utils::DirectoryManagement::PathIsDirectory(fullpath);

			free(fullpath);

			return errno = ret;
		}

		int createpath() const noexcept
		{
			char* fullpath = NULL;
			int err = 0;

			if ((err = getfullpath(fullpath)) != 0) return errno = err;

			err = Utils::DirectoryManagement::MakeDirectory(fullpath);

			free(fullpath);

			return errno = err;
		}
	};

	static const struct storage storages[] =
	{
		{ "%s/.3ds" },
		{ "%s/3ds"  }
	};
}

int NintendoData::SharedStorage::Load(FILE*& out, const char* file) noexcept
{
	if (!file) return errno = EFAULT;
	if (!file[0]) return errno = EINVAL;

	out = NULL;

	bool storage_found = false;

	int err = 0;
	int i = 0;
	int numofstorages = sizeof(storages) / sizeof(struct storage);

	while (true)
	{
		for (; i < numofstorages; i++)
			if ((err = storages[i].checkpath()) == 0) break;

		if (i >= numofstorages) break;

		storage_found = true;

		char* fullstoragepath = NULL;

		if ((err = storages[i].getfullpath(fullstoragepath)) != 0)
			continue;

		size_t length = snprintf(NULL, 0, "%s/%s", fullstoragepath, file) + 1;

		char* fullpath = (char*)malloc(length);

		if (!fullpath)
		{
			free(fullstoragepath);
			err = ENOMEM;
			continue;
		}

		snprintf(fullpath, length, "%s/%s", fullstoragepath, file);
		free(fullstoragepath);

		out = fopen(fullpath, "rb");

		if (!out)
		{
			err = errno;
			i++;
		};

		free(fullpath);

		if (out)
		{
			err = 0;
			break;
		}
	}

	if (!storage_found)
	{
		for (i = 0; i < numofstorages; i++)
			if (!storages[i].createpath()) break;
	}

	return errno = err;
}

int NintendoData::SharedStorage::Save(const void* in, size_t inlen, const char* file) noexcept {
	if (!file || (!in && inlen)) return errno = EFAULT;
	if (!file[0]) return errno = EINVAL;

	int err = 0;
	int i;

	int numofstorages = sizeof(storages) / sizeof(struct storage);

	for (i = 0; i < numofstorages; i++)
		if ((err = storages[i].checkpath()) == 0) break;
	//failed, no storages.
	if (i >= numofstorages)
	{
		for (i = 0; i < numofstorages; i++)
			if ((err = storages[i].createpath()) == 0) break;
		//failed, again?
		if (i >= numofstorages) return errno = err;
	}

	err = 0;

	char* filepathcopy = (char*)malloc(strlen(file) + 1);

	if (!filepathcopy) return errno = ENOMEM;

	strcpy(filepathcopy, file);

	char* str = filepathcopy;
	char* part1 = NULL;
	char* part2 = NULL;
	char* save = NULL;

	char* dirpath = (char*)malloc(1);
	*dirpath = '\0';

	size_t currentsize = 1; //start at one because will refer to null.

	if (!dirpath)
	{
		free(filepathcopy);
		return errno = ENOMEM;
	}

	for (;; str = NULL) {
		part1 = strtok_r(str, "/", &save);

		if (!part1) break;
		if (part2)
		{
			currentsize += 1 + strlen(part2); // a / and the string

			char* tmp = (char*)realloc(dirpath, currentsize);

			if (!tmp)
			{
				free(dirpath);
				free(filepathcopy);
				return errno = ENOMEM;
			}

			dirpath = tmp;
			snprintf(dirpath, currentsize, "/%s", part2);
		}
		part2 = part1;
	}

	free(filepathcopy);

	char* storagepath = NULL;

	if ((err = storages[i].getfullpath(storagepath)) != 0)
	{
		free(dirpath);
		return errno = err;
	}

	size_t length = snprintf(NULL, 0, "%s%s", storagepath, dirpath) + 1;

	char* fullpath = (char*)malloc(length);

	if (!fullpath)
	{
		free(storagepath);
		free(dirpath);
		return errno = ENOMEM;
	}

	snprintf(fullpath, length, "%s%s", storagepath, dirpath);

	free(dirpath);

	err = Utils::DirectoryManagement::MakeDirectory(fullpath);

	free(fullpath);

	if (err)
	{
		free(storagepath);
		return errno = err;
	}

	length = snprintf(NULL, 0, "%s/%s", storagepath, file) + 1;

	fullpath = (char*)malloc(length);

	if (!fullpath)
	{
		free(storagepath);
		return errno = ENOMEM;
	}

	snprintf(fullpath, length, "%s/%s", storagepath, file);

	free(storagepath);

	FILE* fp = fopen(fullpath, "wb");

	if (!fp) return errno;
	if (inlen)
	{
		fwrite(in, inlen, 1, fp);
		if (fflush(fp) != 0) {
			err = errno;
			fclose(fp);
			remove(fullpath);
			free(fullpath);
			return errno = err;
		}
	}

	fclose(fp);
	free(fullpath);
	return errno = 0;
}