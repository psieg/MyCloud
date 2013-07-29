#include "mc_walk.h"
#ifdef MC_QTCLIENT
#	include "qtClient.h"
#else
#	include "mc_workerthread.h"
#endif


/* These functions choose or ask what to do with files that are not clearly up/down */

/* Ask the user which of the conflicting versions he wants to keep, recommend doubtaction
*	db may be NULL	*/
int conflicted(mc_sync_ctx *ctx, const string& path, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr, MC_CONFLICTRECOMMENDATION doubtaction){
	string p,fpath;
	mc_crypt_ctx cctx;
	unsigned char chkhash[16];
	int rc;
	MC_CONFLICTACTION act = MC_CONFLICTACT_UNKNOWN;
	bool actR = false, actD = false;
	MC_INFL("Found conflict at file " << srv->id << ": " << printname(srv));
	fpath.assign(ctx->sync->path).append(path).append(fs->name);

	// check for previous decision
	if(ctx->dirconflictact != MC_CONFLICTACT_UNKNOWN) act = ctx->dirconflictact;

	// check for "easy" cases
	if(act == MC_CONFLICTACT_UNKNOWN){
		if(!fs->is_dir && !srv->is_dir && fs->size == srv->size && srv->status == MC_FILESTAT_COMPLETE){
			init_crypt_ctx(&cctx,ctx);
			rc = crypt_filemd5_known(&cctx,srv,chkhash,fpath); //fs_filemd5(chkhash,fpath,fs->size);
			MC_CHKERR(rc);
			if(memcmp(chkhash,srv->hash,16) == 0){
				MC_DBG("File contents not modified");
				if(fs->mtime <= srv->mtime)
					return download(ctx,path,fs,db,srv,hashstr,true,&cctx);
				else
					return upload(ctx,path,fs,db,srv,hashstr,srv->parent,true,&cctx);
			}
		} else if(fs->is_dir && srv->is_dir && ((db && db->status == srv->status) || srv->status == MC_FILESTAT_COMPLETE)){ //can only be mtime diff
				MC_DBG("Directory mtime/hash mismatch");
				if(fs->mtime <= srv->mtime)
					act = MC_CONFLICTACT_DOWN;
					//return download(ctx,path,fs,db,srv,hashstr);
				else
					act = MC_CONFLICTACT_UP;
					//return upload(ctx,path,fs,db,srv,hashstr,srv->parent);
		}
	}

	// ask user
	if(act == MC_CONFLICTACT_UNKNOWN){
#ifdef MC_QTCLIENT
		p.assign(ctx->sync->path).append(path).append(fs->name);
		ostringstream local,server;
		local << "Name: " << fs->name.substr(0,MC_QTCONFLICTMAXLEN) << "\n";
		if(fs->is_dir) local << "Directory\n";
		else local << "Size: " << BytesToSize(fs->size) << "\n";
		local << "Last Modification: " << TimeToString(fs->mtime) << "\n";
		if(db) local << "Last Synchronization: " << TimeToString(db->mtime);
		server << "Name: " << srv->name.substr(0,MC_QTCONFLICTMAXLEN) << "\n";
		if(srv->status == MC_FILESTAT_DELETED) server << "deleted\n";
		if(srv->is_dir) server << "Directory\n";
		else server << "Size: " << BytesToSize(srv->size) << "\n";
		if(srv->status == MC_FILESTAT_DELETED) server << "Deleted at: " << TimeToString(srv->mtime);
		else server << "Last Modification: " << TimeToString(srv->mtime);

		rc = QtClient::execConflictDialog(&p,&local.str(),&server.str(),doubtaction,fs->is_dir || srv->is_dir,true);
		switch(rc){
			case QtConflictDialog::Download:
			case QtConflictDialog::DownloadD:
			case QtConflictDialog::DownloadDR:
				act = MC_CONFLICTACT_DOWN;
				break;
			case QtConflictDialog::Upload:
			case QtConflictDialog::UploadD:
			case QtConflictDialog::UploadDR:
				act = MC_CONFLICTACT_UP;
				break;
			case QtConflictDialog::Keep:
			case QtConflictDialog::KeepD:
				act = MC_CONFLICTACT_KEEP;
				break;
			case QtConflictDialog::Skip:
			case QtConflictDialog::SkipD:
				act = MC_CONFLICTACT_SKIP;
				break;
			case QtConflictDialog::Terminating:
				MC_CHECKTERMINATING();
			default:
				MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"Unexpected ConflictDialog return value: " << rc);
		}
		if(rc & 16) {
			actD = true;
			if(rc & 32) actR = true;
		}
#else
		char choice;

		cout << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;
		cout << "A conflict was detected between the following files:" << endl;
		cout << "Local filesystem:" << endl;
		cout << fs->name << " (" << fs->size << " bytes)" << endl << "Last modification: " << TimeToString(fs->mtime) << endl;
		cout << "Server:" << endl;
		if(srv->status != MC_FILESTAT_DELETED)
			cout << srv->name << " (" << srv->size << " bytes)" << endl << "Last modification: " << TimeToString(srv->mtime) << endl;
		else
			cout << srv->name << " (deleted, last had " << srv->size << " bytes)" << endl << "Deleted at: " << TimeToString(srv->mtime) << endl;
		cout << "Type 1 for the local version, 2 for the remote version, 3 to solve manually or 4 to skip" << endl;
		if(doubtaction == MC_CONFLICTREC_UP) cout << "If you don't know choose 1 to upload the local version" << endl;
		else if(doubtaction == MC_CONFLICTREC_DOWN) cout << "If you don't know choose 2 to download the server version" << endl;


		while(true){
			cin >> choice;
			if(atoi(&choice) == 1){
				cout << "Uploading the local version" << endl;
				act = MC_CONFLICTACT_UP;
			} else if(atoi(&choice) == 2){
				cout << "Downloading the server version" << endl;
				act = MC_CONFLICTACT_DOWN;
			} else if(atoi(&choice) == 3){
				act = MC_CONFLICTACT_KEEP;
			} else if(atoi(&choice) == 4){
				cout << "Skipping" << endl;
				act = MC_CONFLICTACT_SKIP;
			}
		}
#endif
	}

	if(actR) ctx->rconflictact = true;
	if(actD) ctx->dirconflictact = act; //Will be undone in walk
	switch(act){
		case MC_CONFLICTACT_DOWN:
			rc = download(ctx,path,fs,db,srv,hashstr);
			break;
		case MC_CONFLICTACT_UP:
			rc = upload(ctx,path,fs,db,srv,hashstr,srv->parent);
			break;
		case MC_CONFLICTACT_KEEP:
			rc = fs_rename(p,AddConflictExtension(p));
			MC_CHKERR(rc);
			rc = download(ctx,path,NULL,db,srv,hashstr);
			break;
		case MC_CONFLICTACT_SKIP:
			rc = MC_ERR_CONFLICT_SKIP;
			break;
		default:
			MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"Unexpected ACT value: " << act);
	}
	return rc;
}
/* Ask ...
*	db may be null	*/
int conflicted_nolocal(mc_sync_ctx *ctx, const string& path, mc_file *db, mc_file *srv, string *hashstr){	
	list<mc_file> tree;
	list<string> ptree;
	list<mc_file>::iterator tbegin,tit,tend;
	list<string>::iterator pit;
	mc_file parent,srvdummy;
	string p,fp;
	int rc;
	MC_CONFLICTACTION act = MC_CONFLICTACT_UNKNOWN;
	bool up, actR = false, actD = false;
	MC_INFL("Found conflict at file " << srv->id << ": " << printname(srv));

	// check for previous decision
	if(ctx->dirconflictact != MC_CONFLICTACT_UNKNOWN) act = ctx->dirconflictact;
	if(act == MC_CONFLICTACT_KEEP) act = MC_CONFLICTACT_UNKNOWN; // can't keep, this is nolocal

	// check for "easy" cases
	if(act == MC_CONFLICTACT_UNKNOWN){
		if(db && db->status == MC_FILESTAT_DELETED && srv->status == MC_FILESTAT_DELETED){
			MC_DBG("Mtime/hash mismatch on deleted file");
			if(db->mtime <= srv->mtime)
				act = MC_CONFLICTACT_DOWN;
				//return download(ctx,path,NULL,db,srv,hashstr);
			else
				act = MC_CONFLICTACT_UP;
				//return upload(ctx,path,NULL,db,srv,hashstr,srv->parent);
		}
	}

	if(act == MC_CONFLICTACT_UNKNOWN){
#ifdef MC_QTCLIENT
		p.assign(ctx->sync->path).append(path).append(srv->name);
		ostringstream local,server;
		if(db){
			local << "Name: " << db->name.substr(0,MC_QTCONFLICTMAXLEN) << "\n";
			local << "deleted\n";
			if(db->is_dir) local << "Directory\n";
			else local << "Size: " << BytesToSize(db->size) << "\n";
			local << "Last known Modification: " << TimeToString(db->mtime);
		} else {
			local << "File never seen on local filesystem\nParent directory is deleted";
		}
		server << "Name: " << srv->name.substr(0,MC_QTCONFLICTMAXLEN) << "\n";
		if(srv->status == MC_FILESTAT_DELETED) server << "deleted\n";
		if(srv->is_dir) server << "Directory\n";
		else server << "Size: " << BytesToSize(srv->size) << "\n";
		if(srv->status == MC_FILESTAT_DELETED) server << "Deleted at: " << TimeToString(srv->mtime);
		else server << "Last Modification: " << TimeToString(srv->mtime);

		rc = QtClient::execConflictDialog(&p,&local.str(),&server.str(),0,(db && db->is_dir) || srv->is_dir,false);
		switch(rc){
			case QtConflictDialog::Download:	
			case QtConflictDialog::DownloadD:
			case QtConflictDialog::DownloadDR:	
				act = MC_CONFLICTACT_DOWN;
				break;
			case QtConflictDialog::Upload:
			case QtConflictDialog::UploadD:
			case QtConflictDialog::UploadDR:
				act = MC_CONFLICTACT_UP;
				break;
			case QtConflictDialog::Skip:
			case QtConflictDialog::SkipD:
				act = MC_CONFLICTACT_SKIP;
				break;
			case QtConflictDialog::Terminating:
				MC_CHECKTERMINATING();
			default:
				MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"Unexpected ConflictDialog return value: " << rc);
		}		
		if(rc & 16) {
			actD = true;
			if(rc & 32) actR = true;
		}
#else
		char choice;
		cout << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;
		cout << "A conflict was detected between the following files:" << endl;
		cout << "Local filesystem:" << endl;
		if(db)
			cout << db->name << " (deleted, last had " << db->size << " bytes)" << endl << "Last known modification: " << TimeToString(db->mtime) << endl;
		else
			cout << "File never seen on local filesystem" << endl;
		cout << "Server:" << endl;
		if(srv->status != MC_FILESTAT_DELETED)
			cout << srv->name << " (" << srv->size << " bytes)" << endl << "Last Modification: " << TimeToString(srv->mtime) << endl;
		else
			cout << srv->name << " (deleted, last had " << srv->size << " bytes)" << endl << "Last seen at: " << TimeToString(srv->mtime) << endl;
		cout << "Type 1 for the local version, 2 for the remote version or 4 to skip" << endl;
		cout << "As the local directory is deleted, you probably want to choose 1 to delete the file" << endl;


		while(true){
			cin >> choice;
			if(atoi(&choice) == 1){
				cout << "Uploading the local version" << endl;
				act = MC_CONFLICTACT_UP;
				break;
			} else if(atoi(&choice) == 2){
				cout << "Downloading the server version" << endl;
				act = MC_CONFLICTACT_DOWN;
				break;
			} else if(atoi(&choice) == 4){
				cout << "Skipping" << endl;
				act = MC_CONFLICTACT_SKIP;
				break;
			}
		}
#endif
	}
	
	if(actR) ctx->rconflictact = true;
	if(actD) ctx->dirconflictact = act; //Will be undone in walk
	switch(act){
		case MC_CONFLICTACT_DOWN:
			if(srv->status != MC_FILESTAT_DELETED){ //restore previous path
				fp.assign(ctx->sync->path).append(path).append(srv->name);
				p.assign(path);
				parent.id = srv->parent;
				while(parent.id > 0){
					rc = db_select_file_id(&parent);
					MC_CHKERR(rc);
					p = p.substr(0,p.length()-1);
					p.assign(p.substr(0,p.find_last_of("/")+1));
					fp = fp.substr(0,fp.length()-1);
					fp.assign(fp.substr(0,fp.find_last_of("/")+1));
					if(fs_exists(fp)) break;
					tree.push_back(parent);
					ptree.push_back(p);
					parent.id = parent.parent;
				}

				tree.reverse();
				ptree.reverse();
				tbegin = tree.begin();
				tend = tree.end();
				tit = tbegin;
				pit = ptree.begin();
				while(tit != tend){
					rc = srv_getmeta(tit->id,&srvdummy);
					MC_CHKERR(rc);
					rc = crypt_translate_fromsrv(ctx,path,&srvdummy);
					MC_CHKERR(rc);
					rc = download(ctx,*pit,NULL,&*tit,&srvdummy,hashstr,false);
					MC_CHKERR(rc);
					++tit;
					++pit;
				}
			}
			rc = download(ctx,path,NULL,db,srv,hashstr);
			break;
		case MC_CONFLICTACT_UP:
			rc = upload(ctx,path,NULL,db,srv,hashstr);
			break;
		case MC_CONFLICTACT_SKIP:
			rc = MC_ERR_CONFLICT_SKIP;
			break;
		default:
			MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"Unexpected ACT value: " << act);
	}
	return rc;
}

