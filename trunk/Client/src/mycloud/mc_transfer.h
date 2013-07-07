#ifndef MC_TRANSFER_H
#define MC_TRANSFER_H
#include "mc.h"
#include "mc_fs.h"
#include "mc_db.h"
#include "mc_srv.h"
#include "mc_crypt.h"
#include "mc_walk.h"
#include "mc_filter.h"
using namespace std;

/* The actions taken for a certain file. What to do is decided by walk.
*	Note they may modify the mc_file(_fs) structs	*/

/* Download the file from the server */
int download(mc_sync_ctx *ctx, const string& path, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr, bool recursive = true, mc_crypt_ctx *extcctx = NULL);
/* Upload the file to the server */
int upload(mc_sync_ctx *ctx, const string& path, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr, int parent = 0, bool recursive = true, mc_crypt_ctx *extcctx = NULL);
/* Purge the file from the entire system, ignoring fs - USE WITH CAUTION */
int purge(mc_file *db, mc_file *srv);
/* Resumes an interrupted upload */
int complete_up(mc_sync_ctx *ctx, const string& fpath, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr);
/* Resumes an interrupted download */
int complete_down(mc_sync_ctx *ctx, const string& fpath, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr);


#endif /* MC_TRANSFER_H */