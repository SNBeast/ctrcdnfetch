#define _DEFAULT_SOURCE
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>

#include "DirectoryManagement.hpp"

int Utils::DirectoryManagement::MakeDirectory(const char* path, bool recursive)
{
	if (!path) return errno = EFAULT;

	struct stat buffer;

	memset(&buffer, 0, sizeof(struct stat));

	errno = 0;

	if (stat(path, &buffer) == 0)
	{
		if ((buffer.st_mode & S_IFMT) != S_IFDIR) errno = ENOTDIR;
		if (!errno) return 0;
	}

	if (recursive)
	{
		char* copypath = (char*)calloc(strlen(path) + 1, 1);

		if (!copypath) return errno = ENOMEM;

		strcpy(copypath, path);

		char* workstr = copypath;

		std::string progressivepath("");

		if (path[0] == '/')
			progressivepath += '/';

		char* save = NULL;
		char* str = workstr;

		for (;; str = NULL)
		{
			char* part = strtok_r(str, "/", &save);

			if (!part) break; //we finished.

			progressivepath += part;

			if (stat(progressivepath.c_str(), &buffer) != 0)
			{
				if (errno != ENOENT) break;
				if (mkdir(progressivepath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)) break;

				errno = 0; //success? clear
			}
			else
			{
				if ((buffer.st_mode & S_IFMT) != S_IFDIR)
				{
					errno = ENOTDIR;
					break;
				}
			}

			progressivepath += '/';
		}

		free(copypath);
	}
	else if (errno == ENOENT)
	{
		if (mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0) errno = 0;
	}

	return errno;
}


int Utils::DirectoryManagement::RemoveDirectory(const char* path, bool recursive)
{
	if (!path) return errno = EFAULT;

	int ret = 0;

	char* fixedpath = NULL;

	if ((ret = FixUpPath(fixedpath, path, false)) != 0) return errno;

	struct stat buffer;

	if (lstat(fixedpath, &buffer) != 0) return errno;
	if ((buffer.st_mode & S_IFMT) != S_IFDIR) return errno = ENOTDIR;

	if (!recursive) //just try delete given path, as is
	{
		errno = 0;

		if (rmdir(fixedpath) != 0)
		{
			ret = errno;
		}

		free(fixedpath);
		return ret;
	}

	try {
		std::vector<RecursiveRmdirHelper> v;

		v.push_back(RecursiveRmdirHelper(fixedpath));

		free(fixedpath);

		fixedpath = nullptr;

		while (!v.empty())
		{
			while (v.back().index < v.back().path.size())
			{
				if (v.back().path[v.back().index].GetType() == 0)  //folder
				{
					size_t tmp = v.back().index++;

					v.push_back(RecursiveRmdirHelper(v.back().path + v.back().path[tmp]));

					continue;
				}
				else unlink((v.back().path + v.back().path[v.back().index]).c_str()); //file or other

				v.back().index++;
			}
			if (rmdir(v.back().path.GetPath())) ret = errno;

			v.pop_back();
		}
	}
	catch (std::length_error& e)
	{
		ret = ENOMEM;
	}
	catch (std::bad_alloc& e)
	{
		ret = ENOMEM;
	}
	catch (...)
	{
		ret = ECANCELED;
	}

	free(fixedpath); //only needed if got an exception before the free above on the try block

	return errno = ret;
}

int Utils::DirectoryManagement::DirectoryListing(DirFileList& output, const char* path)
{
	if (!path) return errno = EFAULT;

	DIR* dp;

	struct dirent* ep;
	struct stat buffer;

	memset(&buffer, 0, sizeof(struct stat));

	int ret = 0;

	if (stat(path, &buffer) != 0) return errno;
	if ((buffer.st_mode & S_IFMT) != S_IFDIR) return errno = ENOTDIR;
	if ((dp = opendir(path)) == NULL) return errno;

	output.Clear();

	while ((ep = readdir(dp)))
	{
		if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, "..")) continue;

		DirFileList::entry* dir_entry = new(std::nothrow) DirFileList::entry();

		if (!dir_entry)
		{
			ret = ENOMEM;
			break;
		}

		size_t length = strlen(ep->d_name) + 1;

		dir_entry->name = (char*)malloc(length);

		if (!dir_entry->name)
		{
			delete dir_entry;
			ret = ENOMEM;
			break;
		}

		strcpy(dir_entry->name, ep->d_name);

		try
		{
			std::string pathcheck(path);

			if (pathcheck.back() != '/') pathcheck += '/';

			pathcheck += dir_entry->name;

			if (lstat(pathcheck.c_str(), &buffer) != 0) break;
		}
		catch (...)
		{
			delete dir_entry;
			ret = ENOMEM;
			break;
		}

		dir_entry->type = ((buffer.st_mode & S_IFMT) == S_IFDIR ? 0 : ((buffer.st_mode & S_IFMT) == S_IFREG ? 1 : 2));

		try
		{
			output.entries.push_back(dir_entry);
		}
		catch (...)
		{
			delete dir_entry;
			ret = ENOMEM;
			break;
		}
	}

	if (!ret)
	{
		char* copy_path = NULL;

		if (!(ret = FixUpPath(copy_path, path))) output.original_path = copy_path;
	}

	if (ret) output.Clear();

	closedir(dp);

	return errno = ret;
}

int Utils::DirectoryManagement::PathIsDirectory(const char* path)
{
	if (!path) return errno = EFAULT;

	struct stat buffer;

	memset(&buffer, 0, sizeof(struct stat));

	errno = 0;

	if (stat(path, &buffer) != 0) return errno;

	return ((buffer.st_mode & S_IFMT) != S_IFDIR) ? errno = ENOTDIR : errno;
}

int Utils::DirectoryManagement::FixUpPath(char*& out, const char* path, bool ending_delimitor)
{
	if (!path) return errno = EFAULT;

	std::string fixedpath("");

	if (path[0] == '/' || path[0] == '\\') fixedpath += '/';

	char* copypath = (char*)malloc(strlen(path) + 1);

	if (!copypath) return errno = ENOMEM;

	strcpy(copypath, path);

	int ret = 0;
	char* tmp = NULL;

	try
	{
		char* save = NULL;
		char* str = copypath;

		for (;; str = NULL)
		{
			char* part = strtok_r(str, "/", &save);

			if (!part) break; //we finished.

			fixedpath += part;
			fixedpath += '/';
		}

		if (!ending_delimitor) fixedpath.pop_back();

		tmp = (char*)malloc(fixedpath.size() + 1);

		if (!tmp)
		{
			free(copypath);
			return errno = ENOMEM;
		}

		fixedpath.copy(tmp, fixedpath.size());

		out = tmp;
	}
	catch (std::length_error& e)
	{
		ret = ENOMEM;
		free(tmp);
	}
	catch (std::bad_alloc& e)
	{
		ret = ENOMEM;
		free(tmp);
	}
	catch (...)
	{
		ret = ECANCELED;
		free(tmp);
	}

	free(copypath);

	return errno = ret;
}
