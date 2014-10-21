#include "mc_transfer.h"
#ifdef MC_QTCLIENT
#	include "qtClient.h"
#else
#	include "mc_workerthread.h"
#endif

/* The actions taken for a certain file. What to do is decided by walk.
*	Note they may modify the mc_file(_fs) structs	*/


/* Clear the directory of filtered files (as we are about to delete it)
	to be called by download only, and only when about to call rmdir
	this ignores db and srv, as ignored files shouldn't be there anyway	
	clearall MUST be false	*/
int cleardir(mc_sync_ctx *ctx, const string& path, bool clearall = false){
	list<mc_file_fs> onfs;
	string fpath;
	int rc;
	fpath.assign(ctx->sync->path).append(path).append("/");
	MC_DBG("Clearing directory " << path << " of filtered files");
	rc = fs_listdir(&onfs, fpath);
	MC_CHKERR(rc);
	for(mc_file_fs& f : onfs){
		if(clearall || match_full(fpath,&f,ctx->filter)){
			fpath.assign(path).append("/").append(f.name);
			if(f.is_dir){
				rc = cleardir(ctx,fpath,true);
				MC_CHKERR(rc);
				rc = fs_rmdir(fpath);
				MC_CHKERR(rc);
			} else {
				rc = fs_delfile(fpath);
				MC_CHKERR(rc);
			}
		}
	}
	return 0;
}

/* Check wether the db says the directory is empty of non-deleted files.
*	This is used as an indicator that it can be deleted/overriden	*/
int dirempty(int id, bool *empty){
	int rc;
	list<mc_file> l;
	rc = db_list_file_parent(&l,id);
	MC_CHKERR(rc);

	*empty = true;
	for(mc_file& f : l){
		if(f.status != MC_FILESTAT_DELETED){
			*empty = false;
			break;
		}
		// check the whole tree, directories marked as deleted may still be pending
		MC_CHKERR(dirempty(f.id, empty));
		if (!*empty) break;
	}
	return 0;
}

