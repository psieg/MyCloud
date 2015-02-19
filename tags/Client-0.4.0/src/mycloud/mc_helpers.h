#ifndef MC_HELPERS_H
#define MC_HELPERS_H
#include "mc.h"
#include "mc_types.h"
#include "mc_util.h"
#include "mc_srv.h"
#include "mc_db.h"
#include "mc_crypt.h"

//High-Level helpers (using db and srv)

/* cascade up the tree and recalc hashes from db */
int updateHash(mc_sync_ctx *ctx, mc_file *f, mc_sync_db *s);

/* if the mtime is not as db says, touch it */
int updateMtimeIfMismatch(string fpath, mc_file* db);

/* retrieve and compare sync hashes */
int fullsync();
int fullsync(list<mc_sync_db> *dbsyncs);

/* panic action when decryption failed */
int cryptopanic();

#endif /* MC_HELPERS_H */