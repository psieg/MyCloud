#include "mc_walk.h"
#ifdef MC_QTCLIENT
#	include "qtClient.h"
#else
#	include "mc_workerthread.h"
#endif

#include "mc_conflict.h"

/* Helper: catches the case when a file was purged but then restored with a new ID 
*	this case may apply whenever db and srv are known */
int checkmissedpurge(mc_file *db, mc_file *srv){
	int rc;
	if(db->id < srv->id){ //this means the file was purged (which this client missed) and then restored
		MC_DBG("Missed purge detected");
		//solution: purge and restore with new id
		rc = purge(db,NULL);
		MC_CHKERR(rc);
		db->id = srv->id;
		rc = db_insert_file(db);
		MC_CHKERR(rc);
	}
	return 0;
}




/* Verify the files match (react if not) and make sure the same data is on all stores
*	Assumes the mtime is equal on all files.
*	db may be NULL	*/
int verifyandcomplete(mc_sync_ctx *ctx, const string& path, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr){
	unsigned char orighash[16];
	string spath,fpath;
	int rc;
	bool terminating = false;
	mc_file_fs mtimedummy;
	
	spath.assign(path).append(srv->name);
	fpath.assign(ctx->sync->path).append(spath);
	MC_DBG("Verifying file " << srv->id << ": " << printname(srv));
	//mc_sleepms(10);

	if(db == NULL){
		if(fs->is_dir != srv->is_dir) 
			return conflicted(ctx,path,fs,db,srv,hashstr,MC_CONFLICTREC_DONTKNOW);

		//It's a new file		

		if((fs->size == srv->size)){ //Same file (?)
			if(srv->is_dir) memset(srv->hash,0,16); //Server (directory)hashes are never saved
			if(srv->status == MC_FILESTAT_COMPLETE){
				//TODO: Maybe hash before assuming match? On the other hand, mtime is exactly equal...
				rc = db_insert_file(srv);
				MC_CHKERR(rc);

			} else if(srv->status == MC_FILESTAT_INCOMPLETE_UP){
				srv->status = MC_FILESTAT_INCOMPLETE_UP_ME;
				rc = db_insert_file(srv);
				MC_CHKERR(rc);


				return complete_up(ctx,path,fpath,fs,srv,srv,hashstr);
			} else { //srv->status == MC_FILESTAT_DELETED
				return conflicted(ctx,path,fs,NULL,srv,hashstr,MC_CONFLICTREC_DONTKNOW);
			}
		} else {
			return conflicted(ctx,path,fs,NULL,srv,hashstr,MC_CONFLICTREC_DONTKNOW);
		}
	} else {
		if(fs == NULL){
			if(srv->is_dir != db->is_dir)
				return conflicted(ctx,path,fs,db,srv,hashstr,MC_CONFLICTREC_DONTKNOW);
			if(db->status == MC_FILESTAT_INCOMPLETE_UP){
				return download(ctx,path,fs,db,srv,hashstr);
			} else if(srv->status != MC_FILESTAT_DELETED)
				return upload(ctx,path,fs,db,srv,hashstr);
			else if(db->status != MC_FILESTAT_DELETED)
				return download(ctx,path,fs,db,srv,hashstr);
			else if(db->size == srv->size && memcmp(db->hash,srv->hash,16) == 0){
				if(db->mtime ==  srv->mtime)
					{} //Nothing to do
				else if(db->mtime > srv->mtime)
					return upload(ctx,path,fs,db,srv,hashstr);
				else //db->mtime < srv->mtime
					return download(ctx,path,fs,db,srv,hashstr);
			} else
				return conflicted_nolocal(ctx,path,db,srv,hashstr);
		} else {
			if((fs->is_dir != srv->is_dir) || (srv->is_dir != db->is_dir))
				return conflicted(ctx,path,fs,db,srv,hashstr,MC_CONFLICTREC_DONTKNOW);
			if(db->status == MC_FILESTAT_COMPLETE){
				if(srv->status == MC_FILESTAT_COMPLETE){
					if(db->size == srv->size && srv->size == fs->size && memcmp(db->hash,srv->hash,16) == 0){
						if(fs->mtime == db->mtime && db->mtime ==  srv->mtime)
							if(db->name.compare(srv->name)) //case changed remotely
								return download(ctx,path,fs,db,srv,hashstr);
							else if(fs->name.compare(db->name)) //case changed locally
								return upload(ctx,path,fs,db,srv,hashstr);
							else
								{} //Nothing to do
						else if(fs->mtime > db->mtime || db->mtime > srv->mtime)
							return upload(ctx,path,fs,db,srv,hashstr);
						else //fs->mtime < db->mtime || db->mtime < srv->mtime
							return download(ctx,path,fs,db,srv,hashstr);
					} else
						return conflicted(ctx,path,fs,db,srv,hashstr,MC_CONFLICTREC_DONTKNOW);
				} else if(srv->status == MC_FILESTAT_INCOMPLETE_UP){
					//Shouldn't actually happen: The file was complete but is incomplete now, although its the same file?
					//Continue upload
					return complete_up(ctx,path,fpath,fs,srv,srv,hashstr);
				} else { //srv->status == MC_FILESTAT_DELETED
					return conflicted(ctx,path,fs,db,srv,hashstr,MC_CONFLICTREC_DONTKNOW);
				}
			} else if(db->status == MC_FILESTAT_DELETED){
				//Woha, this really shoudn't happen
				//Deleted at the same time it was (re)added
				if(srv->status == MC_FILESTAT_DELETED){
					return conflicted(ctx,path,fs,db,srv,hashstr,MC_CONFLICTREC_DOWN);
				} else { //complete or incomplete_up
					return conflicted(ctx,path,fs,db,srv,hashstr,MC_CONFLICTREC_UP);
				}
			} else if(db->status == MC_FILESTAT_INCOMPLETE_DOWN){
				if(srv->status == MC_FILESTAT_COMPLETE){
					//Continue download
					return complete_down(ctx,path,fpath,fs,db,srv,hashstr);
				} else if(srv->status == MC_FILESTAT_INCOMPLETE_UP) {
					//Since we don't start downloading incomplete uploads, this shouldn't happen either
					MC_INFL("Not downloading file " << srv->id << ": " << srv->name << ", file is not complete");
					return MC_ERR_INCOMPLETE_SKIP;
				} else { //srv->status == MC_FILESTAT_DELETED
					return conflicted(ctx,path,fs,db,srv,hashstr,MC_CONFLICTREC_DONTKNOW); 
				}
			} else { //db->status == MC_FILESTAT_INCOMPLETE_UP or MC_FILESTAT_INCOMPLETE_UP_ME
				if(srv->status == MC_FILESTAT_COMPLETE){
					//Someone else finished the upload
					db->status = MC_FILESTAT_COMPLETE;
					rc = db_update_file(srv);
					MC_CHKERR(rc);
				} else if(srv->status == MC_FILESTAT_INCOMPLETE_UP){
					//Continue upload
					return complete_up(ctx,path,fpath,fs,db,srv,hashstr);
				} else {  //srv->status == MC_FILESTAT_DELETED
					return conflicted(ctx,path,fs,db,srv,hashstr,MC_CONFLICTREC_DONTKNOW);
				}
			}
		}
	}
	//Execution reaches this point if nothing was to be done, thus we check the children
	if(srv->is_dir){
		if(db) memcpy(orighash,db->hash,16); else memcpy(orighash,srv->hash,16); //This catches the "EVIL" case from nochange where srv and db point to the same data
		if(db && memcmp(db->hash,srv->hash,16) == 0 && memcmp(db->hash,"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",16) != 0){
			if(srv->status != MC_FILESTAT_DELETED) rc = walk_nochange(ctx,spath,srv->id,srv->hash);
			else rc = 0; }
		else if(fs && fs->is_dir) rc = walk(ctx,spath,srv->id,srv->hash);
		else rc = walk_nolocal(ctx,spath,srv->id,srv->hash);
		if(rc == MC_ERR_TERMINATING) terminating = true;
		if(MC_IS_CRITICAL_ERR(rc)) return rc;
		//Hashupdate
		if(!db || memcmp(orighash,srv->hash,16)){
			rc = db_update_file(srv);
			MC_CHKERR(rc);
		}
		//While this sometimes overwrites legit changes, generally it undoes unwanted mtime changes from
		//things we did within the directory
		if(srv->status != MC_FILESTAT_DELETED){
			rc = fs_filestats(&mtimedummy,fpath,srv->name);
			MC_CHKERR(rc);
			if(mtimedummy.mtime != srv->mtime){
				rc = fs_touch(fpath,srv->mtime);
				MC_CHKERR(rc);
			}
		}
	}

	crypt_filestring(ctx,srv,hashstr);

	if(terminating) return MC_ERR_TERMINATING;
	else return 0;
}