/* Download helpers	*/
int download_checkmodified(mc_crypt_ctx *cctx, const string& fpath, mc_file *srv, bool *modified){
	unsigned char hash[16];
	int rc;
				
	rc = crypt_filemd5_known(cctx,srv,hash,fpath);
	MC_CHKERR(rc);
	
	if(memcmp(srv->hash,hash,16) == 0){ //File not modified
		*modified = false;
		MC_DBG("File contents not modified");
	} else {
		*modified = true;
	}
	return 0;
}
int download_actual(mc_crypt_ctx *cctx, const string& fpath, mc_file *srv){
	int64 offset = 0, written = 0;
	FILE *fdesc = 0;
	int rc;
	MC_DBGL("Opening file " << fpath << " for writing");
	MC_NOTIFYSTART(MC_NT_DL,fpath);
	fdesc = fs_fopen(fpath,"wb");
	if(!fdesc) MC_CHKERR_MSG(MC_ERR_IO,"Could not (re)write the file");
	rc = crypt_init_download(cctx,srv);
	MC_CHKERR_FD_DOWN(rc,fdesc,cctx);
					
	while(offset < srv->size){
		MC_NOTIFYPROGRESS(offset,srv->size);
		MC_CHECKTERMINATING_FD_DOWN(fdesc,cctx);
		rc = crypt_getfile(cctx,srv->id,offset,MC_RECVBLOCKSIZE,fdesc,&written,srv->hash);
		MC_CHKERR_FD_DOWN(rc,fdesc,cctx);
		offset += written;
	}

	rc = crypt_finish_download(cctx);
	MC_CHKERR_FD(rc,fdesc);

	fs_fclose(fdesc);
	MC_NOTIFYEND(MC_NT_DL);
	return 0;
}
/* sub-downloads */
int download_deleted_dir(mc_sync_ctx *ctx, const string& fpath, const string& rpath, mc_file_fs *fs, mc_file *db, mc_file *srv, bool recursive, int* rrc){
	int rc;
	bool empty;

	if(!db) rc = db_insert_file(srv);
	else rc = db_update_file(srv);
	MC_CHKERR(rc);

	if(fs == NULL || !fs->is_dir){
		if(fs){ // then !fs->is_dir
			MC_DBG("Replacing a file with a deleted dir");
			rc = fs_delfile(fpath);
			MC_CHKERR(rc);
			fs = NULL;
		}
		if(recursive){
			//TODO: walk_newdeleted ?
			*rrc = walk_nolocal(ctx,rpath,srv->id,srv->hash);

			//walk_nolocal will never leave stuff behind, so we're done
		}
	} else { //fs->is_dir				
		if(recursive){
			*rrc = walk_noremote(ctx,rpath,srv->id,srv->hash);

			rc = dirempty(srv->id,&empty);
			MC_CHKERR(rc);
		} else {
			empty = true;
		}
		if(empty){
			rc = cleardir(ctx,rpath);
			MC_CHKERR(rc);
			rc = fs_rmdir(fpath);
			MC_CHKERR(rc);
		} else { // files remain
			// Umnderlying handlers will have taken care of that
		}
	}
	//Hash was updated
	rc = db_update_file(srv);
	MC_CHKERR(rc);
	return 0;
}
int download_deleted_file(mc_sync_ctx *ctx, const string& fpath, const string& rpath, mc_file_fs *fs, mc_file *db, mc_file *srv, bool recursive, int* rrc){
	int rc;
	bool empty;
	
	if(!db) rc = db_insert_file(srv);
	else rc = db_update_file(srv);
	MC_CHKERR(rc);

	if(fs){
		if(!fs->is_dir){
			rc = fs_delfile(fpath);
			MC_CHKERR(rc);
		} else {
			MC_DBG("Replacing a dir with a deleted file");
			if(recursive){
				*rrc = walk_noremote(ctx,rpath,srv->id,NULL);

				rc = dirempty(srv->id,&empty);
				MC_CHKERR(rc);
			}
			if(!recursive || empty){ // empty
				srv->status = MC_FILESTAT_DELETED;
				if(db == NULL) rc = db_insert_file(srv);
				else rc = db_update_file(srv);
				MC_CHKERR(rc);
				rc = cleardir(ctx,rpath);
				MC_CHKERR(rc);
				rc = fs_rmdir(fpath);
				MC_CHKERR(rc);
			} else { // files remain
				// Underlying handlers will have taken care of that
			}
		}
	}
	return 0;
}
int download_dir(mc_sync_ctx *ctx, const string& fpath, const string& rpath, mc_file_fs *fs, mc_file *db, mc_file *srv, bool recursive, unsigned char serverhash[16], int* rrc){
	int rc;

	if(!db) rc = db_insert_file(srv);
	else rc = db_update_file(srv);
	MC_CHKERR(rc);			
	
	if(fs && !fs->is_dir){
		MC_DBG("Replacing a file with a dir");
		rc = fs_delfile(fpath);
		MC_CHKERR(rc);
		fs = NULL;
	}

	if(!fs){
		rc = fs_mkdir(fpath);
		MC_CHKERR(rc);
	}

	if(recursive){
		if(db && memcmp(serverhash,db->hash,16) == 0)
			*rrc = walk_nochange(ctx,rpath,srv->id,srv->hash);
		else
			*rrc = walk(ctx,rpath,srv->id,srv->hash);

		//Hashupdate
		rc = db_update_file(srv);
		MC_CHKERR(rc);
	}

	rc = fs_touch(fpath,srv->mtime,srv->ctime);
	MC_CHKERR(rc);
	return 0;
}
int download_file(mc_sync_ctx *ctx, const string& fpath, const string& rpath, mc_file_fs *fs, mc_file *db, mc_file *srv, bool recursive, mc_crypt_ctx *extcctx, int* rrc){
	int rc;
	bool empty;
	bool doit;
	mc_crypt_ctx cctx;
	if(extcctx) init_crypt_ctx_copy(&cctx,extcctx);
	else init_crypt_ctx(&cctx,ctx);

	if(fs && fs->is_dir){
		MC_DBG("Replacing a dir with a file");

		if(recursive){
			*rrc = walk_noremote(ctx,rpath,srv->id,srv->hash);

			rc = dirempty(srv->id,&empty);
			MC_CHKERR(rc);
		}
		if(!recursive || empty){
			srv->status = MC_FILESTAT_DELETED;
			if(!db) rc = db_insert_file(srv);
			else rc = db_update_file(srv);
			MC_CHKERR(rc);

			rc = cleardir(ctx,rpath);
			MC_CHKERR(rc);
			rc = fs_rmdir(fpath);
			MC_CHKERR(rc);

			//Only if it was empty we can download the actual file
			doit = true;
		} else { // files remain
			// Underlying handlers will have taken care of that
			doit = false;


			//TODO: They won't take care of filestring!
		}
	} else { // fs || !fs->is_dir
		doit = true;
		if(fs && srv->size == fs->size){ //modify check only on size match
			rc = download_checkmodified(&cctx,fpath,srv,&doit); //TODO: simply compare srv->hash and db->hash?
			MC_CHKERR(rc);
		}
		if(!doit){ //not modified
			if(!db){
				rc = db_insert_file(srv);
				MC_CHKERR(rc);
			} else {
				rc = db_update_file(srv);
				MC_CHKERR(rc);
			}
		}
	}

	if(doit){
		srv->status = MC_FILESTAT_INCOMPLETE_DOWN;				
		if(db == NULL) rc = db_insert_file(srv);
		else rc = db_update_file(srv);
		MC_CHKERR(rc);

		rc = download_actual(&cctx,fpath,srv);
		MC_CHKERR(rc);

		srv->status = MC_FILESTAT_COMPLETE;
		rc = db_update_file(srv);
		MC_CHKERR(rc);
	}

	rc = fs_touch(fpath,srv->mtime,srv->ctime);
	MC_CHKERR(rc);
	return 0;
}
/* Download the file from the server
*	fs and db may be NULL	*/
int download(mc_sync_ctx *ctx, const string& path, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr, bool recursive, mc_crypt_ctx *extcctx){
	Q_ASSERT(srv != NULL);
	unsigned char serverhash[16];
	int rc;
	int rrc = 0; //recursive return value, only changed by walk-calls
	string fpath,rpath; //full path, for fs calls and rpath for recursive calls
	rpath.assign(path).append(srv->name);
	fpath.assign(ctx->sync->path).append(rpath);

	if(srv->is_dir) { memcpy(serverhash,srv->hash,16); memset(srv->hash,0,16); } //Server (directory)hashes are never saved
	
	if(srv->status == MC_FILESTAT_INCOMPLETE_UP){
		MC_INF("Not downloading file " << srv->id << ": " << printname(srv) << ", file is not complete");
		if(db) rc = db_update_file(srv);
		else rc = db_insert_file(srv);
		MC_CHKERR(rc);
		if(db) crypt_filestring(ctx,db,hashstr);
		else crypt_filestring(ctx,srv,hashstr);
		return MC_ERR_INCOMPLETE_SKIP;
		//TODO: Partial download?

	} else if(srv->status == MC_FILESTAT_DELETED){
		MC_INF("Downloading deleted file " << srv->id << ": " << printname(srv));

		if(srv->is_dir) rc = download_deleted_dir(ctx,fpath,rpath,fs,db,srv,recursive,&rrc);
		else rc = download_deleted_file(ctx,fpath,rpath,fs,db,srv,recursive,&rrc);
		MC_CHKERR(rc);

		crypt_filestring(ctx,srv,hashstr);

	} else { // srv->status == MC_FILESTAT_COMPLETE
		if(!fs){
			if(fs_exists(fpath)){
				MC_NOTIFY(MC_NT_INCOMPATIBLE_FS, fpath);
				MC_ERR_MSG(MC_ERR_INCOMPATIBLE_FS, "The file " << fpath << " should not exist, but is returned by the filesystem - assuming case sensitive FS");
			}
		}

		MC_INF("Downloading file " << srv->id << ": " << printname(srv));

		if(srv->is_dir) rc = download_dir(ctx,fpath,rpath,fs,db,srv,recursive,serverhash,&rrc);
		else rc = download_file(ctx,fpath,rpath,fs,db,srv,recursive,extcctx,&rrc);
		MC_CHKERR(rc);

		crypt_filestring(ctx,srv,hashstr);
	}
	return rrc;
}

