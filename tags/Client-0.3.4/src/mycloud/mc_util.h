#ifndef MC_UTIL_H
#define MC_UTIL_H
#include "mc.h"
//#include <gcrypt.h> //for md5
#include <QtCore/QCryptographicHash>
#include <string>
#include <sstream>
#include <time.h>
#ifdef MC_OS_WIN
#	include <winsock2.h> /* FILETIME ?! */
#else
#	include <unistd.h> /* sleep */
#endif
#include "mc_types.h"


// Small utility and conversion functions
/* Choose a "smart" bufsize for e.g. md5 */
size_t choosebufsize(size_t fsize);

/* Convert a 128-bit hash to a hex string */
string MD5BinToHex(unsigned char binhash[16]);

/* Convert a string of binary data to a hex string */
string BinToHex(const string& data);

/* Calculate MD5 hash of string */
int strmd5(unsigned char hash[16], const string& str);

/* add a string identifying the file to s for hashing */
// Deprecated, use crypt_filestring
//void filestring(mc_sync_ctx *ctx, mc_file *f, string *s);

/* add a slash behind the name if it's a dir */
string printname(mc_file *f);
string printname(mc_file_fs *f);

/* crop and add ... where necessary */
string shortname(const string& s, size_t len);

string TimeToString(int64 time);
//negative precision indicates dynamic (up to 2)
string BytesToSize(int64 bytes, bool exact = true, int precision = 2);

string AddConflictExtension(string path);

#ifdef MC_OS_WIN
int64 FileTimeToPosix(FILETIME ft);
FILETIME PosixToFileTime(int64 ft);
#else
//TODO: Find linux version
#endif /* MC_OS_WIN */

void mc_sleep(int sec);
void mc_sleep_checkterminate(int sec);
void mc_sleepms(int ms);


#ifdef MC_OS_WIN
// Convert a wide Unicode string to an UTF8 string
std::string unicode_to_utf8(const std::wstring &wstr);

// Convert an UTF8 string to a wide Unicode String
std::wstring utf8_to_unicode(const std::string &str);
#endif


#endif /* MC_UTIL_H */