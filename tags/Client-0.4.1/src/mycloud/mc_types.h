#ifndef MC_TYPES_H
#define MC_TYPES_H
#include "mc.h"

using namespace std;

/* config */
#define MC_MAXTIMEDIFF	10
#define MC_NETTIMEOUT	60
#define MC_NOSYNCCHECK	60*30
#define MC_NOSYNCWARN	3600*24
#define MC_SENDBLOCKSIZE 1024*1024*10 // 10MB
#define MC_RECVBLOCKSIZE 1024*1024*10 // 10MB

#ifdef MC_OS_WIN
	typedef __int64 int64;
#else
#	include <inttypes.h>
	typedef int64_t int64;
#	include <cstdlib> //malloc
#endif
#include "openssl/evp.h"

#ifdef MC_QTCLIENT
/* Events sent to QtClient with the MC_NOTIFY macros */
// for MC_NOTIFY
enum MC_NOTIFYSINGLETYPE : int {
	MC_NT_FULLSYNC			= 0, 
	MC_NT_ERROR				= 1, 
	MC_NT_NOSYNCWARN		= 2, 
	MC_NT_INCOMPATIBLE_FS	= 3, 
	MC_NT_CRYPTOFAIL		= 4, 
	MC_NT_SERVERRESET		= 5,
	MC_NT_TLSFAIL			= 6
};
// for MC_NOTIFYSTART / MC_NOTIFYEND
enum MC_NOTIFYSTATETYPE : int {
	MC_NT_CONN			= 0, 
	MC_NT_SYNC			= 1, 
	MC_NT_UL			= 2, 
	MC_NT_DL			= 3, 
};
// form MC_NOTIFYIOSTART / MC_NOTIFYIOEND
#ifdef MC_IONOTIFY
enum MC_NOTIFYIOTYPE : int {
	MC_NT_DB	= 0, 
	MC_NT_FS	= 1, 
	MC_NT_SRV	= 2
};
#endif

/* conflict dialog filename maxlen */
#	define MC_QTCONFLICTMAXLEN	35
#endif

//what to add to conflict files to be solve manually
//should be on the server's ignore list!
#define MC_CONFLICTEXTENSION	".mc_conflict"

#define MC_UNTRUSTEDPREFIX		"UNTRUSTED:"

#define MC_OK					0	//implicit, cant change
#define MC_ERR_DB				1
#define MC_ERR_PROTOCOL			2
#define MC_ERR_BADQRY			3	//TODO: candidate for deletion, use SERVER?
#define MC_ERR_SERVER			5
#define MC_ERR_LOGIN			6
#define MC_ERR_TIMEDIFF			7
#define MC_ERR_IO				8
#define MC_ERR_NETWORK			9
#define MC_ERR_VERIFY			10
#define MC_ERR_NOT_CONFIGURED	11
#define MC_ERR_CRYPTO			12
#define MC_ERR_CRYPTOALERT		13 //crypt_finish_download: TAG mismatch after download = the server can't be trusted
#define MC_ERR_INCOMPATIBLE_FS	14 //the FS is case sensitive or worse - it claims files exist (fs_exist) altough they are not (in this casing) in directory listing
//These aren't really errors
#define MC_ERR_ALTCODE			20 //srv_perform: Server replied with altcode rather than requiredcode
#define MC_ERR_AUTODECIDE_UP	21 //autodecide: automatic decision upload
#define MC_ERR_AUTODECIDE_DOWN	22 //autodecide: automatic decision download
#define MC_ERR_AUTODECIDE_SKIP	23 //autodecide: automatic decision skip
#define MC_ERR_AUTODECIDE_NONE	24 //autodecide: no automatic choice
#define MC_ERR_INCOMPLETE_SKIP	25 //download: file is incomplete_up, those files are not downloaded
#define MC_ERR_CONFLICT_SKIP	26 //conflicted: the conflict was skipped
#define MC_ERR_UNCLEAN_WALK		27 //walk: noncritical errors occured
#define MC_ERR_NOTFULLYSYNCED	28 //fullsync: Not all hashes match
#define MC_ERR_NOCHANGE			29 //srv_notifychange: server returned no change

#define MC_ERR_TERMINATING		100 //Asynchronous terminate request from GUI
//Hopefully dev only
#define MC_ERR_NOT_IMPLEMENTED	101
#define MC_IS_CRITICAL_ERR(rc)		(rc && (rc == MC_ERR_TERMINATING || rc == MC_ERR_NETWORK || rc == MC_ERR_LOGIN || rc == MC_ERR_TIMEDIFF || rc == MC_ERR_BADQRY || rc == MC_ERR_PROTOCOL || rc == MC_ERR_CRYPTO || rc == MC_ERR_CRYPTOALERT))


#define MC_SYNCID_NONE -1
#define MC_FILTERID_NONE -1
#define MC_FILEID_NONE -1

