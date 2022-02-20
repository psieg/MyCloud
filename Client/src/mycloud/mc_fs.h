#ifndef MC_FS_H
#define MC_FS_H

// Functions for filesystem interaction
#include "mc.h"
#include "mc_util.h"
#include "mc_workerthread.h"
#include <QtCore/QCryptographicHash>
#ifdef MC_OS_WIN
#include <sys/utime.h>
#include <direct.h>
#else
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

enum MC_FILEACCESS : int {
	MC_FA_READ,
	MC_FA_OVERWRITECREATE,
	MC_FA_READWRITEEXISTING
};

/* For platform independence */
FILE* fs_fopen(const std::string& filename, MC_FILEACCESS access);
int fs_fseek(FILE *f, int64 offset, int origin);
int64 fs_ftell(FILE *f);
int fs_fclose(FILE *f);

/* Calculate (plain) MD5 hash of the file at fname */
int fs_filemd5(unsigned char hash[16], const std::string& fpath, size_t fsize);
int fs_filemd5(unsigned char hash[16], size_t fsize, FILE *fdesc);

/* Get stats of a file */
int fs_filestats(mc_file_fs *file_fs, const std::string& fpath, const std::string& fname, bool fileMightBeGone = false);

/* list directory, expects / at end of path */
int fs_listdir(std::list<mc_file_fs> *l, const std::string& path);

/* Set mtime of a file */
int fs_touch(const std::string& path, int64 mtime, int64 ctime = 0);

/* Create a directory */
int fs_mkdir(const std::string& path);
int fs_mkdir(const std::string& path, int64 mtime, int64 ctime = 0);

/* Rename a file */
int fs_rename(const std::string& oldpath, const std::string& newpath);

/* Delete a file */
int fs_delfile(const std::string& path);

/* Delete a directory */
int fs_rmdir(const std::string& path);

/* Test wether a file exits (is readable) */
bool fs_exists(const std::string& path);

/* Test wether a file is an existing directory */
bool fs_isdir(const std::string& path);

#endif /* MC_FS_H */