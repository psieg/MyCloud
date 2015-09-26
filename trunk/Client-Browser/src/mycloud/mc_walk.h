#ifndef MC_WALK_H
#define MC_WALK_H
#include "mc.h"
#include "mc_fs.h"
#include "mc_db.h"
#include "mc_srv.h"
#include "mc_transfer.h"
#include "mc_filter.h"
#include "mc_crypt.h"
#include "mc_util.h"
#include <regex>

/* The main sync functions. Walk and others decide what to do with what file */


/* Walk through the files in this directory and sync them */
int walk(mc_sync_ctx *ctx, string path, int id, unsigned char hash[16]);
/* The server has not been updated, thus db == srv */
int walk_nochange(mc_sync_ctx *ctx, string path, int id, unsigned char hash[16]);
/* Files have been deleted locally, when downloading make sure to restore things properly */
int walk_nolocal(mc_sync_ctx *ctx, string path, int id, unsigned char hash[16]);
/* Files have been deleted remotely, when uploading make sure to restore things properly */
int walk_noremote(mc_sync_ctx *ctx, string path, int id, unsigned char hash[16]);

#endif /* MC_WALK_H */