/* Upload helpers */
int upload_checkmodified(mc_crypt_ctx *cctx, const string& fpath, mc_file_fs *fs, mc_file *srv, unsigned char hash[16], bool *modified){
	int rc;

	if(!fs->is_dir && srv && (!fs->is_dir) && (!srv->is_dir) && (fs->size == srv->size) && (srv->status == MC_FILESTAT_COMPLETE)){ //only try if there's a chance
		rc = crypt_filemd5_known(cctx,srv,hash,fpath);
		MC_CHKERR(rc);

		if((srv != NULL) && (!fs->is_dir) && (!srv->is_dir) && (fs->size == srv->size) && (srv->status == MC_FILESTAT_COMPLETE) && (memcmp(srv->hash,hash,16) == 0)){ // File not modified
			*modified = false;
			MC_DBG("File contents not modified");
		} else {
			*modified = true;
		}
	} else {
		if(fs->is_dir) memset(hash,0,16); //dirs are always modified (otherwise we would not have come here)
		*modified = true; //if there's no srv it must be "modified"
		
	}

	return 0;
}
int upload_actual(mc_crypt_ctx *cctx, const string& path, const string& fpath, mc_file_fs *fs, mc_file *db, bool insertnew = false){
	int rc;
	FILE* fdesc;
	int64 sent;

	MC_DBGL("Opening file " << fpath << " for reading");
	MC_NOTIFYSTART(MC_NT_UL,fpath);
	fdesc = fs_fopen(fpath,"rb");

	fs_fseek(fdesc,0,SEEK_SET);
	rc = crypt_init_upload(cctx,db);
	MC_CHKERR_FD_UP(rc,fdesc,cctx);

	MC_NOTIFYPROGRESS(0,db->size);						
	MC_CHECKTERMINATING_FD_UP(fdesc,cctx);
	if(db->size <= MC_SENDBLOCKSIZE){
		rc = crypt_putfile(cctx,path,db,db->size,fdesc);
		MC_CHKERR_FD_UP(rc,fdesc,cctx);
		sent = db->size;
	} else {
		rc = crypt_putfile(cctx,path,db,MC_SENDBLOCKSIZE,fdesc);
		MC_CHKERR_FD_UP(rc,fdesc,cctx);
		sent = MC_SENDBLOCKSIZE;
	}

	if(insertnew){
		rc = db_insert_file(db);
		MC_CHKERR_FD_UP(rc,fdesc,cctx);
	}

	if(sent < db->size){
		// Complete the upload
		while(sent < fs->size){
			MC_NOTIFYPROGRESS(sent,db->size);
			MC_CHECKTERMINATING_FD_UP(fdesc,cctx);
			if(db->size-sent <= MC_SENDBLOCKSIZE){
				rc = crypt_addfile(cctx,db->id,sent,db->size-sent,fdesc,db->hash);
				sent += db->size-sent;
			} else {
				rc = crypt_addfile(cctx,db->id,sent,MC_SENDBLOCKSIZE,fdesc,db->hash);
				sent += MC_SENDBLOCKSIZE;
			}
			MC_CHKERR_FD_UP(rc,fdesc,cctx);
		}
	}
	rc = crypt_finish_upload(cctx);
	MC_CHKERR_FD(rc,fdesc);
	
	fs_fclose(fdesc);
	MC_NOTIFYEND(MC_NT_UL);

	return 0;
}
/* sub-uploads */
int upload_newdeleted(mc_sync_ctx *ctx, const string& rpath, mc_file *newdb, mc_file *srv, bool recursive, int *rrc){
	int rc;
	bool empty;

	newdb->id = srv->id;
	newdb->name = srv->name;
	newdb->cryptname = srv->cryptname;
	newdb->ctime = srv->ctime;
	newdb->mtime = time(NULL);
	newdb->size = srv->size;
	newdb->is_dir = srv->is_dir;
	newdb->parent = srv->parent;
	newdb->status = MC_FILESTAT_DELETED;

	rc = db_insert_file(newdb);
	MC_CHKERR(rc);
	if(newdb->is_dir){					
		if(recursive){
			*rrc = walk_nolocal(ctx,rpath,newdb->id,newdb->hash);
						
			rc = db_update_file(newdb);
			MC_CHKERR(rc);
						
			rc = dirempty(newdb->id,&empty);
			MC_CHKERR(rc);
		} else {
			empty = true;
		}					

		if(empty){
			rc = srv_delfile(newdb);
			MC_CHKERR(rc);
		} else { // files remain
			//Underlying handlers...?!
		}
	} else {
		rc = srv_delfile(newdb);
		MC_CHKERR(rc);
	}
	return 0;
}
int upload_new(mc_sync_ctx *ctx, const string& path, const string& fpath, const string& rpath, mc_file_fs *fs, mc_file *newdb, mc_file *srv, int parent, bool recursive, mc_crypt_ctx *extcctx, int *rrc){
	int rc;
	bool modified;
	mc_crypt_ctx cctx;
	if(extcctx) init_crypt_ctx_copy(&cctx,extcctx);
	else init_crypt_ctx(&cctx,ctx);

	rc = upload_checkmodified(&cctx, fpath, fs, srv, newdb->hash, &modified);
	MC_CHKERR(rc);

	if(srv == NULL){
		newdb->id = MC_FILEID_NONE;
	} else {
		newdb->id = srv->id;
	}
	//cryptname will be generated
	newdb->name = fs->name;
	newdb->ctime = fs->ctime;
	newdb->mtime = fs->mtime;
	newdb->size = fs->size;
	newdb->is_dir = fs->is_dir;
	newdb->parent = parent;
	if(fs->is_dir) newdb->status = MC_FILESTAT_COMPLETE;
	else newdb->status = MC_FILESTAT_INCOMPLETE_UP_ME;

	if(!modified){ // File not modified
			newdb->status = MC_FILESTAT_COMPLETE;
			memcpy(newdb->hash,srv->hash,16);

			rc = crypt_patchfile(&cctx,path,newdb);
			MC_CHKERR(rc);
			
			rc = db_insert_file(newdb);
			MC_CHKERR(rc);			
	} else {			
		if(fs->is_dir){
			memset(newdb->hash,'\0',16);

			rc = crypt_putfile(&cctx,path,newdb,0,NULL);
			MC_CHKERR(rc);
						
			rc = db_insert_file(newdb);
			MC_CHKERR(rc);

			if(recursive){
				*rrc = walk(ctx,rpath,newdb->id,newdb->hash);
			}
		} else {
			rc = crypt_filemd5_new(&cctx,newdb->hash,fpath,newdb->size);
			MC_CHKERR(rc);		

			rc = upload_actual(&cctx,path,fpath,fs,newdb,true);
			MC_CHKERR(rc);

			newdb->status = MC_FILESTAT_COMPLETE;
		}
		rc = db_update_file(newdb);
		MC_CHKERR(rc);
	}
	return 0;
}
int upload_patchdeleted(mc_sync_ctx *ctx, const string& path, const string& rpath, mc_file *db, bool recursive, mc_crypt_ctx *extcctx, int *rrc){
	int rc;
	mc_crypt_ctx cctx;
	if(extcctx) init_crypt_ctx_copy(&cctx,extcctx);
	else init_crypt_ctx(&cctx,ctx);

	rc = crypt_patchfile(&cctx,path,db);
	MC_CHKERR(rc);
	if(recursive && db->is_dir){
		*rrc = walk_nolocal(ctx,rpath,db->id,db->hash);
		//Hashupdate
		rc = db_update_file(db);
		MC_CHKERR(rc);
	}
	return 0;
}
int upload_deleted(mc_sync_ctx *ctx, const string& rpath, mc_file *db, bool recursive, int *rrc){
	int rc;
	bool empty;

	db->status = MC_FILESTAT_DELETED;
	db->mtime = time(NULL);

	rc = db_update_file(db);
	MC_CHKERR(rc);
	if(db->is_dir){					
		if(recursive){
			*rrc = walk_nolocal(ctx,rpath,db->id,db->hash);
						
			rc = db_update_file(db);
			MC_CHKERR(rc);
						
			rc = dirempty(db->id,&empty);
			MC_CHKERR(rc);
		} else {
			empty = true;
		}					

		if(empty){
			rc = srv_delfile(db);
			MC_CHKERR(rc);
		} else { // files remain
			//Underlying handlers...?!
		}
	} else {
		rc = srv_delfile(db);
		MC_CHKERR(rc);
	}
	return 0;
}
int upload_normal(mc_sync_ctx *ctx, const string& path, const string& fpath, const string& rpath, mc_file_fs *fs, mc_file *db, mc_file *srv, bool recursive, mc_crypt_ctx *extcctx, int *rrc){
	int rc;
	unsigned char hash[16];
	bool empty;
	bool modified;
	mc_crypt_ctx cctx;
	if(extcctx) init_crypt_ctx_copy(&cctx,extcctx);
	else init_crypt_ctx(&cctx,ctx);
	
	rc = upload_checkmodified(&cctx, fpath, fs, srv, hash, &modified);
	MC_CHKERR(rc);
	
	db->mtime = fs->mtime;
	db->ctime = fs->ctime;
	db->size = fs->size;
	
	if(!modified){
		
		rc = crypt_patchfile(&cctx,path,db);
		MC_CHKERR(rc);

		rc = db_update_file(db);
		MC_CHKERR(rc);

	} else if(srv != NULL){
		if(fs->is_dir){
			if(!srv->is_dir){
				MC_DBG("Replacing a file with a dir");
				srv->status = MC_FILESTAT_DELETED;
				srv->mtime = time(NULL);

				rc = srv_delfile(srv);
				MC_CHKERR(rc);
			}
			
			db->status = MC_FILESTAT_COMPLETE;

			rc = db_update_file(db);
			MC_CHKERR(rc);

			rc = crypt_putfile(&cctx,path,db,0,NULL);
			MC_CHKERR(rc);
					
			if(recursive){
				*rrc = walk(ctx,rpath,db->id,db->hash);

			}
			// Hashupdate
			rc = db_update_file(db);
			MC_CHKERR(rc);
		} else { // !fs->is_dir
			if(!srv->is_dir){

				db->status = MC_FILESTAT_INCOMPLETE_UP_ME;
							
				rc = crypt_filemd5_new(&cctx,db->hash,fpath,db->size);
				MC_CHKERR(rc);	
				
				rc = db_update_file(db);
				MC_CHKERR(rc);

				rc = upload_actual(&cctx,path,fpath,fs,db);
				MC_CHKERR(rc);

				db->status = MC_FILESTAT_COMPLETE;
				rc = db_update_file(db);
				MC_CHKERR(rc);
			} else { // srv->is_dir
				MC_DBG("Replacing a dir with a file");

				db->status = MC_FILESTAT_COMPLETE;
				db->is_dir = true;

				rc = db_update_file(db);
				MC_CHKERR(rc);

				if(recursive){
					*rrc = walk_nolocal(ctx,rpath,db->id,db->hash);

					rc = dirempty(db->id,&empty);
					MC_CHKERR(rc);
				} else {
					empty = true;
				}
				if(empty){
					rc = srv_delfile(db);
					MC_CHKERR(rc);
							
					db->status = MC_FILESTAT_INCOMPLETE_UP_ME;
					db->is_dir = false;
					rc = crypt_filemd5_new(&cctx,db->hash,fpath,db->size);
					MC_CHKERR(rc);	
					//db->mtime = time(NULL);

					rc = db_update_file(db);
					MC_CHKERR(rc);

					rc = upload_actual(&cctx,path,fpath,fs,db);
					MC_CHKERR(rc);

					db->status = MC_FILESTAT_COMPLETE;
					rc = db_update_file(db);
					MC_CHKERR(rc);


				} else { // files remain
					//Underlying handlers?
				}
			}
		}
	} else { // srv == NULL
		rc = db_delete_file(db->id); //old ID is most likely void, let server generate new
		MC_CHKERR(rc);
		db->id = MC_FILEID_NONE;

		db->status = MC_FILESTAT_INCOMPLETE_UP_ME;
		memcpy(db->hash,hash,16);


		if(fs->is_dir){
			rc = crypt_putfile(&cctx,path,db,0,NULL);
			MC_CHKERR(rc);

			rc = db_insert_file(db);
			MC_CHKERR(rc);

			if(recursive){
				*rrc = walk(ctx,rpath,db->id,db->hash);
			}
		} else {
			rc = upload_actual(&cctx,path,fpath,fs,db,true);
			MC_CHKERR(rc);
		}

		db->status = MC_FILESTAT_COMPLETE;
		rc = db_update_file(db);
		MC_CHKERR(rc);
	}
	return 0;
}
/* Upload the file to the server
*	srv may be NULL 
*	if db is NULL, fs and parent must be set
*	if fs is NULL, db and srv must be set and we treat the file as MC_FILESTAT_DELETED	*/
int upload(mc_sync_ctx *ctx, const string& path, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr, int parent, bool recursive, mc_crypt_ctx *extcctx){
	Q_ASSERT(db != NULL || (fs != NULL && parent != 0));
	Q_ASSERT(fs != NULL || (db != NULL && srv != NULL));
	mc_file newdb;
	int rc;
	int rrc = 0; //recursive return value, only changed by walk-calls
	string fpath,rpath;
	list<mc_file> l;
	list<mc_file>::iterator lit,lend;
	fpath.assign(ctx->sync->path);
	rpath.assign(path);

	if(fs && ((db && fs->mtime < db->mtime) || (srv && fs->mtime < srv->mtime))){
		//Upload means we want this to be persistent, so it has to be newer (#24)
		MC_DBG("Touching for persistency");
		string tpath;
		tpath.assign(fpath).append(path).append(fs->name);
		fs->mtime = time(NULL);
		rc = fs_touch(tpath,fs->mtime);
		MC_CHKERR(rc);
	}
	
	if(db == NULL){
		if(fs == NULL){
			MC_INF("Uploading new deleted file " << srv->id << ": " << printname(srv));
			rpath.append(srv->name);
			fpath.append(rpath);

			rc = upload_newdeleted(ctx, rpath, &newdb, srv, recursive, &rrc);
			MC_CHKERR(rc);
			db = &newdb;

			crypt_filestring(ctx,&newdb,hashstr);
		} else {
			MC_INF("Uploading new file: " << printname(fs));
			rpath.append(fs->name);
			fpath.append(rpath);
			
			rc = upload_new(ctx, path, fpath, rpath, fs, &newdb, srv, parent, recursive, extcctx, &rrc);
			MC_CHKERR(rc);
			db = &newdb;

			crypt_filestring(ctx,&newdb,hashstr);
		}
	} else { // db != NULL
		if(fs == NULL){
			if((db->status == MC_FILESTAT_DELETED) && (srv->status == MC_FILESTAT_DELETED)){
				if(db->mtime == srv->mtime && memcmp(db->hash,srv->hash,16) == 0){
					MC_DBG("Unchanged deleted file " << db->id << ": " << printname(db)); //TODO: still necessary?
					/*if(db->is_dir){
						rrc = walk_nolocal(ctx,fpath,db->id,db->hash);
						//Hashupdate
						rc = db_update_file(db);
						MC_CHKERR(rc);
					}*/
					crypt_filestring(ctx,db,hashstr);

				} else { //db->mtime != srv->mtime
					MC_INF("Patching deleted file " << db->id << ": " << printname(db));
					rpath.append(db->name);

					rc = upload_patchdeleted(ctx, path, rpath, db, recursive, extcctx, &rrc);
					MC_CHKERR(rc);
					
					crypt_filestring(ctx,db,hashstr);
				}
			} else {
				MC_INF("Uploading deleted file " << db->id << ": " << printname(db));
				rpath.append(db->name);

				rc = upload_deleted(ctx, rpath, db, recursive, &rrc);
				MC_CHKERR(rc);

				crypt_filestring(ctx,db,hashstr);
			}
		} else { // fs != NULL
			MC_INF("Uploading file " << db->id << ": " << printname(fs));
			rpath.append(db->name);
			fpath.append(rpath);
			
			rc = upload_normal(ctx, path, fpath, rpath, fs, db, srv, recursive, extcctx, &rrc);
			MC_CHKERR(rc);

			crypt_filestring(ctx,db,hashstr);
		}
	}

	//While this sometimes overwrites legit changes, generally it undoes unwanted mtime changes from
	//things we did within the directory
	if(db && db->is_dir && db->status != MC_FILESTAT_DELETED){
		rc = fs_touch(fpath,db->mtime,db->ctime);
		MC_CHKERR(rc);
	}

	return rrc;
}
/* Purge the file from the entire system, it is some kind of leftover - USE WITH CAUTION
	the file should not exist in filesystem unless ignored!	
	no conflict checks will be performed whatsoever!	
	db or srv may be NULL	*/
