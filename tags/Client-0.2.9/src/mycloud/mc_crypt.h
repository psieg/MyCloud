#ifndef MC_CRYPT_H
#define MC_CRYPT_H
#include "mc.h"
#include "mc_types.h"
#include "openssl/evp.h"
#include "openssl/err.h"
//Crypt is a partial overlay to
#include "mc_fs.h"
#include "mc_srv.h"

#define MC_CRYPT_KEYSIZE	32	// 256bits Manually used in types.h!
#define MC_CRYPT_IDENTIFIER			"MCC\x01"	//MCC-1	//Format: 4ID+12IV+DATA+16TAG
#define MC_CRYPT_IDOFFSET	4
#define MC_CRYPT_IVSIZE		12
#define MC_CRYPT_OFFSET		16	// 4ID + 12IV
#define MC_CRYPT_PADDING	16	// Auth-TAG
#define MC_CRYPT_SIZEOVERHEAD 32
#define MC_CRYPT_BLOCKSIZE	16	//128 for aes


#define MC_CRYPTNAME_IDENTIFIER	"MCN\x01"	//MCN-1 //Format: 4ID+DATA+16TAG, IV is implicit (first 12 byte of MD5 of path)
#define MC_CRYPTNAME_IDOFFSET	4
#define MC_CRYPTNAME_IVSIZE		12
#define MC_CRYPTNAME_OFFSET		4
#define MC_CRYPTNAME_PADDING	16
#define MC_CRYPTNAME_SIZEOVERHEAD 20

// Generate a random Key (for a new sync)
int crypt_randkey(unsigned char key[MC_CRYPT_KEYSIZE]);
// Feed the crypto RNG
void crypt_seed(const string& data);
void crypt_seed(int64 data);

// Translate Metadata to/from cryptformat
// Encrypts rule, can't be used locally this way!
int crypt_filter_tosrv(mc_sync_ctx *ctx, const string& syncname, mc_filter *f);
// Translates rule
int crypt_filter_fromsrv(mc_sync_ctx *ctx, const string& syncname, mc_filter *f); //TODO: private because implicit?
// Translate a filter listing in-place
int crypt_filterlist_fromsrv(mc_sync_ctx *ctx, const string& syncname, list<mc_filter> *l);
// Does not translate size
int crypt_file_tosrv(mc_sync_ctx *ctx, const string& path, mc_file *f);
// Also translates size, as locally only the unencrypted size counts, or sizetranslate is used
int crypt_file_fromsrv(mc_sync_ctx *ctx, const string& path, mc_file *f); //TODO: private because implicit?
// Translate a directory listing in-place
int crypt_filelist_fromsrv(mc_sync_ctx *ctx, const string& path, list<mc_file> *l);


// Filestring differs because of the size translation
void crypt_filestring(mc_sync_ctx *ctx, mc_file *f, string *s);

// Calculate MD5 for the file wether crypted or not
int crypt_filemd5_new(mc_crypt_ctx *cctx, unsigned char hash[16], const string& fpath, size_t fsize);
int crypt_filemd5_new(mc_crypt_ctx *cctx, unsigned char hash[16], const string& fpath, size_t fsize, FILE *fdesc);
int crypt_filemd5_known(mc_crypt_ctx *cctx, mc_file *file, unsigned char hash[16], const string& fpath);
int crypt_filemd5_known(mc_crypt_ctx *cctx, mc_file *file, unsigned char hash[16], const string& fpath, FILE *fdesc);

// Retreive and translate addrs
int crypt_init_download(mc_crypt_ctx *cctx, mc_file *file);
int crypt_initresume_down(mc_crypt_ctx *cctx, mc_file *file);
int crypt_resumetopos_down(mc_crypt_ctx *cctx, mc_file *file, int64 offset, FILE *fdesc);
int crypt_getfile(mc_crypt_ctx *cctx, int id, int64 offset, int64 blocksize, FILE *fdesc, int64 *byteswritten, unsigned char hash[16]);
int crypt_finish_download(mc_crypt_ctx *cctx);

// Modify and send, if new, encrypt name
int crypt_init_upload(mc_crypt_ctx *cctx, mc_file *file);
int crypt_initresume_up(mc_crypt_ctx *cctx, mc_file *file, int64 *offset);
int crypt_resumetopos_up(mc_crypt_ctx *cctx, mc_file *file, int64 offset, FILE *fdesc);
int crypt_putfile(mc_crypt_ctx *cctx, const string& path, mc_file *file, int64 blocksize, FILE *fdesc);
int crypt_addfile(mc_crypt_ctx *cctx, int id, int64 offset, int64 blocksize, FILE *fdesc, unsigned char hash[16]);
int crypt_patchfile(mc_crypt_ctx *cctx, const string& path, mc_file *file);
int crypt_finish_upload(mc_crypt_ctx *cctx);


#endif /* MC_CRYPT_H */