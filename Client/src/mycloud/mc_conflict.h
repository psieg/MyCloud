#ifndef MC_CONFLICT_H
#define MC_CONFLICT_H

#include "mc.h"
#include "mc_types.h"

/* These functions are called when a conflict is found, to be called only by mc_walk */


/* Ask the user which of the conflicting versions he wants to keep, recommend doubtaction
*	db may be NULL	*/
int conflicted(mc_sync_ctx *ctx, const std::string& path, mc_file_fs *fs, mc_file *db, mc_file *srv, std::string *hashstr, MC_CONFLICTRECOMMENDATION doubtaction);
/* Ask ...
*	db may be null	*/
int conflicted_nolocal(mc_sync_ctx *ctx, const std::string& path, mc_file *db, mc_file *srv, std::string *hashstr);

/* Ask ...
*	srv must be NULL or srv->status must be deleted	*/
int conflicted_noremote(mc_sync_ctx *ctx, const std::string& path, mc_file_fs *fs, mc_file *db, mc_file *srv, std::string *hashstr);


#endif