/* Ask ...
*	srv must be NULL or srv->status must be deleted	*/
int conflicted_noremote(mc_sync_ctx *ctx, const string& path, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr){
	list<mc_file> tree;
	list<string> ptree;
	list<mc_file>::iterator tbegin,tit,tend;
	list<string>::iterator pit;
	mc_file parent,dbdummy;
	mc_file_fs fsdummy;
	string p;
	int rc;
	MC_CONFLICTACTION act = MC_CONFLICTACT_UNKNOWN;
	bool up, actR = false, actD = false;
	if(srv) { MC_INFL("Found conflict at file " << srv->id << ": " << printname(srv)) }
	else { MC_INFL("Found conflict at file ?: " << printname(fs)); }

	//check for previous
	if(ctx->dirconflictact != MC_CONFLICTACT_UNKNOWN) act = ctx->dirconflictact;
	if(act == MC_CONFLICTACT_KEEP) act = MC_CONFLICTACT_UNKNOWN; // can't keep, this is noremote

	if(act == MC_CONFLICTACT_UNKNOWN){
#ifdef MC_QTCLIENT
		p.assign(ctx->sync->path).append(path).append(fs->name);
		ostringstream local,server;
		local << "Name: " << fs->name.substr(0,MC_QTCONFLICTMAXLEN) << "\n";
		if(fs->is_dir) local << "Directory\n";
		else local << "Size: " << BytesToSize(fs->size) << "\n";
		local << "Last Modification: " << TimeToString(fs->mtime);
		if(srv){
			server << "Name: " << srv->name.substr(0,MC_QTCONFLICTMAXLEN) << "\n";
			if(srv->status == MC_FILESTAT_DELETED) server << "deleted\n";
			if(srv->is_dir) server << "Directory\n";
			else server << "Size: " << BytesToSize(srv->size) << "\n";
			if(srv->status == MC_FILESTAT_DELETED) server << "Deleted at: " << TimeToString(srv->mtime);
			else server << "Last Modification: " << TimeToString(srv->mtime);
		} else {
			server << "File never seen on Server\nParent directory is deleted";
		}
	
		rc = QtClient::execConflictDialog(&p,&local.str(),&server.str(),0,fs->is_dir || (srv && srv->is_dir),false);
		switch(rc){
			case QtConflictDialog::Download:	
			case QtConflictDialog::DownloadD:	
			case QtConflictDialog::DownloadDR:	
				act = MC_CONFLICTACT_DOWN;
				break;
			case QtConflictDialog::Upload:
			case QtConflictDialog::UploadD:
			case QtConflictDialog::UploadDR:
				act = MC_CONFLICTACT_UP;
				break;
			case QtConflictDialog::Skip:
			case QtConflictDialog::SkipD:
				act = MC_CONFLICTACT_SKIP;
				break;
			case QtConflictDialog::Terminating:
				MC_CHECKTERMINATING();
			default:
				MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"Unexpected ConflictDialog return value: " << rc);
		}		
		if(rc & 16) {
			actD = true;
			if(rc & 32) actR = true;
		}
#else
		char choice;
		cout << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;
		cout << "A conflict was detected between the following files:" << endl;
		cout << "Local filesystem:" << endl;
		cout << fs->name << " (" << fs->size << " bytes)" << endl << "Last modification: " << TimeToString(fs->mtime) << endl;
		cout << "Server:" << endl;	
		if(srv)
			cout << srv->name << " (deleted, last had " << srv->size << " bytes)" << endl << "Last known modification: " << TimeToString(srv->mtime) << endl;
		else
			cout << "File never seen on server" << endl;
		cout << "Type 1 for the local version, 2 for the remote version or 4 to skip" << endl;
		cout << "As the local directory is deleted, you probably want to choose 1 to delete the file" << endl;


		while(true){
			cin >> choice;
			if(atoi(&choice) == 1){
				cout << "Uploading the local version" << endl;
				act = MC_CONFLICTACT_UP;
				break;
			} else if(atoi(&choice) == 2){
				cout << "Downloading the server version" << endl;
				act = MC_CONFLICTACT_DOWN;
				break;
			} else if(atoi(&choice) == 4){
				cout << "Skipping" << endl;
				act = MC_CONFLICTACT_SKIP;
				break;
			}
		}
#endif
	}
	
	if(actR) ctx->rconflictact = true;
	if(actD) ctx->dirconflictact = act; //Will be undone in walk
	switch(act){
		case MC_CONFLICTACT_DOWN:	
			rc = download(ctx,path,fs,db,srv,hashstr);
			break;
		case MC_CONFLICTACT_UP:
			p.assign(path);
			parent.id = db->parent;
			while(parent.id > 0){
				//rc = db_select_file_id(&parent);
				rc = srv_getmeta(parent.id,&parent);
				MC_CHKERR(rc);
				rc = crypt_translate_fromsrv(ctx,path,&parent);
				MC_CHKERR(rc);
				p = p.substr(0,p.length()-1);
				p.assign(p.substr(0,p.find_last_of("/")+1));
				if(parent.status != MC_FILESTAT_DELETED) break;
				tree.push_back(parent);
				ptree.push_back(p);
				parent.id = parent.parent;
			}

			tree.reverse();
			ptree.reverse();
			tbegin = tree.begin();
			tend = tree.end();
			tit = tbegin;
			pit = ptree.begin();
			while(tit != tend){
				dbdummy.id = tit->id;
				rc = db_select_file_id(&dbdummy);
				MC_CHKERR(rc);
				rc = fs_filestats(&fsdummy,pit->c_str(),tit->name.c_str());
				MC_CHKERR(rc);
				rc = upload(ctx,*pit,&fsdummy,&dbdummy,&*tit,false);
				MC_CHKERR(rc);
				++tit;
				++pit;
			}
			rc = upload(ctx,path,fs,db,srv,hashstr);
			break;
		case MC_CONFLICTACT_SKIP:
			rc = MC_ERR_CONFLICT_SKIP;
			break;
		default:
			MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"Unexpected ACT value: " << act);
	}
	return rc;
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
				rc = db_insert_file(srv);
				MC_CHKERR(rc);


				return complete_up(ctx,fpath,fs,srv,srv,hashstr);
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
			if(srv->status != MC_FILESTAT_DELETED)
				return upload(ctx,path,fs,db,srv,hashstr);
			else if(db->status != MC_FILESTAT_DELETED)
				return download(ctx,path,fs,db,srv,hashstr);
			else if(db->size == srv->size && memcmp(db->hash,srv->hash,16) == 0)
				{}//Nothing to do
			else
				return conflicted_nolocal(ctx,path,db,srv,hashstr);
		} else {
			if((fs->is_dir != srv->is_dir) || (srv->is_dir != db->is_dir))
				return conflicted(ctx,path,fs,db,srv,hashstr,MC_CONFLICTREC_DONTKNOW);
			if(db->status == MC_FILESTAT_COMPLETE){
				if(srv->status == MC_FILESTAT_COMPLETE){
					if(db->size == srv->size && srv->size == fs->size && memcmp(db->hash,srv->hash,16) == 0)
						{}//Nothing to do
					else
						return conflicted(ctx,path,fs,db,srv,hashstr,MC_CONFLICTREC_DONTKNOW);
				} else if(srv->status == MC_FILESTAT_INCOMPLETE_UP){
					//Shouldn't actually happen: The file was complete but is incomplete now, although its the same file?
					//Continue upload
					return complete_up(ctx,fpath,fs,srv,srv,hashstr);
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
					return complete_down(ctx,fpath,fs,db,srv,hashstr);
				} else if(srv->status == MC_FILESTAT_INCOMPLETE_UP) {
					//Since we don't start downloading incomplete uploads, this shouldn't happen either
					MC_INFL("Not downloading file " << srv->id << ": " << srv->name << ", file is not complete");
					return MC_ERR_INCOMPLETE_SKIP;
				} else { //srv->status == MC_FILESTAT_DELETED
					return conflicted(ctx,path,fs,db,srv,hashstr,MC_CONFLICTREC_DONTKNOW); 
				}
			} else { //db->status == MC_FILESTAT_INCOMPLETE_UP
				if(srv->status == MC_FILESTAT_COMPLETE){
					//Someone else finished the upload
					db->status = MC_FILESTAT_COMPLETE;
					rc = db_update_file(srv);
					MC_CHKERR(rc);
				} else if(srv->status == MC_FILESTAT_INCOMPLETE_UP){
					//Continue upload
					return complete_up(ctx,fpath,fs,db,srv,hashstr);
				} else {  //srv->status == MC_FILESTAT_DELETED
					return conflicted(ctx,path,fs,db,srv,hashstr,MC_CONFLICTREC_DONTKNOW);
				}
			}
		}
	}
	//Execution reaches this point if nothing was to be done, thus we check the children
	if(srv->is_dir){
		if(db) memcpy(orighash,db->hash,16); else memcpy(orighash,srv->hash,16); //This catches the "EVIL" case from nochange where srv and db point to the same data
		if(db && memcmp(db->hash,srv->hash,16) == 0){
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
	MC_INFL("Walking directory " << id << ": " << path);
	//MC_NOTIFYSTART(MC_NT_WALKTEST,path);
	MC_CHECKTERMINATING();
	//if(id == 2698)
		//MC_WRN("HIT");


	rc = fs_listdir(&onfs, fpath);
	MC_CHKERR(rc);

	rc = db_list_file_parent(&indb,id);
	MC_CHKERR(rc);

	rc  = srv_listdir(&onsrv,id);
	MC_CHKERR(rc);

	rc = crypt_translatelist_fromsrv(ctx,path,&onsrv);
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
				// File has been deleted
				if(onsrvit->mtime == indbit->mtime){
					rc = verifyandcomplete(ctx,path,NULL,&*indbit,&*onsrvit,&hashstr);
					//rc = upload(ctx,path,NULL,&*indbit,&*onsrvit,&hashstr);
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
							rc = complete_down(ctx,spath,&*onfsit,&*indbit,&*onsrvit,&hashstr);
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
	mc_file srv;
	string hashstr = "";
	list<string> hashes;
	list<string>::iterator hashesit,hashesend;
	MC_CONFLICTACTION outerconflictact = ctx->dirconflictact; //must be saved in case upper layers used it...
	string fpath = ctx->sync->path;
	if(path.size()) path.append("/");
	fpath.append(path);
	MC_INFL("Walking directory " << id << ": " << path);
	//MC_NOTIFYSTART(MC_NT_WALKTEST,path);
	MC_CHECKTERMINATING();
	//if(id == 2698)
		//MC_WRN("HIT");


	rc = fs_listdir(&onfs,fpath);
	MC_CHKERR(rc);

	rc = db_list_file_parent(&indb,id);
	MC_CHKERR(rc);

	onfs.sort(compare_mc_file_fs);
	indb.sort(compare_mc_file);
	
	rc = filter_lists(path,&onfs,&indb,&list<mc_file>(indb),ctx->filter);
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
			// File has been deleted
			rc = verifyandcomplete(ctx,path,NULL,&*indbit,&*indbit,&hashstr);
			if(MC_IS_CRITICAL_ERR(rc)) return rc; else if(rc) clean = false;
			++indbit;
		} else if(onfsit->name == indbit->name){
			// Standard: File known to all, check modified
			if(indbit->status == MC_FILESTAT_INCOMPLETE_DOWN){
				rc = srv_getmeta(indbit->id,&srv); //actually just because we need to have srv->status == COMPLETE (or, what shouldn't be INCOMPLETE_UP)
				MC_CHKERR(rc);
				rc = crypt_translate_fromsrv(ctx,path,&srv);
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
	MC_INFL("Walking directory " << id << ": " << path);
	//MC_NOTIFYSTART(MC_NT_WALKTEST,path);
	MC_CHECKTERMINATING();

	rc = db_list_file_parent(&indb,id);
	MC_CHKERR(rc);

	rc  = srv_listdir(&onsrv,id);
	MC_CHKERR(rc);

	rc = crypt_translatelist_fromsrv(ctx,path,&onsrv);
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
	MC_INFL("Walking directory " << id << ": " << path);
	//MC_NOTIFYSTART(MC_NT_WALKTEST,path);
	MC_CHECKTERMINATING();


	rc = fs_listdir(&onfs,fpath);
	MC_CHKERR(rc);

	rc = db_list_file_parent(&indb,id);
	MC_CHKERR(rc);

	rc  = srv_listdir(&onsrv,id);
	MC_CHKERR(rc);

	rc = crypt_translatelist_fromsrv(ctx,path,&onsrv);
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