/* used by db */
/* These structs defines the format of all tables used, each struct represents one row of the specific table 
 *  they are meant to be used when accessing the database, as mc_db will use them to write binary data
 *  (instead of converting stuff to char strings to use them in db_execstr).
 */
typedef struct _mc_status {
	bool locked;
	string url;
	string uname;
	string passwd;
	bool acceptallcerts;
	int watchmode;
	int64 basedate;
	int64 updatecheck;
	string updateversion;
	int uid;
	int64 lastconn;
} mc_status;

enum MC_SYNCSTATUS : int {
	MC_SYNCSTAT_UNKOWN		= 0, 
	MC_SYNCSTAT_RUNNING		= 1, 
	MC_SYNCSTAT_COMPLETED	= 2, 
	MC_SYNCSTAT_SYNCED		= 3, 
	MC_SYNCSTAT_UNAVAILABLE	= 4, 
	MC_SYNCSTAT_FAILED		= 5, 
	MC_SYNCSTAT_ABORTED		= 6, 
	MC_SYNCSTAT_DISABLED	= 7, 
	MC_SYNCSTAT_CRYPTOFAIL	= 8
};

enum MC_FILESTATUS : int {
	MC_FILESTAT_COMPLETE			= 0, 
	MC_FILESTAT_DELETED				= 1, 
	MC_FILESTAT_INCOMPLETE_UP		= 2, 	// for files only: in progress of being uploaded
	MC_FILESTAT_INCOMPLETE_UP_ME	= 3, 	// for files only: in progress of being uploaded by me
	MC_FILESTAT_INCOMPLETE_DOWN		= 4, 
};

enum MC_FILTERTYPE : int {
	MC_FILTERT_MATCH_NAME			= 0, 
	MC_FILTERT_MATCH_EXTENSION		= 1, 
	MC_FILTERT_MATCH_FULLNAME		= 2, 
	MC_FILTERT_MATCH_PATH			= 3, 
	MC_FILTERT_REGEX_NAME			= 10, 
	MC_FILTERT_REGEX_EXTENSION		= 11, 
	MC_FILTERT_REGEX_FULLNAME		= 12, 
	MC_FILTERT_REGEX_PATH			= 13
};

enum MC_CONFLICTACTION : int {
	MC_CONFLICTACT_UNKNOWN	= 0, 	//not decided yet
	MC_CONFLICTACT_SKIP		= 1, 
	MC_CONFLICTACT_KEEP		= 2, 
	MC_CONFLICTACT_UP		= 3, 
	MC_CONFLICTACT_DOWN		= 4
};

enum MC_CONFLICTRECOMMENDATION : int {
	MC_CONFLICTREC_DONTKNOW	= 0, 
	MC_CONFLICTREC_UP		= 1, 
	MC_CONFLICTREC_DOWN		= 2
};

inline bool compare_hashstr(const string& a, const string& b) { return *((int*)a.c_str()) < *((int*)b.c_str()); }

struct mc_sync {
	int id;
	int uid;
	string name;
	int filterversion;
	int shareversion;
	bool crypted;
	unsigned char hash[16];

	inline friend bool operator<(const mc_sync& left, const mc_sync& right) { return left.id < right.id; }
	inline friend bool operator>(const mc_sync& left, const mc_sync& right) { return left.id > right.id; }
};

/* mc_sync */
/* data of sync in db. includes local directory */
struct mc_sync_db {
	int id;
	int uid;
	int priority;
	string name;
	string path;
	int filterversion;
	int shareversion;
	bool crypted;
	MC_SYNCSTATUS status;
	int64 lastsync;
	unsigned char hash[16];
	unsigned char cryptkey[32];

	inline friend bool operator<(const mc_sync_db& left, const mc_sync_db& right) { return left.id < right.id; }
	inline friend bool operator>(const mc_sync_db& left, const mc_sync_db& right) { return left.id > right.id; }
};
inline bool compare_mc_sync_db_prio(mc_sync_db a, mc_sync_db b) { return a.priority < b.priority; }

struct mc_filter {
	int id;
	int sid;
	bool files;
	bool directories;
	MC_FILTERTYPE type;
	string rule;
};

struct mc_share {
	int sid;
	int uid;
};

struct mc_user {
	int id;
	string name;
};

struct mc_keyringentry {
	string sname;
	string uname;
	unsigned char key[32];
};

struct mc_sync_ctx {
	mc_sync_db *sync;
	list<mc_filter> *filter;
	MC_CONFLICTACTION dirconflictact;	// confict action for current directory
	bool rconflictact;					// wether dirconflictact is recursive
};
inline void init_sync_ctx(mc_sync_ctx *ctx, mc_sync_db *sync, list<mc_filter> *filter) { 
	ctx->dirconflictact = MC_CONFLICTACT_UNKNOWN; ctx->rconflictact = false; 
	ctx->sync = sync; ctx->filter = filter; };

