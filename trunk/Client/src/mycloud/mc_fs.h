#ifndef MC_FS_H
#define MC_FS_H

// Functions for filesystem interaction
#include "mc.h"
#include "mc_util.h"
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

FILE* fs_fopen(const string& filename, const string& mode);

/* Calculate (plain) MD5 hash of the file at fname */
int fs_filemd5(unsigned char hash[16], const string& fpath, size_t fsize);
int fs_filemd5(unsigned char hash[16], const string& fpath, size_t fsize, FILE *fdesc);

/* Get stats of a file */
int fs_filestats(mc_file_fs *file_fs, const string& fpath, const string& fname); //TODO: not used. deprecate?

/* List directory, expects / at end of path */
int fs_listdir(list<mc_file_fs> *l, const string& path);

/* Set mtime of a file */
int fs_touch(const string& path, int64 mtime, int64 ctime = 0);

/* Create a directory */
int fs_mkdir(const string& path);
int fs_mkdir(const string& path, int64 mtime, int64 ctime = 0);

/* Rename a file */
int fs_rename(const string& oldpath, const string& newpath);

/* Delete a file */
int fs_delfile(const string& path);

/* Delete a directory */
int fs_rmdir(const string& path);

/* Test wether a file exits (is readable) */
int fs_exists(const string& path);

#endif /* MC_FS_H */