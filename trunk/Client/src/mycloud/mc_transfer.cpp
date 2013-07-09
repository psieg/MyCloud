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
	list<mc_file_fs>::iterator lit,lend;
	string fpath;
	int rc;
	fpath.assign(ctx->sync->path).append(path).append("/");
	MC_DBG("Clearing directory " << path << " of filtered files");
	rc = fs_listdir(&onfs, fpath);
	MC_CHKERR(rc);
	lit = onfs.begin();
	lend = onfs.end();
	while(lit != lend){
		if(clearall || match_full(fpath,&*lit,ctx->filter)){
			fpath.assign(path).append("/").append(lit->name);
			if(lit->is_dir){
				rc = cleardir(ctx,fpath,true);
				MC_CHKERR(rc);
				rc = fs_rmdir(fpath);
				MC_CHKERR(rc);
			} else {
				rc = fs_delfile(fpath);
				MC_CHKERR(rc);
			}
		}
		++lit;
	}
	return 0;
}
/* Download the file from the server
*	fs and db may be NULL	*/
int download(mc_sync_ctx *ctx, const string& path, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr, bool recursive, mc_crypt_ctx *extcctx){
	unsigned char hash[16], serverhash[16];
	FILE *fdesc = 0;
	int64 offset = 0, written = 0;
	int rc;
	int rrc = 0; //recursive return value, only changed by walk-calls
	string fpath,rpath; //full path, for fs calls and rpath for recursive calls
	list<mc_file> l;
	list<mc_file>::iterator lit,lend;
	bool empty;
	bool modified;
	bool terminating = false;
	mc_crypt_ctx cctx;
	if(extcctx) init_crypt_ctx_copy(&cctx,extcctx);
	else init_crypt_ctx(&cctx,ctx);
	rpath.assign(path).append(srv->name);
	fpath.assign(ctx->sync->path).append(rpath);

	if(srv->is_dir) { memcpy(serverhash,srv->hash,16); memset(srv->hash,0,16); } //Server (directory)hashes are never saved

	if(srv->status == MC_FILESTAT_INCOMPLETE_UP){
		MC_INF("Not downloading file " << srv->id << ": " << printname(srv) << ", file is not complete");
		if(db) crypt_filestring(ctx,db,hashstr); //We use db to have a hash mismatch -> no fullsync
		return MC_ERR_INCOMPLETE_SKIP;
		//TODO: Partial download?
	} else if(srv->status == MC_FILESTAT_DELETED){
		MC_INF("Downloading deleted file " << srv->id << ": " << printname(srv));
		if(srv->is_dir){
			if(db == NULL){
				rc = db_insert_file(srv);
				MC_CHKERR(rc);
			} else {
				rc = db_update_file(srv);
				MC_CHKERR(rc);
			}
			if(fs == NULL || !fs->is_dir){
				if(fs){ // then !fs->is_dir
					MC_DBG("Replacing a file with a deleted dir");
					rc = fs_delfile(fpath);
					MC_CHKERR(rc);
					fs = NULL;
				}
				if(recursive){
					//TODO: walk_newdeleted ?
					rrc = walk_nolocal(ctx,rpath,srv->id,srv->hash);
					//rrc = rc;

					//walk_nolocal will never leave stuff behind, so we're done
				}
			} else { //fs->is_dir				
				if(recursive){
					rrc = walk_noremote(ctx,rpath,srv->id,srv->hash);
					//rrc = rc;

					rc = db_list_file_parent(&l,srv->id);
					MC_CHKERR(rc);

					empty = true;
					lend = l.end();
					for(lit = l.begin(); lit != lend; ++lit){
						if(lit->status != MC_FILESTAT_DELETED){
							empty = false;
							break;
						}
					}
				} else {
					empty = true;
				}
				if(empty){
					rc = cleardir(ctx,rpath);
					MC_CHKERR(rc);
					rc = fs_rmdir(fpath);
					MC_CHKERR(rc);
				} else { // files remain
					// Underlying handlers will have taken care of that
				}
			}
			//Hash was updated
			rc = db_update_file(srv);
			MC_CHKERR(rc);
			crypt_filestring(ctx,srv,hashstr);
		} else { //!srv->is_dir
			if(db == NULL){
				rc = db_insert_file(srv);
				MC_CHKERR(rc);
			} else {
				rc = db_update_file(srv);
				MC_CHKERR(rc);
			}
			if(fs){
				if(!fs->is_dir){
					rc = fs_delfile(fpath);
					MC_CHKERR(rc);
				} else {
					MC_DBG("Replacing a dir with a deleted file");
					if(recursive){
						rrc = walk_noremote(ctx,rpath,srv->id,NULL);
						//rrc = rc;

						rc = db_list_file_parent(&l,srv->id);
						MC_CHKERR(rc);

						empty = true;
						lend = l.end();
						for(lit = l.begin(); lit != lend; ++lit){
							if(lit->status != MC_FILESTAT_DELETED) {
								empty = false;
								break;
							}
						}
					} else {
						empty = true;
					}
					if(empty){ // empty
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
			crypt_filestring(ctx,srv,hashstr);
		}
	} else { // srv->status == MC_FILESTAT_COMPLETE
		MC_INF("Downloading file " << srv->id << ": " << printname(srv));
		if(db && fs && (srv->mtime-fs->mtime==3600 || srv->mtime-fs->mtime==-3600))
			MC_WRN("Exact 1-hour offset, something smells wrong");
		if(srv->is_dir){
			if((fs != NULL) && !fs->is_dir){
				MC_DBG("Replacing a file with a dir");
				rc = fs_delfile(fpath);
				MC_CHKERR(rc);
				fs = NULL;
			}
			if(db == NULL){
				rc = db_insert_file(srv);
				MC_CHKERR(rc);
			} else {
				rc = db_update_file(srv);
				MC_CHKERR(rc);
			}
			if(fs == NULL){
				rc = fs_mkdir(fpath);
				MC_CHKERR(rc);
			}
			
			if(recursive){
				if(db && memcmp(serverhash,db->hash,16) == 0)
					rrc = walk_nochange(ctx,rpath,srv->id,srv->hash);
				else
					rrc = walk(ctx,rpath,srv->id,srv->hash);
				//rrc = rc;

				//Hashupdate
				rc = db_update_file(srv);
				MC_CHKERR(rc);
				crypt_filestring(ctx,srv,hashstr);
			}
		} else { //!srv->is_dir
			if((fs == NULL) || (!fs->is_dir)){
				modified = true;
				if(fs != NULL){
					if(srv->size == fs->size){ //modify check only on size match
						MC_DBGL("Opening file " << fpath << " for reading");
						MC_NOTIFYSTART(MC_NT_DL,fpath);
						fdesc = fs_fopen(fpath,"rb");
						if(!fdesc) MC_CHKERR_MSG(MC_ERR_IO,"Could not read the file");
				
						rc = crypt_filemd5_known(&cctx,srv,hash,fpath,fdesc);
						MC_CHKERR(rc);

						fclose(fdesc);

						if(memcmp(srv->hash,hash,16) == 0){ //File not modified
							modified = false;
							MC_DBG("File contents not modified");
							MC_NOTIFYEND(MC_NT_DL);
						}
					}
				}
				if(modified){
					srv->status = MC_FILESTAT_INCOMPLETE_DOWN;				
					if(db == NULL) rc = db_insert_file(srv);
					else rc = db_update_file(srv);
					MC_CHKERR(rc);
					MC_DBGL("Opening file " << fpath << " for writing");
					MC_NOTIFYSTART(MC_NT_DL,fpath);
					fdesc = fs_fopen(fpath,"wb");
					if(!fdesc) MC_CHKERR_MSG(MC_ERR_IO,"Could not (re)write the file");
					rc = crypt_init_download(&cctx,srv);
					MC_CHKERR_FD(rc,fdesc);
					
					while(offset < srv->size){
						MC_NOTIFYPROGRESS(offset,srv->size);
						MC_CHECKTERMINATING_FD(fdesc);
						rc = crypt_getfile(&cctx,srv->id,offset,MC_RECVBLOCKSIZE,fdesc,&written,srv->hash);
						MC_CHKERR_FD(rc,fdesc);
						offset += written;
					}

					rc = crypt_finish_download(&cctx,offset,fdesc);
					MC_CHKERR_FD(rc,fdesc);

					fclose(fdesc);
					MC_NOTIFYEND(MC_NT_DL);
				} else {
					if(db == NULL){
						rc = db_insert_file(srv);
						MC_CHKERR(rc);
					}
				}
				
				srv->status = MC_FILESTAT_COMPLETE;
				rc = db_update_file(srv);
				MC_CHKERR(rc);
				crypt_filestring(ctx,srv,hashstr);
			} else { //fs->is_dir
				MC_DBG("Replacing a dir with a file");

				if(recursive){
					rrc = walk_noremote(ctx,rpath,srv->id,srv->hash);
					//rrc = rc;


					rc = db_list_file_parent(&l,srv->id);
					MC_CHKERR(rc);

					empty = true;
					lend = l.end();
					for(lit = l.begin(); lit != lend; ++lit){
						if(lit->status != MC_FILESTAT_DELETED) {
							empty = false;
							break;
						}
					}
				} else {
					empty = true;
				}
				if(empty){
					srv->status = MC_FILESTAT_DELETED;
					if(db == NULL) rc = db_insert_file(srv);
					else rc = db_update_file(srv);
					MC_CHKERR(rc);
					rc = cleardir(ctx,rpath);
					MC_CHKERR(rc);
					rc = fs_rmdir(fpath);
					MC_CHKERR(rc);

					//Only if it was empty we can download the actual file
					MC_DBGL("Opening file " << fpath << " for writing");
					MC_NOTIFYSTART(MC_NT_DL,fpath);
					fdesc = fs_fopen(fpath,"wb");
					if(!fdesc) MC_CHKERR_MSG(MC_ERR_IO,"Could not (re)write the file");
					rc = crypt_init_download(&cctx,srv);
					MC_CHKERR(rc);

					while(offset < srv->size){
						rc = crypt_getfile(&cctx,srv->id,offset,MC_RECVBLOCKSIZE,fdesc,&written,srv->hash);
						MC_CHKERR_FD(rc,fdesc);
						offset += written;
					}
					rc = crypt_finish_download(&cctx,offset,fdesc);
					MC_CHKERR_FD(rc,fdesc);

					fclose(fdesc);
					MC_NOTIFYEND(MC_NT_DL);
					
					srv->status = MC_FILESTAT_COMPLETE;
					rc = db_update_file(srv);
					MC_CHKERR(rc);
					crypt_filestring(ctx,srv,hashstr);
				} else { // files remain
					// Underlying handlers will have taken care of that
					//srv->status = MC_FILESTAT_COMPLETE;
					//rc = db_update_file(srv);
					//MC_CHKERR(rc);
					//rc = fs_touch(fpath,maxt); //bad idea, will trigger reupload next time
					//MC_CHKERR(rc);

					//TODO: They won't take care of filestring!
				}
			}
		}
		if(srv->status != MC_FILESTAT_DELETED){
			rc = fs_touch(fpath,srv->mtime,srv->ctime);
			MC_CHKERR(rc);
		}
	}
	return rrc;
}
/* Upload the file to the server
*	srv may be NULL 
*	if db is NULL, fs and parent must be set
*	if fs is NULL, db and srv must be set and we treat the file as MC_FILESTAT_DELETED	*/
int upload(mc_sync_ctx *ctx, const string& path, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr, int parent, bool recursive, mc_crypt_ctx *extcctx){
	unsigned char hash[16];
	FILE *fdesc = 0;
	mc_file newdb;
	int64 sent = 0;
	int rc;
	int rrc = 0; //recursive return value, only changed by walk-calls
	string fpath,rpath;
	list<mc_file> l;
	list<mc_file>::iterator lit,lend;
	bool empty;
	bool terminating = false;
	mc_crypt_ctx cctx;
	fpath.assign(ctx->sync->path);
	rpath.assign(path);
	if(extcctx) init_crypt_ctx_copy(&cctx,extcctx);
	else init_crypt_ctx(&cctx,ctx);

	//mc_sleepms(10);

	if(db == NULL){
		if(fs == NULL){
			MC_INF("Uploading never known deleted file " << printname(srv));
			rpath.append(srv->name);
			fpath.append(rpath);

			srv->status = MC_FILESTAT_DELETED;
			srv->mtime = time(NULL);

			rc = db_insert_file(srv);
			MC_CHKERR(rc);

			if(srv->is_dir){
				if(recursive){
					rrc = walk_nolocal(ctx,rpath,srv->id,srv->hash);
					//rrc = rc;
					
					rc = db_list_file_parent(&l,srv->id);
					MC_CHKERR(rc);

					empty = true;
					lend = l.end();
					for(lit = l.begin(); lit != lend; ++lit){
						if(lit->status != MC_FILESTAT_DELETED){
							empty = false;
							break;
						}
					}
				} else {
					empty = true;
				}
				if(empty){
					rc = srv_delfile(srv);
					MC_CHKERR(rc);

					crypt_filestring(ctx,srv,hashstr);
				} else { // files remain
					//Underlying handlers...
					//TODO: no filestring handlers
				}
			} else {
				crypt_filestring(ctx,srv,hashstr);

				rc = srv_delfile(srv);
				MC_CHKERR(rc);
			}
		} else {
			MC_INF("Uploading new file: " << printname(fs));
			rpath.append(fs->name);
			fpath.append(rpath);

			if(fs->is_dir){
				memset(newdb.hash,0,16);
			} else {
				MC_DBGL("Opening file " << fpath << " for reading");
				MC_NOTIFYSTART(MC_NT_UL,fpath);
				fdesc = fs_fopen(fpath,"rb");
				if(!fdesc) MC_CHKERR_MSG(MC_ERR_IO,"Could not read the file");
		
				if((srv != NULL) && (!fs->is_dir) && (!srv->is_dir) && (fs->size == srv->size) && (srv->status == MC_FILESTAT_COMPLETE))
					rc = crypt_filemd5_known(&cctx,srv,newdb.hash,fpath,fdesc);
				else
					rc = crypt_filemd5_new(&cctx,newdb.hash,fpath,fs->size,fdesc);
				MC_CHKERR_FD(rc,fdesc);
			}
			if(srv == NULL){
				newdb.id = MC_FID_NONE;
				//cryptname will be generated
			} else {
				newdb.id = srv->id;
				newdb.cryptname = srv->cryptname;
			}
			newdb.name = fs->name;
			newdb.ctime = fs->ctime;
			newdb.mtime = fs->mtime;
			newdb.size = fs->size;
			newdb.is_dir = fs->is_dir;
			newdb.parent = parent;
			if(fs->is_dir) newdb.status = MC_FILESTAT_COMPLETE;
			else newdb.status = MC_FILESTAT_INCOMPLETE_UP;

			if((srv != NULL) && (!fs->is_dir) && (!srv->is_dir) && (fs->size == srv->size) && (srv->status == MC_FILESTAT_COMPLETE) && (memcmp(srv->hash,newdb.hash,16) == 0)){ // File not modified
					newdb.mtime = fs->mtime;
					newdb.ctime = fs->ctime;
					newdb.status = MC_FILESTAT_COMPLETE;

					MC_DBG("File contents not modified");

					rc = db_insert_file(&newdb);
					MC_CHKERR(rc);

					rc = crypt_patchfile(&cctx,path,&newdb);
					MC_CHKERR(rc);

					crypt_filestring(ctx,&newdb,hashstr);

			} else {
			
				if(fs->is_dir){
					rc = crypt_putfile(&cctx,path,&newdb,0,NULL);
					MC_CHKERR(rc);
						
					rc = db_insert_file(&newdb);
					MC_CHKERR(rc);

					if(recursive){
						rrc = walk(ctx,rpath,newdb.id,newdb.hash);
						//rrc = rc;
					}
				} else {
					fseek(fdesc,0,SEEK_SET);
					rc = crypt_init_upload(&cctx,&newdb);
					MC_CHKERR_FD(rc,fdesc);

					MC_NOTIFYPROGRESS(0,fs->size);
					MC_CHECKTERMINATING_FD(fdesc);
					if(newdb.size <= MC_SENDBLOCKSIZE){
						rc = crypt_putfile(&cctx,path,&newdb,newdb.size,fdesc);
						MC_CHKERR_FD(rc,fdesc);
						sent = newdb.size;
					} else {
						rc = crypt_putfile(&cctx,path,&newdb,MC_SENDBLOCKSIZE,fdesc);
						MC_CHKERR_FD(rc,fdesc);
						sent = MC_SENDBLOCKSIZE;
					}
					rc = db_insert_file(&newdb);
					MC_CHKERR_FD(rc,fdesc);
				}
				

				if(fs->is_dir){
					// We're done
				} else {
					if (sent == newdb.size){
						// We're done
						rc = crypt_finish_upload(&cctx,newdb.id);
						MC_CHKERR_FD(rc,fdesc);
					} else {
						// Complete the upload
						while(sent < fs->size){
							MC_NOTIFYPROGRESS(sent,fs->size);
							MC_CHECKTERMINATING_FD(fdesc);
							if(newdb.size-sent <= MC_SENDBLOCKSIZE){
								rc = crypt_addfile(&cctx, newdb.id,sent,newdb.size-sent,fdesc,newdb.hash);
								sent += newdb.size-sent;
							} else {
								rc = crypt_addfile(&cctx, newdb.id,sent,MC_SENDBLOCKSIZE,fdesc,newdb.hash);
								sent += MC_SENDBLOCKSIZE;
							}
							MC_CHKERR_FD(rc,fdesc);
						}
						rc = crypt_finish_upload(&cctx,newdb.id);
						MC_CHKERR_FD(rc,fdesc);
					}
					newdb.status = MC_FILESTAT_COMPLETE;
				}
				rc = db_update_file(&newdb);
				MC_CHKERR_FD(rc,fdesc);
				crypt_filestring(ctx,&newdb,hashstr);
			}
			if(!fs->is_dir){			
				fclose(fdesc);
				MC_NOTIFYEND(MC_NT_UL);
			}
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
					rc = crypt_patchfile(&cctx,path,db);
					MC_CHKERR(rc);
					if(db->is_dir){
						rrc = walk_nolocal(ctx,rpath,db->id,db->hash);
						//Hashupdate
						rc = db_update_file(db);
						MC_CHKERR(rc);
					}
					crypt_filestring(ctx,db,hashstr);
				}
			} else {
				MC_INF("Uploading deleted file " << db->id << ": " << printname(db));
				rpath.append(db->name);

				db->status = MC_FILESTAT_DELETED;
				db->mtime = time(NULL);

				rc = db_update_file(db);
				MC_CHKERR(rc);
				if(db->is_dir){					
					if(recursive){
						rrc = walk_nolocal(ctx,rpath,db->id,db->hash);
						//rrc = rc;
						
						rc = db_update_file(db);
						MC_CHKERR(rc);
						crypt_filestring(ctx,db,hashstr);

						rc = db_list_file_parent(&l,db->id);
						MC_CHKERR(rc);

						empty = true;
						lend = l.end();
						for(lit = l.begin(); lit != lend; ++lit){
							if(lit->status != MC_FILESTAT_DELETED){
								empty = false;
								break;
							}
						}
					} else {
						empty = true;
						crypt_filestring(ctx,db,hashstr);
					}					

					if(empty){
						rc = srv_delfile(db);
						MC_CHKERR(rc);
					} else { // files remain
						//Underlying handlers...?!
						//db->status = MC_FILESTAT_COMPLETE;

						//rc = db_update_file(db);
						//MC_CHKERR(rc);

						//rc = fs_touch(fpath,maxt);
						//MC_CHKERR(rc);
					}
				} else {
					crypt_filestring(ctx,db,hashstr);

					rc = srv_delfile(db);
					MC_CHKERR(rc);
				}
			}
		} else { // fs != NULL
			MC_INF("Uploading file " << db->id << ": " << printname(db));
			rpath.append(db->name);
			fpath.append(rpath);

			if(srv && (srv->mtime-fs->mtime==3600 || srv->mtime-fs->mtime==-3600))
				MC_WRN("Exact 1-hour offset, something smells wrong");

			if(fs->is_dir){
				memset(hash,0,16);
			} else {
				MC_DBGL("Opening file " << fpath << " for reading");
				MC_NOTIFYSTART(MC_NT_UL,fpath);
				fdesc = fs_fopen(fpath,"rb");
				if(!fdesc) MC_CHKERR_MSG(MC_ERR_IO,"Could not read the file");

				if((srv != NULL) && (!fs->is_dir) && (!srv->is_dir) && (fs->size == srv->size) && (srv->status == MC_FILESTAT_COMPLETE))
					rc = crypt_filemd5_known(&cctx,srv,hash,fpath,fdesc);
				else
					rc = crypt_filemd5_new(&cctx,hash,fpath,fs->size,fdesc);
				MC_CHKERR_FD(rc,fdesc);
								
			}

			if((srv != NULL) && (!fs->is_dir) && (!srv->is_dir) && (fs->size == srv->size) && (srv->status == MC_FILESTAT_COMPLETE) && (memcmp(srv->hash,hash,16) == 0)){ // File not modified
				db->mtime = fs->mtime;
				db->ctime = fs->ctime;

				MC_DBG("File contents not modified");

				rc = db_update_file(db);
				MC_CHKERR_FD(rc,fdesc);

				rc = crypt_patchfile(&cctx,path,db);
				MC_CHKERR_FD(rc,fdesc);


				//Can't get here if is_dir
				//if(fs->is_dir && recursive){
				//	rrc = walk(ctx,rpath,db->id,db->hash);
				//	//rrc = rc;
				//}
				//Hashupdate
				//rc = db_update_file(db);
				//MC_CHKERR(rc);
				crypt_filestring(ctx,db,hashstr);
			} else if(srv != NULL){
				if(fs->is_dir){
					if(!srv->is_dir){
						MC_DBG("Replacing a file with a dir");
						srv->status = MC_FILESTAT_DELETED;
						srv->mtime = time(NULL);

						rc = srv_delfile(srv);
						MC_CHKERR(rc);
					}

					db->mtime = fs->mtime;
					db->ctime = fs->ctime;
					db->size = fs->size;
					db->status = MC_FILESTAT_COMPLETE;

					rc = db_update_file(db);
					MC_CHKERR(rc);

					rc = crypt_putfile(&cctx,path,db,0,NULL);
					MC_CHKERR(rc);
					
					if(recursive){
						rrc = walk(ctx,rpath,db->id,db->hash);
						//rrc = rc;

					}
					// Hashupdate
					rc = db_update_file(db);
					MC_CHKERR(rc);
					crypt_filestring(ctx,db,hashstr);
				} else { // !fs->is_dir
					if(!srv->is_dir){
						db->mtime = fs->mtime;
						db->ctime = fs->ctime;
						db->size = fs->size;
						db->status = MC_FILESTAT_INCOMPLETE_UP;
						memcpy(db->hash,hash,16);

						rc = db_update_file(db);
						MC_CHKERR_FD(rc,fdesc);

						fseek(fdesc,0,SEEK_SET);
						rc = crypt_init_upload(&cctx,db);
						MC_CHKERR_FD(rc,fdesc);

						MC_NOTIFYPROGRESS(0,db->size);						
						MC_CHECKTERMINATING_FD(fdesc);
						if(db->size <= MC_SENDBLOCKSIZE){
							rc = crypt_putfile(&cctx,path,db,db->size,fdesc);
							MC_CHKERR_FD(rc,fdesc);
							sent = db->size;
						} else {
							rc = crypt_putfile(&cctx,path,db,MC_SENDBLOCKSIZE,fdesc);
							MC_CHKERR_FD(rc,fdesc);
							sent = MC_SENDBLOCKSIZE;
						}
						if (sent == db->size){
							// We're done
							rc = crypt_finish_upload(&cctx,db->id);
							MC_CHKERR_FD(rc,fdesc);
						} else {
							// Complete the upload
							while(sent < fs->size){
								MC_NOTIFYPROGRESS(sent,db->size);
								MC_CHECKTERMINATING_FD(fdesc);
								if(db->size-sent <= MC_SENDBLOCKSIZE){
									rc = crypt_addfile(&cctx,db->id,sent,db->size-sent,fdesc,db->hash);
									sent += db->size-sent;
								} else {
									rc = crypt_addfile(&cctx,db->id,sent,MC_SENDBLOCKSIZE,fdesc,db->hash);
									sent += MC_SENDBLOCKSIZE;
								}
								MC_CHKERR_FD(rc,fdesc);
							}							
							rc = crypt_finish_upload(&cctx,db->id);
							MC_CHKERR_FD(rc,fdesc);
						}

						db->status = MC_FILESTAT_COMPLETE;
						rc = db_update_file(db);
						MC_CHKERR_FD(rc,fdesc);
						crypt_filestring(ctx,db,hashstr);
					} else { // srv->is_dir
						MC_DBG("Replacing a dir with a file");
						db->mtime = fs->mtime;
						db->ctime = fs->ctime;
						db->size = fs->size;
						db->status = MC_FILESTAT_COMPLETE;
						db->is_dir = true;

						rc = db_update_file(db);
						MC_CHKERR_FD(rc,fdesc);

						if(recursive){
							rrc = walk_nolocal(ctx,rpath,db->id,NULL);
							//rrc = rc;

							rc = db_list_file_parent(&l,db->id);
							MC_CHKERR_FD(rc,fdesc);

							empty = true;
							lend = l.end();
							for(lit = l.begin(); lit != lend; ++lit){
								if(lit->status != MC_FILESTAT_DELETED){
									empty = false;
									break;
								}
							}
						} else {
							empty = true;
						}
						if(empty){
							rc = srv_delfile(db);
							MC_CHKERR_FD(rc,fdesc);
							
							db->status = MC_FILESTAT_INCOMPLETE_UP;
							memcpy(db->hash,hash,16);
							//db->mtime = time(NULL);

							rc = db_update_file(db);
							MC_CHKERR_FD(rc,fdesc);

							fseek(fdesc,0,SEEK_SET);
							rc = crypt_init_upload(&cctx,db);
							MC_CHKERR_FD(rc,fdesc);

							MC_NOTIFYPROGRESS(0,db->size);
							MC_CHECKTERMINATING_FD(fdesc);
							if(db->size <= MC_SENDBLOCKSIZE){
								rc = crypt_putfile(&cctx,path,db,db->size,fdesc);
								MC_CHKERR_FD(rc,fdesc);
								sent = db->size;
							} else {
								rc = crypt_putfile(&cctx,path,db,MC_SENDBLOCKSIZE,fdesc);
								MC_CHKERR_FD(rc,fdesc);
								sent = MC_SENDBLOCKSIZE;
								// Complete the upload
								while(sent < fs->size){
									MC_NOTIFYPROGRESS(sent,fs->size);
									MC_CHECKTERMINATING_FD(fdesc);
									if(db->size-sent <= MC_SENDBLOCKSIZE){
										rc = crypt_addfile(&cctx,db->id,sent,db->size-sent,fdesc,db->hash);
										sent += db->size-sent;
									} else {
										rc = crypt_addfile(&cctx,db->id,sent,MC_SENDBLOCKSIZE,fdesc,db->hash);
										sent += MC_SENDBLOCKSIZE;
									}
									MC_CHKERR_FD(rc,fdesc);
								}						
							}
							db->status = MC_FILESTAT_COMPLETE;
							rc = db_update_file(db);
							MC_CHKERR_FD(rc,fdesc);

							crypt_filestring(ctx,db,hashstr);

						} else { // files remain
							//Underlying handlers?
							//db->status = MC_FILESTAT_COMPLETE;

							//rc = db_update_file(db);
							//MC_CHKERR(rc);

							//rc = fs_touch(fpath,maxt);
							//MC_CHKERR(rc);
						}
					}
				}
			} else { // srv == NULL
				rc = db_delete_file(db->id);
				MC_CHKERR_FD(rc,fdesc);
				db->id = MC_FID_NONE;
				db->name = fs->name;
				db->ctime = fs->ctime;
				db->mtime = fs->mtime;
				db->size = fs->size;
				db->status = MC_FILESTAT_INCOMPLETE_UP;
				memcpy(db->hash,hash,16);


				if(fs->is_dir){
					rc = crypt_putfile(&cctx,path,db,0,NULL);
					MC_CHKERR(rc);

					rc = db_insert_file(db);
					MC_CHKERR(rc);

					if(recursive){
						rrc = walk(ctx,rpath,db->id,db->hash);
						//rrc = rc;
					}
				} else {
					fseek(fdesc,0,SEEK_SET);
					rc = crypt_init_upload(&cctx,db);
					MC_CHKERR_FD(rc,fdesc);
					
					MC_NOTIFYPROGRESS(0,db->size);
					MC_CHECKTERMINATING_FD(fdesc);
					if(db->size <= MC_SENDBLOCKSIZE){
						rc = crypt_putfile(&cctx,path,db,db->size,fdesc);
						sent = db->size;
					} else {
						rc = crypt_putfile(&cctx,path,db,MC_SENDBLOCKSIZE,fdesc);
						sent = MC_SENDBLOCKSIZE;
					}
					MC_CHKERR_FD(rc,fdesc);
					rc = db_insert_file(db);
					MC_CHKERR(rc);
				}

				if(fs->is_dir){
					// We're done
				} else if(sent == db->size){
					// We're done
					rc = crypt_finish_upload(&cctx,db->id);
					MC_CHKERR_FD(rc,fdesc);
				} else {
					// Complete the upload
					while(sent < fs->size){
						MC_NOTIFYPROGRESS(sent,fs->size);
						MC_CHECKTERMINATING_FD(fdesc);
						if(db->size-sent <= MC_SENDBLOCKSIZE){
							rc = crypt_addfile(&cctx,db->id,sent,db->size-sent,fdesc,db->hash);
							sent += db->size-sent;
						} else {
							rc = crypt_addfile(&cctx,db->id,sent,MC_SENDBLOCKSIZE,fdesc,db->hash);
							sent += MC_SENDBLOCKSIZE;
						}
						MC_CHKERR_FD(rc,fdesc);
					}
					rc = crypt_finish_upload(&cctx,db->id);
					MC_CHKERR_FD(rc,fdesc);
				}
				db->status = MC_FILESTAT_COMPLETE;
				rc = db_update_file(db);
				MC_CHKERR_FD(rc,fdesc);
				crypt_filestring(ctx,db,hashstr);
			}
			if(!fs->is_dir) {
				fclose(fdesc);
				MC_NOTIFYEND(MC_NT_UL);
			}
		}
	}

	//While this sometimes overwrites legit changes, generally it undoes unwanted mtime changes from
	//things we did within the directory
	if((db && db->is_dir && db->status != MC_FILESTAT_DELETED) || 
		(!db && srv && srv->status != MC_FILESTAT_DELETED) ||
		(!db && !srv && newdb.is_dir && newdb.status != MC_FILESTAT_DELETED)){
		if(db) rc = fs_touch(fpath,db->mtime,db->ctime);
		else rc = fs_touch(fpath,newdb.mtime,newdb.ctime);
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
int complete_up(mc_sync_ctx *ctx, const string& fpath, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr){
	FILE *fdesc;
	int64 sent;
	mc_crypt_ctx cctx;
	int rc;
	MC_INF("Resuming upload of file " << db->id << ": " << printname(db));
	if(fs->is_dir) MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"not to be called on dirs");
	//if(ctx->sync->crypted) MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"Can't resume crypted yet");
	init_crypt_ctx(&cctx,ctx);

	if(memcmp(db->hash,srv->hash,16)) MC_ERR_MSG(MC_ERR_VERIFY,"Hash mismatch");
	
	rc = crypt_initresume_up(&cctx,db,&sent);
	MC_CHKERR(rc);
	
	MC_DBG("Opening file " << fpath << " for reading");
	MC_NOTIFYSTART(MC_NT_UL,fpath);
	fdesc = fs_fopen(fpath,"rb");
	if(!fdesc) MC_CHKERR_MSG(MC_ERR_IO,"Could not read the file");

	fseek(fdesc,0,SEEK_END);
	if(ftell(fdesc) != db->size) MC_ERR_MSG_FD(MC_ERR_VERIFY,fdesc,"File size has changed");
	
	rc = crypt_resumetopos_up(&cctx,db,sent,fdesc);
	MC_CHKERR_FD(rc,fdesc);
	//fseek(fdesc,sent,SEEK_SET);

	if(ftell(fdesc) != sent) MC_ERR_MSG_FD(MC_ERR_VERIFY,fdesc,"Couldn't seek to offset, file must have changed");

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
		MC_CHKERR_FD(rc,fdesc);
	}

	fclose(fdesc);
	MC_NOTIFYEND(MC_NT_UL);

	srv->status = MC_FILESTAT_COMPLETE;
	rc = db_update_file(db);
	MC_CHKERR(rc);

	crypt_filestring(ctx,srv,hashstr);

	return 0;
}

/* Called by verifyandcomplete, resumes an interrupted download 
*	Should not be called on dirs	*/
int complete_down(mc_sync_ctx *ctx, const string& fpath, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr){
	FILE *fdesc;
	int64 offset,written;
	mc_crypt_ctx cctx;
	int rc;
	MC_INF("Resuming download of file " << db->id << ": " << printname(db));
	if(srv->is_dir) MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"not to be called on dirs");
	//if(ctx->sync->crypted) MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"Can't resume crypted yet");
	init_crypt_ctx(&cctx,ctx);

	if(memcmp(db->hash,srv->hash,16)) MC_ERR_MSG(MC_ERR_VERIFY,"Hash mismatch");

	rc = crypt_initresume_down(&cctx,srv);
	MC_CHKERR(rc);

	MC_DBG("Opening file " << fpath << " for writing");
	MC_NOTIFYSTART(MC_NT_DL,fpath);
	fdesc = fs_fopen(fpath,"r+b");
	if(!fdesc) MC_CHKERR_MSG(MC_ERR_IO,"Could not write the file");

	fseek(fdesc,0,SEEK_END);
	offset = ftell(fdesc);

	rc = crypt_resumetopos_down(&cctx,srv,offset,fdesc);
	MC_CHKERR_FD(rc,fdesc);

	if(offset > srv->size) MC_ERR_MSG_FD(MC_ERR_VERIFY,fdesc,"File on disk bigger than original: " << offset << "/" << srv->size);
	while(offset < srv->size){
		MC_NOTIFYPROGRESS(offset,srv->size);
		MC_CHECKTERMINATING_FD(fdesc);
		rc = crypt_getfile(&cctx,srv->id,offset,MC_RECVBLOCKSIZE,fdesc,&written,srv->hash);
		MC_CHKERR_FD(rc,fdesc);
		offset += written;
	}

	fclose(fdesc);
	MC_NOTIFYEND(MC_NT_DL);

	rc = fs_touch(fpath,srv->mtime,srv->ctime);
	MC_CHKERR(rc);

	srv->status = MC_FILESTAT_COMPLETE;
	rc = db_update_file(srv);
	MC_CHKERR(rc);	

	crypt_filestring(ctx,srv,hashstr);

	return 0;
}