/* helper */
// comparison, not case sensitive.
inline bool nocase_smaller (const std::string& first, const std::string& second)
{
  unsigned int i=0;
  while ( (i<first.length()) && (i<second.length()) )
  {
	if (tolower(first[i])<tolower(second[i])) return true;
	else if (tolower(first[i])>tolower(second[i])) return false;
	++i;
  }
  if (first.length()<second.length()) return true;
  else return false;
}
inline bool nocase_greater (const std::string& first, const std::string& second)
{
  unsigned int i=0;
  while ( (i<first.length()) && (i<second.length()) )
  {
	if (tolower(first[i])>tolower(second[i])) return true;
	else if (tolower(first[i])<tolower(second[i])) return false;
	++i;
  }
  if (first.length()>second.length()) return true;
  else return false;
}
inline bool nocase_equals (const std::string& first, const std::string& second)
{
  unsigned int i=0;
  while ( (i<first.length()) && (i<second.length()) )
  {
	if (tolower(first[i])>tolower(second[i])) return false;
	else if (tolower(first[i])<tolower(second[i])) return false;
	++i;
  }
  if (first.length()==second.length()) return true;
  else return false;
}

struct mc_file {
	int id; // negative if we don't know the server's id
	string name;
	string cryptname; // = "" for unencrypted
	int64 ctime;
	int64 mtime;
	int64 size;
	bool is_dir;
	int parent; // negative for parent is a sync
	MC_FILESTATUS status;
	unsigned char hash[16];
};
inline bool compare_mc_file(mc_file a, mc_file b) { return a.name < b.name; }
inline bool compare_mc_file_id(mc_file a, mc_file b) { return a.id < b.id; }

/* used by fs */
/* smaller mc_file with information available directly from the os (= without md5 hashes) */
struct mc_file_fs {
	string name;
	int64 ctime;
	int64 mtime;
	int64 size;
	bool is_dir;
};
inline bool compare_mc_file_fs(mc_file_fs a, mc_file_fs b) { return a.name < b.name; }

/* used by srv */
struct mc_buf {
  char *mem;
  size_t size;
  size_t used;
};

inline void SetBuf(mc_buf *buf) {
	buf->mem = (char*) malloc(4);
	MC_MEM(buf->mem, 4);
	buf->used = 0;
	buf->size = 4;
}
inline void SetBuf(mc_buf *buf, size_t size) {
	buf->mem = (char*) malloc(size);
	MC_MEM(buf->mem, size);
	buf->used = 0;
	buf->size = size;
}
 
inline void ClearBuf(mc_buf *buf) {
	buf->used = 0;
}

inline void ResetBuf(mc_buf *buf) {
	void* p = realloc(buf->mem, 4);
	MC_MEM(p, 4);
	buf->mem = ( char*) p;
	buf->used = 0;
	buf->size = 4;
}

inline void MatchBuf(mc_buf *buf, size_t required) {
	size_t newsize = buf->size;
	if (required > buf->size) {
		if (newsize == 0) return; //error condition
		//while (newsize < required) newsize *= 2;
		newsize = required;
		void* p = realloc(buf->mem, newsize);
		MC_MEM(p, newsize);		
		buf->mem = (char*) p;
		MC_MEM(buf->mem, newsize);
		buf->size = newsize;
		//memset(&buf->mem[buf->used], 0, required-buf->used);
	}
}

inline void FreeBuf(mc_buf *buf) {
	free(buf->mem);
	buf->size = 0;
	buf->used = 0;
	buf->mem = NULL;
}

/* used for crypt */
struct mc_crypt_ctx {
	mc_sync_ctx *ctx;
	EVP_CIPHER_CTX *evp;
	mc_file *f;
	unsigned char iv[16];
	bool hasiv;
	unsigned char tag[16];
	bool hastag;
	mc_buf pbuf; //persistent buffer to avoid mallocs
};
inline void init_crypt_ctx(mc_crypt_ctx *cctx, mc_sync_ctx *ctx)
{
	cctx->hasiv = false;
	cctx->hastag = false; 
	cctx->f = NULL; 
	cctx->ctx = ctx; 
	cctx->evp = NULL;
	cctx->pbuf.size = 0; 
	cctx->pbuf.mem = NULL;
}
inline void init_crypt_ctx_copy(mc_crypt_ctx *cctx, mc_crypt_ctx *extcctx)
{
	cctx->hasiv = extcctx->hasiv;
	cctx->hastag = extcctx->hastag; 
	cctx->f = extcctx->f; 
	cctx->ctx = extcctx->ctx;  
	cctx->evp = NULL;
	cctx->pbuf.size = 0; 
	cctx->pbuf.mem = NULL;
}

#endif /* MC_TYPES_H */