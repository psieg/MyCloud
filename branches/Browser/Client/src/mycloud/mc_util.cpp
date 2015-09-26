#include "mc_util.h"
#include <sys/stat.h> //needed for file stats  
#include <bitset>
#ifdef MC_QTCLIENT
#	include "mc_workerthread.h"
#else
#	include "mc_workerthread.h"
#endif


/* Choose a "smart" bufsize for e.g. md5 */
size_t choosebufsize(size_t fsize) {
	// Determine buffer size
	if (fsize < 1024) return 512; // < 1KB => 512B
	else if (fsize < 10240) return 1024; // < 10KB => 1KB
	else if (fsize < 102400) return 10240; // < 100KB => 10KB
	else if (fsize < 1048576) return 131072; // < 1MB => 128KB
	else if (fsize < 10485760) return 1048576; // < 10MB => 1MB
	else if (fsize < 104857600) return 10485760; // < 100MB => 10MB
	else /*if (fsize < 1073741824)*/ return 104857600;// < 1GB => 100MB
	//else return 1073741824; //dont allocate 1gig
}

/* Convert binary MD5 to hex string */
string MD5BinToHex(unsigned char binhash[16]) {
	string hash;
	char result[32] = "";
	static const char symbols[] = "0123456789abcdef";
	//MC_DBG("Converting MD5 to hex");
	for (short i = 0; i < 16; i++) {
		result[2*i] = symbols[(binhash[i] >> 4) & 0xf];
		result[2*i+1] = symbols[binhash[i] & 0xf];
	}
	hash.assign(result, 0, 32);
	return hash;
}

/* Convert a string of binary data to a hex string */
string BinToHex(const string& data) {
	std::ostringstream ret;
	const char* beg = reinterpret_cast<const char*>(data.c_str());
	const char* end = beg + data.length();
	static const char symbols[] = "0123456789abcdef";
	while (beg != end) ret << symbols[ (*beg >> 4) & 0xf] << symbols[ *beg++ &0xf ];
	return ret.str();
}

/* Calculate MD5 hash of a string */
int strmd5(unsigned char hash[16], const string& str) {
	QByteArray tmp = QCryptographicHash::hash(QByteArray(str.c_str(), str.length()), QCryptographicHash::Md5);

	memcpy(hash, tmp.constData(), sizeof(unsigned char)*16);

	return 0;
}

/* add a slash behind the name if it's a dir */
string printname(mc_file *f) {
	string s = f->name;
	if (f->is_dir) s.append("/");
	return s;
}
string printname(mc_file_fs *f) {
	string s = f->name;
	if (f->is_dir) s.append("/");
	return s;
}

/* truncate and add ... where necessary */
string shortname(const string& s, size_t len) {
	if (s.length()+3 > len) {
		return s.substr(0, len-3).append("...");
	} else {
		return s;
	}
}

string TimeToString(int64 time) {
	string ret;
	struct tm *ts;
	char buf[20];
	if (time == 0) {
		ret = "--.--.---- --:--:--";
	} else {
		ts = localtime((time_t*)&time);
		if (ts == NULL) ret = "Invalid Time Value";
		else {
			strftime(buf, 20, "%d.%m.%Y %H:%M:%S", ts);
			ret.assign(buf);
		}
	}
	return ret;
}

