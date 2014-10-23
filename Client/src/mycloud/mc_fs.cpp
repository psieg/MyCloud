#include "mc_fs.h"
#ifdef MC_QTCLIENT
#	ifdef MC_IONOTIFY
#		include "qtClient.h"
#	endif
#endif

#ifdef MC_OS_WIN
// Convert a wide Unicode string to an UTF8 string
std::string unicode_to_utf8(const std::wstring &wstr)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo( size_needed, 0 );
    WideCharToMultiByte                  (CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Convert an UTF8 string to a wide Unicode String
std::wstring utf8_to_unicode(const std::string &str)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar                  (CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
#endif


#ifdef MC_OS_WIN
FILE* fs_fopen(const string& filename, const string& mode) {
	return _wfsopen(utf8_to_unicode(filename).c_str(), utf8_to_unicode(mode).c_str(), _SH_DENYNO);

}
int fs_fseek(FILE *f, int64 offset, int origin) {
	return _fseeki64(f, offset, origin);
}
int64 fs_ftell(FILE *f) {
	return _ftelli64(f);
}
int fs_fclose(FILE *f) {
	return fclose(f);
}
#else
FILE* fs_fopen(const string& filename, const string& mode) {
	return fopen(filename.c_str(), mode.c_str());
}
int fs_fseek(FILE *f, int64 offset, int origin) {
	return fseeko(f, offset, origin);
}
int64 fs_ftell(FILE *f) {
	return ftello(f);
}
int fs_fclose(FILE *f) {
	return fclose(f);
}
#endif

/* Calculate MD5 hash from filename when the file is already open (rb, seek_set) */
int fs_filemd5(unsigned char hash[16], size_t fsize, FILE *fdesc) {
	size_t bufsize;
	char* fbuf;
	QCryptographicHash cry(QCryptographicHash::Md5);
	int i;

	bufsize = choosebufsize(fsize);
	fbuf = (char*) malloc(bufsize);
	if (fbuf == 0) {
		bufsize = bufsize/10;
		fbuf = (char*) malloc(bufsize);
		MC_MEM(fbuf, bufsize);
	}	
	
	for (i = 0; (i+1)*bufsize<fsize; i++) {
		MC_NOTIFYIOSTART(MC_NT_FS);
		fread(fbuf, bufsize, 1, fdesc);
		MC_NOTIFYIOEND(MC_NT_FS);
		
		if (MC_TERMINATING()) {
			free(fbuf);
			return MC_ERR_TERMINATING;
		}
		cry.addData(fbuf, bufsize);
	}

	memset(fbuf, 0, bufsize);
	fread(fbuf, fsize-i*bufsize, 1, fdesc);
	cry.addData(fbuf, fsize-i*bufsize);
	

	free(fbuf);

	QByteArray h = cry.result();
	memcpy(hash, h.constData(), sizeof(unsigned char)*16);

	return 0;
}
/* Calculate MD5 hash from filename when the file size is known */
int fs_filemd5(unsigned char hash[16], const string& fpath, size_t fsize) {
	FILE *fdesc;
	int rc;
	MC_DBGL("Opening " << fpath);
	fdesc = fs_fopen(fpath, "rb");
	if (!fdesc) MC_ERR_MSG(-1, "Can't open file");
	
	rc = fs_filemd5(hash, fsize, fdesc);

	fs_fclose(fdesc);

	return rc;
}

/* Get stats of a file */
//TODO: linux version
#ifdef MC_OS_WIN
int fs_filestats(mc_file_fs *file_fs, const string& fpath, const string& fname) {
	//struct _stat64 attr;
	HANDLE h;
	FILETIME ctime, mtime;
	LARGE_INTEGER size;
	DWORD attr;
	int rc;
	MC_DBGL("Getting stats of " << fname);
	// stat's time info can't be trusted, as it will be offset by 1 h if DST is active, though it
	// is supposed to be UTC
	//rc =_wstat64(utf8_to_unicode(fpath).c_str(), &attr);
	//MC_CHKERR_MSG(rc, "Failed to get stats of file");
	file_fs->name = fname;
	h = CreateFile(utf8_to_unicode(fpath).c_str(), NULL, NULL, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (h == INVALID_HANDLE_VALUE) MC_ERR_MSG(MC_ERR_IO, "CreateFile failed: " << GetLastError());

	if (!GetFileTime(h, &ctime, NULL, &mtime)) MC_ERR_MSG(MC_ERR_IO, "GetFileTime failed: " << GetLastError());
	file_fs->ctime = FileTimeToPosix(ctime);
	file_fs->mtime = FileTimeToPosix(mtime);

	if (!GetFileSizeEx(h, &size)) MC_ERR_MSG(MC_ERR_IO, "GetFileSizeEx failed: " << GetLastError());
	file_fs->size = size.QuadPart;

	attr = GetFileAttributes(utf8_to_unicode(fpath).c_str());
	if (attr == INVALID_FILE_ATTRIBUTES) MC_ERR_MSG(MC_ERR_IO, "GetFileAttributes failed: " << GetLastError());
	file_fs->is_dir = (attr & FILE_ATTRIBUTE_DIRECTORY);

	CloseHandle(h);

	return 0;
}
#else
int fs_filestats(mc_file_fs *file_fs, const string& fpath, const string& fname) {
	struct stat st;
	int rc;
	MC_DBGL("Getting stats of " << fname);

	rc = stat(fpath.c_str(), &st);
	if (rc) MC_ERR_MSG(MC_ERR_IO, "stat failed: " << errno);
	file_fs->name = fname;
	file_fs->ctime = st.st_ctime;
	file_fs->mtime = st.st_mtime;
	file_fs->is_dir = S_ISDIR(st.st_mode);
	file_fs->size = (file_fs->is_dir?0:st.st_size);

	return 0;
}
#endif

#ifdef MC_OS_WIN
int fs_listdir(list<mc_file_fs> *l, const string& path) {
	WIN32_FIND_DATA fd;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	mc_file_fs file;
	string scanpath, fpath;
	int rc;
	MC_DBGL("Listing " << path);
	scanpath.assign(path).append("*");
	MC_NOTIFYIOSTART(MC_NT_FS);
	hFile = FindFirstFileW(utf8_to_unicode(scanpath).c_str(), &fd); /* Project Encoding set to Mutlibyte (...A) (correct?) */
	if (hFile == INVALID_HANDLE_VALUE) {		
		MC_NOTIFYIOEND(MC_NT_FS);
		MC_ERR_MSG(MC_ERR_IO, "FindFirstFile failed for " << scanpath << ": " << GetLastError());
	}
	/* First result "." can be skipped */
	while (FindNextFileW(hFile, &fd) != 0) {
		memset(&file, 0, sizeof(mc_file_fs));
		file.name.assign(unicode_to_utf8(wstring(fd.cFileName)));
		if (file.name != "..") {
			//as we dont trust FindFirstFiles's time info, we have to stat every file manually
			//wrong, we can't trust _stat64's time info
			//fpath.assign(path).append(unicode_to_utf8(wstring(fd.cFileName)));
			//rc = fs_filestats(&file, fpath, file.name);
			//MC_CHKERR(rc);
			file.ctime = FileTimeToPosix(fd.ftCreationTime);
			file.mtime = FileTimeToPosix(fd.ftLastWriteTime);
			file.size = (int64)fd.nFileSizeHigh <<32 | fd.nFileSizeLow;
			file.is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
			l->push_back(file);
		}
	}
	FindClose(hFile);
	MC_NOTIFYIOEND(MC_NT_FS);
	MC_CHKERR_EXP(GetLastError(), ERROR_NO_MORE_FILES, "FindNextFile failed: " << GetLastError());
	return 0;
}
#else
int fs_listdir(list<mc_file_fs> *l, const string& path) {
	DIR *d;
	struct dirent *dent;
	struct stat st;
	mc_file_fs file;
	string fpath;
	int rc;
	MC_DBGL("Listing " << path);
	
	MC_NOTIFYIOSTART(MC_NT_FS);
	d = opendir(path.c_str());
	if (d == NULL) {
		MC_NOTIFYIOEND(MC_NT_FS);
		MC_ERR_MSG(MC_ERR_IO, "opendir failed: " << errno);
	}

	errno = 0;
	while ((dent = readdir(d)) != NULL) {
		if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) continue;
		fpath.assign(path).append(dent->d_name);

		rc = stat(fpath.c_str(), &st);
		if (rc) MC_ERR_MSG(MC_ERR_IO, "stat failed: " << errno);
		file.name = dent->d_name;
		file.ctime = st.st_ctime;
		file.mtime = st.st_mtime;
		file.is_dir = S_ISDIR(st.st_mode);
		file.size = (file.is_dir?0:st.st_size);
		l->push_back(file);
	}
	closedir(d);
	MC_NOTIFYIOEND(MC_NT_FS);
	if (errno) MC_ERR_MSG(MC_ERR_IO, "readdir failed: " << errno);

	return 0;
}
#endif

/* Set mtime of a file */
#ifdef MC_OS_WIN
int fs_touch(const string& path, int64 mtime, int64 ctime) {
	int rc;
	//__utimbuf64 buf;
	HANDLE h;
	FILETIME mt, ct;
	if (ctime == 0)
		MC_DBGL("Touching " << path << " with m" << mtime)
	else
		MC_DBGL("Touching " << path << " with m" << mtime << "/c" << ctime);
	//buf.actime = time(NULL);
	//buf.modtime = mtime;
	//rc = _wutime64(utf8_to_unicode(path).c_str(), &buf); //DST crap
	//if (rc) {
	//	if (errno == EACCES) { //... path is a directory
			h = CreateFileW(utf8_to_unicode(path).c_str(), FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
			if (h == INVALID_HANDLE_VALUE)
				MC_ERR_MSG(MC_ERR_IO, "Failed to set mtime for file " << path << ": " << GetLastError());

			mt = PosixToFileTime(mtime);

			if (ctime == 0) {
				rc = SetFileTime(h, NULL, NULL, &mt);
			} else {
				ct = PosixToFileTime(ctime);
				rc = SetFileTime(h, &ct, NULL, &mt);
			}
			if (rc == 0) 
				MC_ERR_MSG(MC_ERR_IO, "Failed to set mtime for file " << path << ": " << GetLastError());

			CloseHandle(h);
	//	} else {
	//		MC_ERR_MSG(MC_ERR_IO, "Failed to set mtime for file " << path << ": " << errno);
	//	}
	//}
	return 0;
}
#else
int fs_touch(const string& path, int64 mtime, int64 ctime) {
	int rc;
	struct utimbuf buf;
	if (ctime == 0)
		MC_DBGL("Touching " << path << " with m" << mtime)
	else
		MC_DBGL("Touching " << path << " with m" << mtime << "/c" << ctime);
	buf.actime = 0;
	buf.modtime = mtime;
	//TODO: set c(reation)time on linux?!
	rc = utime(path.c_str(), &buf);
	MC_CHKERR_MSG(rc, "Failed to set mtime for file " << path);
	return 0;
}
#endif

/* Create a directory */
#ifdef MC_OS_WIN
int fs_mkdir(const string& path, int64 mtime, int64 ctime) {
	int rc;
	MC_DBGL("Creating directory " << path);
	rc = _wmkdir(utf8_to_unicode(path).c_str());
	MC_CHKERR_MSG(rc, "Failed to create directory " << path);
	if (mtime != -1) {
		return fs_touch(path, mtime, ctime);
	} else {
		return 0;
	}
}
int fs_mkdir(const string& path) {
	int rc;
	MC_DBGL("Creating directory " << path);
	rc = _wmkdir(utf8_to_unicode(path).c_str());
	MC_CHKERR_MSG(rc, "Failed to create directory " << path);
	return 0;
}
#else
int fs_mkdir(const string& path, int64 mtime, int64 ctime) {
	int rc;
	MC_DBGL("Creating directory " << path);
	rc = mkdir(path.c_str(), 0777);
	MC_CHKERR_MSG(rc, "Failed to create directory " << path);
	if (mtime != -1) {
		return fs_touch(path, mtime, ctime);
	} else {
		return 0;
	}
}
int fs_mkdir(const string& path) {
	int rc;
	MC_DBGL("Creating directory " << path);
	rc = mkdir(path.c_str(), 0777);
	MC_CHKERR_MSG(rc, "Failed to create directory " << path);
	return 0;
}
#endif

/* Rename a file */
#ifdef MC_OS_WIN
int fs_rename(const string& oldpath, const string& newpath) {
	int rc;
	MC_DBGL("Renaming " << oldpath << " to " << newpath);
	rc =_wrename(utf8_to_unicode(oldpath).c_str(), utf8_to_unicode(newpath).c_str());
	MC_CHKERR_MSG(rc, "Rename failed: " << errno << ": " << strerror(errno));
	return 0;
}
#else
int fs_rename(const string& oldpath, const string& newpath) {
	int rc;
	MC_DBGL("Renaming " << oldpath << " to " << newpath);
	rc = rename(oldpath.c_str(), newpath.c_str());
	MC_CHKERR_MSG(rc, "Rename failed: " << errno << ": " << strerror(errno));
	return 0;
}
#endif

/* Delete a file */
#ifdef MC_OS_WIN
int fs_delfile(const string& path) {
	int rc;
	MC_DBGL("Deleting file " << path);
	rc = _wunlink(utf8_to_unicode(path).c_str());	
	MC_CHKERR_MSG(rc, "Unlink failed: " << errno << ": " << strerror(errno));
	return 0;
}
#else
int fs_delfile(const string& path) {
	int rc;
	MC_DBGL("Deleting file " << path);
	rc = unlink(path.c_str());	
	MC_CHKERR_MSG(rc, "Unlink failed: " << errno << ": " << strerror(errno));
	return 0;
}
#endif

/* Delete a directory */
#ifdef MC_OS_WIN
int fs_rmdir(const string& path) {
	int rc;
	MC_DBGL("Deleting dir " << path);
	rc = _wrmdir(utf8_to_unicode(path).c_str());
	MC_CHKERR_MSG(rc, "Rmdir failed: " << errno << ": " << strerror(errno));
	return 0;
}
#else
int fs_rmdir(const string& path) {
	int rc;
	MC_DBGL("Deleting dir " << path);
	rc = rmdir(path.c_str());
	MC_CHKERR_MSG(rc, "Rmdir failed: " << errno << ": " << strerror(errno));
	return 0;
}
#endif

/* Test wether a file exists (is readable) - CARE: may be case insensitive */
#ifdef MC_OS_WIN
int fs_exists(const string& path) {
	/*FILE *f;

	f = fs_fopen(path, "rb");
	if (!f) return false;
	fs_fclose(f);
	return true;*/
	DWORD ftyp = GetFileAttributes(utf8_to_unicode(path).c_str());
	if (ftyp != INVALID_FILE_ATTRIBUTES)
		return true;
	else
		//if (GetLastError() == ERROR_FILE_NOT_FOUND)
		//	return false; 
		return false;
}
#else
int fs_exists(const string& path) {
	/*FILE *f;

	f = fs_fopen(path, "rb");
	if (!f) return false;
	fs_fclose(f);
	return true;*/
	struct stat st;
	return (stat(path.c_str(), &st) == 0);
}
#endif