int purge(mc_file *db, mc_file *srv){
	int rc;
	list<mc_file> sublist;
	list<mc_file>::iterator lit,lend;
	if(db != NULL){ //local purge
		MC_INF("Locally PURGING file " << db->id << ": " << printname(db));
		if(db->is_dir){
			rc = db_list_file_parent(&sublist,db->id);
			MC_CHKERR(rc);
			lit = sublist.begin();
			lend = sublist.end();
			while(lit != lend){
				rc = purge(&*lit,NULL);
				MC_CHKERR(rc);
				++lit;
			}

		}
		rc = db_delete_file(db->id);
		MC_CHKERR(rc);
	}
	sublist.clear();
	if(srv != NULL){ //remote purge
		MC_INF("Remotely PURGING file " << srv->id << ": " << printname(srv));
		if(srv->is_dir){
			rc = srv_listdir(&sublist,srv->id); //don't need to decrypt filenames / sizes
			MC_CHKERR(rc);
			lit = sublist.begin();
			lend = sublist.end();
			while(lit != lend){
				rc = purge(NULL,&*lit);
				MC_CHKERR(rc);
				++lit;
			}

		}
		rc = srv_purgefile(srv->id);
		MC_CHKERR(rc);
	}

	return 0;
}
/* Called by verifyandcomplete, resumes an interrupted upload 
*	Should not be called on dirs	*/
int complete_up(mc_sync_ctx *ctx, const string& path, const string& fpath, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr){
	FILE *fdesc;
	int64 sent;
	mc_crypt_ctx cctx;
	int rc;
	MC_INF("Resuming upload of file " << db->id << ": " << printname(db));
	if(fs->is_dir) MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"not to be called on dirs");
	
	if(memcmp(db->hash,srv->hash,16)){
		MC_INF("Hash mismatch, restarting upload");
		return upload(ctx, path, fs, db, srv, hashstr);
	}

	init_crypt_ctx(&cctx,ctx);
	
	rc = crypt_initresume_up(&cctx,db,&sent);
	MC_CHKERR_UP(rc,&cctx);
	
	if(sent > db->size){
		MC_INF("File changed on disk, restarting upload");
		crypt_abort_upload(&cctx);
		return upload(ctx, path, fs, db, srv, hashstr);
	}

	MC_DBG("Opening file " << fpath << " for reading");
	MC_NOTIFYSTART(MC_NT_UL,fpath);
	fdesc = fs_fopen(fpath,"rb");
	if(!fdesc){
		crypt_abort_upload(&cctx);
		MC_CHKERR_MSG(MC_ERR_IO,"Could not read the file");
	}

	fs_fseek(fdesc,0,SEEK_END);
	if(fs_ftell(fdesc) != db->size){
		int code = errno;
		MC_INF("File changed on disk, restarting upload");
		crypt_abort_upload(&cctx);
		fs_fclose(fdesc);
		return upload(ctx, path, fs, db, srv, hashstr);
	}
	
	rc = crypt_resumetopos_up(&cctx,db,sent,fdesc);
	MC_CHKERR_FD_UP(rc,fdesc,&cctx);
	//fs_fseek(fdesc,sent,SEEK_SET);

	if(fs_ftell(fdesc) != sent){
		MC_INF("File changed on disk, restarting upload");
		crypt_abort_upload(&cctx);
		fs_fclose(fdesc);
		return upload(ctx, path, fs, db, srv, hashstr);
	}

	while(sent < db->size){
		MC_NOTIFYPROGRESS(sent,db->size);
		MC_CHECKTERMINATING_FD(fdesc);
		if(db->size-sent <= MC_SENDBLOCKSIZE){
			rc = crypt_addfile(&cctx,db->id,sent,db->size-sent,fdesc,db->hash);
			sent += db->size-sent;
		} else {
			rc = crypt_addfile(&cctx,db->id,sent,MC_SENDBLOCKSIZE,fdesc,db->hash);
			sent += MC_SENDBLOCKSIZE;
		}
		MC_CHKERR_FD_UP(rc,fdesc,&cctx);
	}

	fs_fclose(fdesc);
	MC_NOTIFYEND(MC_NT_UL);

	rc = crypt_finish_upload(&cctx);
	MC_CHKERR(rc);

	db->status = MC_FILESTAT_COMPLETE;
	rc = db_update_file(db);
	MC_CHKERR(rc);

	crypt_filestring(ctx,db,hashstr);

	return 0;
}