/* The actual walkers, made for different preconditions */

/* Go through the files in this dir and decide what to do */
int walk(mc_sync_ctx *ctx, string path, int id, unsigned char hash[16]){
	int rc;
	bool clean = true;
	list<mc_file_fs> onfs;
	list<mc_file_fs>::iterator onfsit,onfsend;
	list<mc_file> indb;
	list<mc_file>::iterator indbit,indbend;
	list<mc_file> onsrv;
	list<mc_file>::iterator onsrvit,onsrvend;
	string hashstr = "";
	list<string> hashes;
	list<string>::iterator hashesit,hashesend;
	MC_CONFLICTACTION outerconflictact = ctx->dirconflictact; //must be saved in case upper layers used it...
	string fpath = ctx->sync->path;
	if(path.size()) path.append("/");
	fpath.append(path);
	MC_DBG("Walking directory " << id << ": " << path);
	//MC_NOTIFYSTART(MC_NT_WALKTEST,path);
	MC_CHECKTERMINATING();

	rc = fs_listdir(&onfs, fpath);
	MC_CHKERR(rc);

	rc = db_list_file_parent(&indb,id);
	MC_CHKERR(rc);

	rc  = srv_listdir(&onsrv,id);
	MC_CHKERR(rc);

	rc = crypt_filelist_fromsrv(ctx,path,&onsrv);
	MC_CHKERR(rc);

	onfs.sort(compare_mc_file_fs);
	indb.sort(compare_mc_file);
	onsrv.sort(compare_mc_file);

	rc = filter_lists(path,&onfs, &indb, &onsrv, ctx->filter);
	MC_CHKERR(rc);

	onfsit = onfs.begin();
	onfsend = onfs.end();
	indbit = indb.begin();
	indbend = indb.end();
	onsrvit = onsrv.begin();
	onsrvend = onsrv.end();

	if(!ctx->rconflictact) ctx->dirconflictact = MC_CONFLICTACT_UNKNOWN;
	while((onfsit != onfsend) || (indbit != indbend) || (onsrvit != onsrvend)){
		if(indbit == indbend){
			if(onfsit == onfsend){
				// New file on server
				rc = download(ctx,path,NULL,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onsrvit;
			} else if((onsrvit == onsrvend) || (onsrvit->name > onfsit->name)){
				// New file in fs
				rc = upload(ctx,path,&*onfsit,NULL,NULL,&hashstr,id);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onfsit;
			} else if(onsrvit->name < onfsit->name){
				// New file on server
				rc = download(ctx,path,NULL,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onsrvit;
			} else { // onsrvit->name == onfsit->name
				// File added on srv and fs
				if(onfsit->mtime == onsrvit->mtime){
					rc = verifyandcomplete(ctx,path,&*onfsit,NULL,&*onsrvit,&hashstr);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				} else if(onfsit->mtime > onsrvit->mtime){
					rc = conflicted(ctx,path,&*onfsit,NULL,&*onsrvit,&hashstr,MC_CONFLICTREC_UP);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				} else { // onfsit->mtime < onsrvit->mtime
					rc = conflicted(ctx,path,&*onfsit,NULL,&*onsrvit,&hashstr,MC_CONFLICTREC_DOWN);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				}
				++onsrvit;
				++onfsit;
			}
		} else if((onfsit == onfsend) || onfsit->name > indbit->name){
			if((onsrvit == onsrvend) || (onsrvit->name > indbit->name)){
				// Lonely db entry
				// Should not happen (known to db = known to srv at last sync)
				// Since deleted files are still known to srv, this should not happen
				// Assumption: File has been deleted on fs while not known to srv (client must have been killed during sync)
				// We drop this entry
				//rc = upload(ctx,path,NULL,&*indbit,NULL);
				rc = db_delete_file(indbit->id);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++indbit;
			} else if(onsrvit->name == indbit->name){
				rc = checkmissedpurge(&*indbit,&*onsrvit);
				MC_CHKERR(rc);
				// File has been deleted or is incomplete up
				if(onsrvit->mtime == indbit->mtime){ //on incomplete up timestamps match
					rc = verifyandcomplete(ctx,path,NULL,&*indbit,&*onsrvit,&hashstr);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				} else if(onsrvit->mtime > indbit->mtime){
					rc = download(ctx,path,NULL,&*indbit,&*onsrvit,&hashstr);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				} else { // onsrvit->mtime < indbit->mtime
					rc = upload(ctx,path,NULL,&*indbit,&*onsrvit,&hashstr);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				}
				++indbit;
				++onsrvit;
			} else { // onsrvit->name < indbit->name
				// New file on server
				rc = download(ctx,path,NULL,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onsrvit;
			}
		} else if(onfsit->name == indbit->name){
			if((onsrvit == onsrvend) || (onsrvit->name > onfsit->name)){
				// Should not happen (known to db = known to srv at last sync)
				// Since deleted files are still known to srv, this should not happen
				// Assumption: Added (client must have been killed during sync)
				rc = upload(ctx,path,&*onfsit,&*indbit,NULL,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onfsit;
				++indbit;
			} else if(onsrvit->name == onfsit->name){
				// Standard: File known to all, check modified
				rc = checkmissedpurge(&*indbit,&*onsrvit);
				MC_CHKERR(rc);
				if(onfsit->mtime == indbit->mtime){
					if(indbit->mtime == onsrvit->mtime){
						rc = verifyandcomplete(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
						if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
					} else if (indbit->mtime < onsrvit->mtime){
						rc = download(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
						if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
					} else { // indbit->mtime > onsrvit->mtime
						// Should not happen, client must have been killed while syncing this change
						rc = upload(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
						if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
					}
				} else if(onfsit->mtime > indbit->mtime){
					if(indbit->mtime >= onsrvit->mtime){
						// Newer should not happen, client must have been killed while syncing this change
						if(indbit->status == MC_FILESTAT_INCOMPLETE_DOWN){
							string spath;
							spath.assign(fpath).append(onsrvit->name);
							rc = complete_down(ctx,path,spath,&*onfsit,&*indbit,&*onsrvit,&hashstr);
						} else {
							rc = upload(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
						}
						if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
					} else { // indbit->mtime < onsrvit->mtime
						if(onfsit->mtime > onsrvit->mtime){
							rc = conflicted(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr,MC_CONFLICTREC_UP);
							if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
						} else if(onfsit->mtime == onsrvit->mtime){
							rc = verifyandcomplete(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
							if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
						} else { // onfsit->mtime < onsrvit->mtime
							rc = conflicted(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr,MC_CONFLICTREC_DOWN);
							if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
						}
					}
				} else { // onfsit->mtime < indbit->mtime
					// Should not happen, when a file is registered to the db, its mtime is registered
					// as mtime normally only grows, the file must have been replaced
					if(indbit->mtime > onsrvit->mtime){
						// This really really shouldn't happen. DB newer than FS and SRV is impossible during normal operation
						// Assumption: Server has been restored from backup, thus we want to rc = upload new changes
						if(onfsit->mtime > onsrvit->mtime){
							rc = upload(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
							if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
						} else if(onfsit->mtime == onsrvit->mtime){
							rc = verifyandcomplete(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
							if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
						} else { // onfsit->mtime < onsrvit->mtime
							rc = upload(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
							if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
						}
					} else if(indbit->mtime == onsrvit->mtime){
						rc = upload(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
						if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
					} else { // indbit->mtime < onsrvit->mtime
						rc = conflicted(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr,MC_CONFLICTREC_DOWN);
						if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
					}
						
				}
				++onfsit;
				++indbit;
				++onsrvit;
			} else { // onsrvit->name < onfsit->name
				// New file on server
				rc = download(ctx,path,NULL,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onsrvit;
			}
		} else { // onfsit->name < indbit->name
			if((onsrvit == onsrvend) || (onsrvit->name > onfsit->name)){
				// New file in fs
				rc = upload(ctx,path,&*onfsit,NULL,NULL,&hashstr,id);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onfsit;
			} else if(onsrvit->name == onfsit->name){
				// File added on srv and fs
				if(onfsit->mtime == onsrvit->mtime){
					rc = verifyandcomplete(ctx,path,&*onfsit,NULL,&*onsrvit,&hashstr);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				} else if(onfsit->mtime > onsrvit->mtime){
					rc = conflicted(ctx,path,&*onfsit,NULL,&*onsrvit,&hashstr,MC_CONFLICTREC_UP);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				} else { // onfsit->mtime < onsrvit->mtime
					rc = conflicted(ctx,path,&*onfsit,NULL,&*onsrvit,&hashstr,MC_CONFLICTREC_DOWN);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				}
				++onsrvit;
				++onfsit;
			} else { // onsrvit->name < onfsit->name
				// New file on server
				rc = download(ctx,path,NULL,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onsrvit;
			}
		}
		hashes.push_back(hashstr);
		hashstr = "";
	}
	ctx->dirconflictact = outerconflictact;

	hashes.sort(compare_hashstr);
	hashesit = hashes.begin();
	hashesend = hashes.end();
	while(hashesit != hashesend){
		hashstr.append(*hashesit);
		++hashesit;
	}
	rc = strmd5(hash,hashstr);
	MC_CHKERR(rc);
	MC_DBG("Hash of " << id << ": " << path << " is " << MD5BinToHex(hash));
	MC_DBGL("Hashdata is " << BinToHex(hashstr));
	if(!clean) return MC_ERR_UNCLEAN_WALK;
	return 0;
}

/* The server has not been updated, thus db == srv 
*	This is a cut-down normal walk	

	EVIL! Generally, we pass the dbit as srvit, this might have unexpected side effects!
	Currently none known or known fixed */
int walk_nochange(mc_sync_ctx *ctx, string path, int id, unsigned char hash[16]){
	int rc;
	bool clean = true;
	list<mc_file_fs> onfs;
	list<mc_file_fs>::iterator onfsit,onfsend;
	list<mc_file> indb;
	list<mc_file>::iterator indbit,indbend;
	list <mc_file> srvdummy;
	mc_file srv;
	string hashstr = "";
	list<string> hashes;
	list<string>::iterator hashesit,hashesend;
	MC_CONFLICTACTION outerconflictact = ctx->dirconflictact; //must be saved in case upper layers used it...
	string fpath = ctx->sync->path;
	if(path.size()) path.append("/");
	fpath.append(path);
	MC_DBG("Walking directory " << id << ": " << path);
	//MC_NOTIFYSTART(MC_NT_WALKTEST,path);
	MC_CHECKTERMINATING();

	rc = fs_listdir(&onfs,fpath);
	MC_CHKERR(rc);

	rc = db_list_file_parent(&indb,id);
	MC_CHKERR(rc);

	onfs.sort(compare_mc_file_fs);
	indb.sort(compare_mc_file);

	srvdummy.assign(indb.begin(),indb.end());
	
	rc = filter_lists(path,&onfs,&indb,&srvdummy,ctx->filter);
	MC_CHKERR(rc);

	onfsit = onfs.begin();
	onfsend = onfs.end();
	indbit = indb.begin();
	indbend = indb.end();


	if(!ctx->rconflictact) ctx->dirconflictact = MC_CONFLICTACT_UNKNOWN;
	while((onfsit != onfsend) || (indbit != indbend)){
		if(indbit == indbend){
			// New file in fs
			rc = upload(ctx,path,&*onfsit,NULL,NULL,&hashstr,id);
			if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			++onfsit;
		} else if((onfsit == onfsend) || onfsit->name > indbit->name){
			// File has been deleted or is incomplete up
			rc = verifyandcomplete(ctx,path,NULL,&*indbit,&*indbit,&hashstr);
			if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			++indbit;
		} else if(onfsit->name == indbit->name){
			// Standard: File known to all, check modified
			//we don't need to check for restored as db=srv by definition
			if(indbit->status == MC_FILESTAT_INCOMPLETE_DOWN){
				rc = srv_getmeta(indbit->id,&srv); //actually just because we need to have srv->status == COMPLETE (or, what shouldn't be INCOMPLETE_UP)
				MC_CHKERR(rc);
				rc = crypt_file_fromsrv(ctx,path,&srv);
				MC_CHKERR(rc);
				rc = verifyandcomplete(ctx,path,&*onfsit,&*indbit,&srv,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			} else if(onfsit->mtime == indbit->mtime){
				rc = verifyandcomplete(ctx,path,&*onfsit,&*indbit,&*indbit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			} else if(onfsit->mtime > indbit->mtime){
				rc = upload(ctx,path,&*onfsit,&*indbit,&*indbit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			} else { // onfsit->mtime < indbit->mtime
				// Should not happen, when a file is registered to the db, its mtime is registered
				// as mtime normally only grows, the file must have been replaced
				rc = upload(ctx,path,&*onfsit,&*indbit,&*indbit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
					
			}
			++onfsit;
			++indbit;
		} else { // onfsit->name < indbit->name
			// New file in fs
			rc = upload(ctx,path,&*onfsit,NULL,NULL,&hashstr,id);
			if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			++onfsit;
		}
		hashes.push_back(hashstr);
		hashstr = "";
	}
	ctx->dirconflictact = outerconflictact;

	hashes.sort(compare_hashstr);
	hashesit = hashes.begin();
	hashesend = hashes.end();
	while(hashesit != hashesend){
		hashstr.append(*hashesit);
		++hashesit;
	}
	rc = strmd5(hash,hashstr);
	MC_CHKERR(rc);
	MC_DBG("Hash of " << id << ": " << path << " is " << MD5BinToHex(hash));
	MC_DBGL("Hashdata is " << BinToHex(hashstr));
	if(!clean) return MC_ERR_UNCLEAN_WALK;
	return 0;
}

/* Files have been deleted locally, when rc = downloading make sure to restore things properly */
int walk_nolocal(mc_sync_ctx *ctx, string path, int id, unsigned char hash[16]){
	int rc;
	bool clean = true;
	list<mc_file> indb;
	list<mc_file>::iterator indbit,indbend;
	list<mc_file> onsrv;
	list<mc_file>::iterator onsrvit,onsrvend;
	string hashstr = "";
	list<string> hashes;
	list<string>::iterator hashesit,hashesend;
	MC_CONFLICTACTION outerconflictact = ctx->dirconflictact; //must be saved in case upper layers used it...
	if(path.size()) path.append("/");
	MC_DBG("Walking directory " << id << ": " << path);
	//MC_NOTIFYSTART(MC_NT_WALKTEST,path);
	MC_CHECKTERMINATING();

	rc = db_list_file_parent(&indb,id);
	MC_CHKERR(rc);

	rc  = srv_listdir(&onsrv,id);
	MC_CHKERR(rc);

	rc = crypt_filelist_fromsrv(ctx,path,&onsrv);
	MC_CHKERR(rc);

	indb.sort(compare_mc_file);
	onsrv.sort(compare_mc_file);

	rc = filter_lists(path,NULL,&indb,&onsrv,ctx->filter);
	MC_CHKERR(rc);

	indbit = indb.begin();
	indbend = indb.end();
	onsrvit = onsrv.begin();
	onsrvend = onsrv.end();

	if(!ctx->rconflictact) ctx->dirconflictact = MC_CONFLICTACT_UNKNOWN;
	while((indbit != indbend) || (onsrvit != onsrvend)){
		if(indbit == indbend){
			//File on srv thats not in db
			if(onsrvit->status != MC_FILESTAT_DELETED){
				rc = conflicted_nolocal(ctx,path,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			} else {
				rc = download(ctx,path,NULL,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			}
			++onsrvit;
		} else if((onsrvit == onsrvend) || (onsrvit->name > indbit->name)){
			//File in db thats not on srv = lonely db entry (->walk), drop it
			rc = db_delete_file(indbit->id);
			if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			++indbit;
		} else if(indbit->name > onsrvit->name){
			//File on srv thats not in db
			if(onsrvit->status != MC_FILESTAT_DELETED){
				rc = conflicted_nolocal(ctx,path,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			} else {
				rc = download(ctx,path,NULL,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			}
			++onsrvit;
		} else { // indbit->name == onsrvit->name
			rc = checkmissedpurge(&*indbit,&*onsrvit);
			MC_CHKERR(rc);
			if(onsrvit->mtime != indbit->mtime){
				rc = conflicted_nolocal(ctx,path,&*indbit,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			} else {
				//rc = upload(ctx,path,NULL,&*indbit,&*onsrvit,&hashstr);
				rc = verifyandcomplete(ctx,path,NULL,&*indbit,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			}
			++indbit;
			++onsrvit;
		}
		hashes.push_back(hashstr);
		hashstr = "";
	}
	ctx->dirconflictact = outerconflictact;

	hashes.sort(compare_hashstr);
	hashesit = hashes.begin();
	hashesend = hashes.end();
	while(hashesit != hashesend){
		hashstr.append(*hashesit);
		++hashesit;
	}
	rc = strmd5(hash,hashstr);
	MC_CHKERR(rc);
	MC_DBG("Hash of " << id << ": " << path << " is " << MD5BinToHex(hash));
	MC_DBGL("Hashdata is " << BinToHex(hashstr));
	if(!clean) return MC_ERR_UNCLEAN_WALK;
	return 0;
}


/* Files have been deleted remotely, when rc = uploading make sure to restore things properly */
int walk_noremote(mc_sync_ctx *ctx, string path, int id, unsigned char hash[16]){
	int rc;
	bool clean = true;
	list<mc_file_fs> onfs;
	list<mc_file_fs>::iterator onfsit,onfsend;
	list<mc_file> indb;
	list<mc_file>::iterator indbit,indbend;
	list<mc_file> onsrv;
	list<mc_file>::iterator onsrvit,onsrvend;
	string hashstr = "";
	list<string> hashes;
	list<string>::iterator hashesit,hashesend;
	MC_CONFLICTACTION outerconflictact = ctx->dirconflictact; //must be saved in case upper layers used it...
	string fpath = ctx->sync->path;
	if(path.size()) path.append("/");
	fpath.append(path);
	MC_DBG("Walking directory " << id << ": " << path);
	//MC_NOTIFYSTART(MC_NT_WALKTEST,path);
	MC_CHECKTERMINATING();

	rc = fs_listdir(&onfs,fpath);
	MC_CHKERR(rc);

	rc = db_list_file_parent(&indb,id);
	MC_CHKERR(rc);

	rc  = srv_listdir(&onsrv,id);
	MC_CHKERR(rc);

	rc = crypt_filelist_fromsrv(ctx,path,&onsrv);
	MC_CHKERR(rc);

	onfs.sort(compare_mc_file_fs);
	indb.sort(compare_mc_file);
	onsrv.sort(compare_mc_file);
	
	rc = filter_lists(path,&onfs,&indb,NULL,ctx->filter);
	MC_CHKERR(rc);

	onfsit = onfs.begin();
	onfsend = onfs.end();
	indbit = indb.begin();
	indbend = indb.end();
	onsrvit = onsrv.begin();
	onsrvend = onsrv.end();

	if(!ctx->rconflictact) ctx->dirconflictact = MC_CONFLICTACT_UNKNOWN;
	while((onfsit != onfsend) || (indbit != indbend) || (onsrvit != onsrvend)){
		if(indbit == indbend){
			if(onfsit == onfsend){
				// New file on server
				// Since its deleted we load it
				rc = download(ctx,path,NULL,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onsrvit;
			} else if((onsrvit == onsrvend) ||(onsrvit->name > onfsit->name)){
				// New file in fs
				rc = conflicted_noremote(ctx,path,&*onfsit,NULL,NULL,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onfsit;
			} else if(onsrvit->name < onfsit->name){
				// New file on server
				// Since its deleted we load it
				rc = download(ctx,path,NULL,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onsrvit;
			} else { // onsrvit->name == onfsit->name
				// File added on srv and fs
				// Since srv is deleted but fs exists newly
				if(onsrvit->mtime < onfsit->mtime){
					// Local file newer
					rc = conflicted_noremote(ctx,path,&*onfsit,NULL,&*onsrvit,&hashstr);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				} else { //onsrvit->mtime >= onfsit->mtime
					//donwload deleted
					rc = download(ctx,path,&*onfsit,NULL,&*onsrvit,&hashstr);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				}
				++onsrvit;
				++onfsit;
			}
		} else if((onfsit == onfsend) || onfsit->name > indbit->name){
			if((onsrvit == onsrvend) || (onsrvit->name > indbit->name)){
				// Lonely db entry
				rc = db_delete_file(indbit->id);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++indbit;
			} else if(onsrvit->name == indbit->name){
				rc = checkmissedpurge(&*indbit,&*onsrvit);
				MC_CHKERR(rc);
				// File has been deleted
				if(onsrvit->mtime < indbit->mtime){
					// Local deletion newer (another thing that shouldn't happen)
					rc = upload(ctx,path,NULL,&*indbit,&*onsrvit,&hashstr);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				} else { //onsrvit->mtime >= indbit->mtime){
					rc = download(ctx,path,NULL,&*indbit,&*onsrvit,&hashstr);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				}
				++indbit;
				++onsrvit;
			} else { // onsrvit->name < indbit->name
				// New file on server
				// Since its deleted we load it
				rc = download(ctx,path,NULL,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onsrvit;
			}
		} else if(onfsit->name == indbit->name){
			if((onsrvit == onsrvend) || (onsrvit->name > onfsit->name)){
				// Should not happen (known to db = known to srv at last sync)
				rc = conflicted_noremote(ctx,path,&*onfsit,&*indbit,NULL,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onfsit;
				++indbit;
			} else if(onsrvit->name == onfsit->name){
				// Standard: File known to all, check modified
				rc = checkmissedpurge(&*indbit,&*onsrvit);
				MC_CHKERR(rc);
				if((onfsit->mtime == indbit->mtime) || (indbit->status == MC_FILESTAT_INCOMPLETE_DOWN)){
					if(onsrvit->mtime < indbit->mtime){
						// Local newer
						// shouldn't happen either
						rc = conflicted_noremote(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
						if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
					} else { // onsrvit->mtime >= indbit->mtime
						rc = download(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
						if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
					}
				} else if(onfsit->mtime > indbit->mtime){
					if(onsrvit->mtime < onfsit->mtime){
						rc = conflicted_noremote(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
						if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
					} else { //onsrvit->mtime >= onfsit->mtime
						rc = download(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
						if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
					}
				} else { // onfsit->mtime < indbit->mtime
					// Should not happen, when a file is registered to the db, its mtime is registered
					// as mtime normally only grows, the file must have been replaced
					if(indbit->mtime > onsrvit->mtime){
						// This really really shouldn't happen. DB newer than FS and SRV is impossible during normal operation
						rc = conflicted_noremote(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
						if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
					} else { // indbit->mtime <= onsrvit->mtime
						rc = download(ctx,path,&*onfsit,&*indbit,&*onsrvit,&hashstr);
						if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
					}
						
				}
				++onfsit;
				++indbit;
				++onsrvit;
			} else { // onsrvit->name < onfsit->name
				// New file on server
				rc = download(ctx,path,NULL,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onsrvit;
			}
		} else { // onfsit->name < indbit->name
			if((onsrvit == onsrvend) || (onsrvit->name > onfsit->name)){
				// New file in fs
				rc = conflicted_noremote(ctx,path,&*onfsit,NULL,NULL,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onfsit;
			} else if(onsrvit->name == onfsit->name){
				// File added on srv and fs
				// Since srv is deleted but fs exists newly
				if(onsrvit->mtime < onfsit->mtime){
					// Local file newer
					rc = conflicted_noremote(ctx,path,&*onfsit,NULL,&*onsrvit,&hashstr);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				} else { //onsrvit->mtime >= onfsit->mtime
					//donwload deleted
					rc = download(ctx,path,&*onfsit,NULL,&*onsrvit,&hashstr);
					if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				}
				++onsrvit;
				++onfsit;
			} else { // onsrvit->name < onfsit->name
				// New file on server
				rc = download(ctx,path,NULL,NULL,&*onsrvit,&hashstr);
				if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
				++onsrvit;
			}
		}
		hashes.push_back(hashstr);
		hashstr = "";
	}
	ctx->dirconflictact = outerconflictact;

	hashes.sort(compare_hashstr);
	hashesit = hashes.begin();
	hashesend = hashes.end();
	while(hashesit != hashesend){
		hashstr.append(*hashesit);
		++hashesit;
	}
	rc = strmd5(hash,hashstr);
	MC_CHKERR(rc);
	MC_DBG("Hash of " << id << ": " << path << " is " << MD5BinToHex(hash));
	MC_DBGL("Hashdata is " << BinToHex(hashstr));
	if(!clean) return MC_ERR_UNCLEAN_WALK;
	return 0;
}