string BytesToSize(int64 bytes, bool exact, int precision) {
	std::ostringstream ret;
	if (precision >= 0) {
		ret.precision(precision);
		if (bytes < 1024) {
			ret.precision(0);
			ret << fixed << bytes << " B";
		} else if (bytes < 1048576) {
			ret << fixed << bytes/1024.0 << " KB";
		} else if (bytes < 1073741824) {
			ret << fixed << bytes/1048576.0 << " MB";
		} else if (bytes < 1099511627776) {
			ret << fixed << bytes/1073741824.0 << " GB";
		} else {
			ret << fixed << bytes/1099511627776.0 << " TB";
		}
	} else {
		if (bytes < 1024) {
			ret.precision(0);
			ret << fixed << bytes << " B";
		} else if (bytes < 10240) {
			ret.precision(2);
			ret << fixed << bytes/1024.0 << " KB";
		} else if (bytes < 102400) {
			ret.precision(1);
			ret << fixed << bytes/1024.0 << " KB";
		} else if (bytes < 1048576) {
			ret.precision(0);
			ret << fixed << bytes/1024.0 << " KB";
		} else if (bytes < 10485760) {
			ret.precision(2);
			ret << fixed << bytes/1048576.0 << " MB";
		} else if (bytes < 104857600) {
			ret.precision(1);
			ret << fixed << bytes/1048576.0 << " MB";
		} else if (bytes < 1073741824) {
			ret.precision(0);
			ret << fixed << bytes/1048576.0 << " MB";
		} else if (bytes < 10737418240) {
			ret.precision(2);
			ret << fixed << bytes/1073741824.0 << " GB";
		} else if (bytes < 107374182400) {
			ret.precision(1);
			ret << fixed << bytes/1073741824.0 << " GB";
		} else if (bytes < 1099511627776) {
			ret.precision(2);
			ret << fixed << bytes/1073741824.0 << " GB";
		} else if (bytes < 10995116277760) {
			ret.precision(1);
			ret << fixed << bytes/1073741824.0 << " GB";
		} else if (bytes < 109951162777600) {
			ret.precision(0);
			ret << fixed << bytes/1073741824.0 << " GB";
		} else {
			ret.precision(2);
			ret << fixed << bytes/1099511627776.0 << " TB";
		}

	}
	if (bytes >= 1024 && exact) ret << " (" << bytes << " byte)";
	return ret.str();
}

string AddConflictExtension(string path) {
	size_t posname, posext;
	posname = path.find_last_of("/");
	if (posname == string::npos) posname = 0;
	posext = path.find_last_of(".");
	if ((posext == string::npos) || posext < posname) posext = path.length();
	return path.insert(posext, MC_CONFLICTEXTENSION);
}

#ifdef MC_OS_WIN
int64 FileTimeToPosix(FILETIME ft) {
	return ((int64) ft.dwHighDateTime <<32 | ft.dwLowDateTime) / 10000000 - 11644473600;
}

FILETIME PosixToFileTime(int64 t) {
	FILETIME ft;
	int64 converted = (t + 11644473600) * 10000000;
	ft.dwHighDateTime = (converted & 0xFFFFFFFF00000000) >>32;
	ft.dwLowDateTime = converted & 0xFFFFFFFF;
	return ft;
}
#endif /* MC_OS_WIN */

#ifdef MC_OS_WIN
void mc_sleep(int time) {
	Sleep(time*1000);
}
void mc_sleep_checkterminate(int sec) {
	int t0 = time(NULL);
	MC_INF("Sleeping for " << sec << " secs");
	while (time(NULL)-t0 < sec) {
		if (MC_TERMINATING()) {
			MC_INF("Abort from GUI");
			break;
		}
		Sleep(2000);
	}
}
void mc_sleepms(int ms) {
	Sleep(ms);
}
#else
void mc_sleep(int time) {
	sleep(time);
}
void mc_sleep_checkterminate(int sec) {
	int t0 = time(NULL);
	MC_INF("Sleeping for " << sec << " secs");
	while (time(NULL)-t0 < sec) {
		if (MC_TERMINATING()) {
			MC_INF("Abort from GUI");
			break;
		}
		sleep(2000);
	}
}
void mc_sleepms(int time) {
	usleep(time*1000);
}
#endif /* MC_OS_WIN */


#ifdef MC_OS_WIN
// Convert a wide Unicode string to an UTF8 string
std::string unicode_to_utf8(const std::wstring &wstr)
{
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

// Convert an UTF8 string to a wide Unicode String
std::wstring utf8_to_unicode(const std::string &str)
{
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}
#endif /* MC_OS_WIN */