/* Called by verifyandcomplete, resumes an interrupted download 
*	Should not be called on dirs	*/
int complete_down(mc_sync_ctx *ctx, const string& path, const string& fpath, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr){
	FILE *fdesc;
	int64 offset,written;
	mc_crypt_ctx cctx;
	int rc;
	MC_INF("Resuming download of file " << db->id << ": " << printname(db));
	if(srv->is_dir) MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"not to be called on dirs");

	if(memcmp(db->hash,srv->hash,16)){
		MC_INF("Hash mismatch, restarting download");
		return download(ctx, path, fs, db, srv, hashstr);
	}

	init_crypt_ctx(&cctx,ctx);

	rc = crypt_initresume_down(&cctx,srv);
	MC_CHKERR_DOWN(rc,&cctx);

	MC_DBG("Opening file " << fpath << " for writing");
	MC_NOTIFYSTART(MC_NT_DL,fpath);
	fdesc = fs_fopen(fpath,"r+b");
	if(!fdesc){
		crypt_abort_download(&cctx);
		MC_CHKERR_MSG(MC_ERR_IO,"Could not write the file");
	}

	fs_fseek(fdesc,0,SEEK_END);
	offset = fs_ftell(fdesc);

	rc = crypt_resumetopos_down(&cctx,srv,offset,fdesc);
	MC_CHKERR_FD_DOWN(rc,fdesc,&cctx);

	if(offset > srv->size){
		MC_INF("File changed on disk, restarting download");
		crypt_abort_download(&cctx);
		fs_fclose(fdesc);
		return download(ctx, path, fs, db, srv, hashstr);
	}
	while(offset < srv->size){
		MC_NOTIFYPROGRESS(offset,srv->size);
		MC_CHECKTERMINATING_FD_DOWN(fdesc,&cctx);
		rc = crypt_getfile(&cctx,srv->id,offset,MC_RECVBLOCKSIZE,fdesc,&written,srv->hash);
		MC_CHKERR_FD_DOWN(rc,fdesc,&cctx);
		offset += written;
	}
	
	rc = crypt_finish_download(&cctx);
	MC_CHKERR_FD(rc,fdesc);

	fs_fclose(fdesc);
	MC_NOTIFYEND(MC_NT_DL);
	
	rc = fs_touch(fpath,srv->mtime,srv->ctime);
	MC_CHKERR(rc);

	srv->status = MC_FILESTAT_COMPLETE;
	rc = db_update_file(srv);
	MC_CHKERR(rc);	

	crypt_filestring(ctx,srv,hashstr);

	return 0;
}