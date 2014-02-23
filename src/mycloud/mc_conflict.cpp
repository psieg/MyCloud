#include "mc_conflict.h"

#include "mc_crypt.h"
#include "mc_transfer.h"
#ifdef MC_QTCLIENT
#	include "qtClient.h"
#else
#	include "mc_workerthread.h"
#endif


#include <sstream>

/* Ask the user which of the conflicting versions he wants to keep, recommend doubtaction
*	db may be NULL	*/
int conflicted(mc_sync_ctx *ctx, const string& path, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr, MC_CONFLICTRECOMMENDATION doubtaction){
	Q_ASSERT(fs != NULL);
	Q_ASSERT(db != NULL);
	string p, fpath;
	mc_crypt_ctx cctx;
	unsigned char chkhash[16];
	int rc;
	MC_CONFLICTACTION act = MC_CONFLICTACT_UNKNOWN;
	bool actR = false, actD = false;
	MC_INFL("Found conflict at file " << srv->id << ": " << printname(srv));
	fpath.assign(ctx->sync->path).append(path).append(fs->name);

	// check for previous decision
	if (ctx->dirconflictact != MC_CONFLICTACT_UNKNOWN) act = ctx->dirconflictact;

	// check for "easy" cases
	if (act == MC_CONFLICTACT_UNKNOWN){
		if (!fs->is_dir && !srv->is_dir && fs->size == srv->size && srv->status == MC_FILESTAT_COMPLETE){
			init_crypt_ctx(&cctx, ctx);
			rc = crypt_filemd5_known(&cctx, srv, chkhash, fpath); //fs_filemd5(chkhash,fpath,fs->size);
			MC_CHKERR(rc);
			if (memcmp(chkhash, srv->hash, 16) == 0){
				MC_DBG("File contents not modified");
				if (fs->mtime <= srv->mtime)
					return download(ctx, path, fs, db, srv, hashstr, true, &cctx);
				else
					return upload(ctx, path, fs, db, srv, hashstr, srv->parent, true, &cctx);
			}
		}
		else if (fs->is_dir && srv->is_dir && ((db && db->status == srv->status) || srv->status == MC_FILESTAT_COMPLETE)){ //can only be mtime diff
			MC_DBG("Directory mtime/hash mismatch");
			if (fs->mtime <= srv->mtime)
				act = MC_CONFLICTACT_DOWN;
			//return download(ctx,path,fs,db,srv,hashstr);
			else
				act = MC_CONFLICTACT_UP;
			//return upload(ctx,path,fs,db,srv,hashstr,srv->parent);
		}
	}

	// ask user
	if (act == MC_CONFLICTACT_UNKNOWN){
#ifdef MC_QTCLIENT
		p.assign(ctx->sync->path).append(path).append(fs->name);
		ostringstream local, server;
		string localstr, serverstr;
		local << "Name: " << shortname(fs->name, MC_QTCONFLICTMAXLEN) << "\n";
		if (fs->is_dir) local << "Directory\n";
		else local << "Size: " << BytesToSize(fs->size) << "\n";
		local << "Last Modification: " << TimeToString(fs->mtime) << "\n";
		if (db) local << "Last Synchronization: " << TimeToString(db->mtime);
		server << "Name: " << shortname(srv->name, MC_QTCONFLICTMAXLEN) << "\n";
		if (srv->status == MC_FILESTAT_DELETED) server << "deleted\n";
		if (srv->is_dir) server << "Directory\n";
		else server << "Size: " << BytesToSize(srv->size) << "\n";
		if (srv->status == MC_FILESTAT_DELETED) server << "Deleted at: " << TimeToString(srv->mtime);
		else server << "Last Modification: " << TimeToString(srv->mtime);

		localstr = local.str();
		serverstr = server.str();
		rc = QtClient::execConflictDialog(&p, &localstr, &serverstr, doubtaction, true);
		switch (rc){
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
			MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED, "Unexpected ConflictDialog return value: " << rc);
		}
		if (rc & 16) {
			actD = true;
			if (rc & 32) actR = true;
		}
#else
		char choice;

		cout << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;
		cout << "A conflict was detected between the following files:" << endl;
		cout << "Local filesystem:" << endl;
		cout << fs->name << " (" << fs->size << " bytes)" << endl << "Last modification: " << TimeToString(fs->mtime) << endl;
		cout << "Server:" << endl;
		if (srv->status != MC_FILESTAT_DELETED)
			cout << srv->name << " (" << srv->size << " bytes)" << endl << "Last modification: " << TimeToString(srv->mtime) << endl;
		else
			cout << srv->name << " (deleted, last had " << srv->size << " bytes)" << endl << "Deleted at: " << TimeToString(srv->mtime) << endl;
		cout << "Type 1 for the local version, 2 for the remote version, 3 to solve manually or 4 to skip" << endl;
		if (doubtaction == MC_CONFLICTREC_UP) cout << "If you don't know choose 1 to upload the local version" << endl;
		else if (doubtaction == MC_CONFLICTREC_DOWN) cout << "If you don't know choose 2 to download the server version" << endl;


		while (true){
			cin >> choice;
			if (atoi(&choice) == 1){
				cout << "Uploading the local version" << endl;
				act = MC_CONFLICTACT_UP;
			}
			else if (atoi(&choice) == 2){
				cout << "Downloading the server version" << endl;
				act = MC_CONFLICTACT_DOWN;
			}
			else if (atoi(&choice) == 3){
				act = MC_CONFLICTACT_KEEP;
			}
			else if (atoi(&choice) == 4){
				cout << "Skipping" << endl;
				act = MC_CONFLICTACT_SKIP;
			}
		}
#endif
	}

	if (actR) ctx->rconflictact = true;
	if (actD) ctx->dirconflictact = act; //Will be undone in walk
	switch (act){
	case MC_CONFLICTACT_DOWN:
		rc = download(ctx, path, fs, db, srv, hashstr);
		break;
	case MC_CONFLICTACT_UP:
		rc = upload(ctx, path, fs, db, srv, hashstr, srv->parent);
		break;
	case MC_CONFLICTACT_KEEP:
		rc = fs_rename(p, AddConflictExtension(p));
		MC_CHKERR(rc);
		rc = download(ctx, path, NULL, db, srv, hashstr);
		break;
	case MC_CONFLICTACT_SKIP:
		rc = MC_ERR_CONFLICT_SKIP;
		break;
	default:
		MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED, "Unexpected ACT value: " << act);
	}
	return rc;
}
/* Ask ...
*	db may be null	*/
int conflicted_nolocal(mc_sync_ctx *ctx, const string& path, mc_file *db, mc_file *srv, string *hashstr){
	Q_ASSERT(srv != NULL);
	list<mc_file> tree;
	list<string> ptree;
	list<mc_file>::iterator tbegin, tit, tend;
	list<string>::iterator pit;
	mc_file parent, srvdummy;
	string p, fp;
	int rc;
	MC_CONFLICTACTION act = MC_CONFLICTACT_UNKNOWN;
	bool actR = false, actD = false;
	MC_INFL("Found conflict at file " << srv->id << ": " << printname(srv));

	// check for previous decision
	if (ctx->dirconflictact != MC_CONFLICTACT_UNKNOWN) act = ctx->dirconflictact;
	if (act == MC_CONFLICTACT_KEEP) act = MC_CONFLICTACT_UNKNOWN; // can't keep, this is nolocal

	// check for "easy" cases
	if (act == MC_CONFLICTACT_UNKNOWN){
		if (db && srv->status == MC_FILESTAT_DELETED){
			MC_DBG("Mtime/hash mismatch on deleted file");
			if (db->mtime <= srv->mtime)
				act = MC_CONFLICTACT_DOWN;
			//return download(ctx,path,NULL,db,srv,hashstr);
			else
				act = MC_CONFLICTACT_UP;
			//return upload(ctx,path,NULL,db,srv,hashstr,srv->parent);
		}
	}

	if (act == MC_CONFLICTACT_UNKNOWN){
#ifdef MC_QTCLIENT
		p.assign(ctx->sync->path).append(path).append(srv->name);
		ostringstream local, server;
		string localstr, serverstr;
		if (db){
			local << "Name: " << shortname(db->name, MC_QTCONFLICTMAXLEN) << "\n";
			local << "deleted\n";
			if (db->is_dir) local << "Directory\n";
			else local << "Size: " << BytesToSize(db->size) << "\n";
			local << "Last known Modification: " << TimeToString(db->mtime);
		}
		else {
			local << "File never seen on local filesystem\nParent directory is deleted";
		}
		server << "Name: " << shortname(srv->name, MC_QTCONFLICTMAXLEN) << "\n";
		if (srv->status == MC_FILESTAT_DELETED) server << "deleted\n";
		if (srv->is_dir) server << "Directory\n";
		else server << "Size: " << BytesToSize(srv->size) << "\n";
		if (srv->status == MC_FILESTAT_DELETED) server << "Deleted at: " << TimeToString(srv->mtime);
		else server << "Last Modification: " << TimeToString(srv->mtime);

		localstr = local.str();
		serverstr = server.str();
		rc = QtClient::execConflictDialog(&p, &localstr, &serverstr, 0, false);
		switch (rc){
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
			MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED, "Unexpected ConflictDialog return value: " << rc);
		}
		if (rc & 16) {
			actD = true;
			if (rc & 32) actR = true;
		}
#else
		char choice;
		cout << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;
		cout << "A conflict was detected between the following files:" << endl;
		cout << "Local filesystem:" << endl;
		if (db)
			cout << db->name << " (deleted, last had " << db->size << " bytes)" << endl << "Last known modification: " << TimeToString(db->mtime) << endl;
		else
			cout << "File never seen on local filesystem" << endl;
		cout << "Server:" << endl;
		if (srv->status != MC_FILESTAT_DELETED)
			cout << srv->name << " (" << srv->size << " bytes)" << endl << "Last Modification: " << TimeToString(srv->mtime) << endl;
		else
			cout << srv->name << " (deleted, last had " << srv->size << " bytes)" << endl << "Last seen at: " << TimeToString(srv->mtime) << endl;
		cout << "Type 1 for the local version, 2 for the remote version or 4 to skip" << endl;
		cout << "As the local directory is deleted, you probably want to choose 1 to delete the file" << endl;


		while (true){
			cin >> choice;
			if (atoi(&choice) == 1){
				cout << "Uploading the local version" << endl;
				act = MC_CONFLICTACT_UP;
				break;
			}
			else if (atoi(&choice) == 2){
				cout << "Downloading the server version" << endl;
				act = MC_CONFLICTACT_DOWN;
				break;
			}
			else if (atoi(&choice) == 4){
				cout << "Skipping" << endl;
				act = MC_CONFLICTACT_SKIP;
				break;
			}
		}
#endif
	}

	if (actR) ctx->rconflictact = true;
	if (actD) ctx->dirconflictact = act; //Will be undone in walk
	switch (act){
	case MC_CONFLICTACT_DOWN:
		if (srv->status != MC_FILESTAT_DELETED){ //restore previous path
			fp.assign(ctx->sync->path).append(path);
			p.assign(path);
			parent.id = srv->parent;
			while (parent.id > 0){
				rc = db_select_file_id(&parent);
				MC_CHKERR(rc);
				if (fs_exists(fp)) break;
				p = p.substr(0, p.length() - 1);
				p.assign(p.substr(0, p.find_last_of("/") + 1));
				fp = fp.substr(0, fp.length() - 1);
				fp.assign(fp.substr(0, fp.find_last_of("/") + 1));
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
			while (tit != tend){
				rc = srv_getmeta(tit->id, &srvdummy);
				MC_CHKERR(rc);
				rc = crypt_file_fromsrv(ctx, p, &srvdummy);
				MC_CHKERR(rc);
				rc = download(ctx, *pit, NULL, &*tit, &srvdummy, hashstr, false);
				MC_CHKERR(rc);
				p.append(srvdummy.name).append("/");
				++tit;
				++pit;
			}
		}
		rc = download(ctx, path, NULL, db, srv, hashstr);
		break;
	case MC_CONFLICTACT_UP:
		rc = upload(ctx, path, NULL, db, srv, hashstr, srv->parent);
		break;
	case MC_CONFLICTACT_SKIP:
		rc = MC_ERR_CONFLICT_SKIP;
		break;
	default:
		MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED, "Unexpected ACT value: " << act);
	}
	return rc;
}

/* Ask ...
*	srv must be NULL or srv->status must be deleted	*/
int conflicted_noremote(mc_sync_ctx *ctx, const string& path, mc_file_fs *fs, mc_file *db, mc_file *srv, string *hashstr){
	Q_ASSERT(fs != NULL);
	Q_ASSERT(db != NULL);
	list<mc_file> tree;
	list<string> ptree;
	list<mc_file>::iterator tbegin, tit, tend;
	list<string>::iterator pit;
	mc_file parent, dbdummy;
	mc_file_fs fsdummy;
	string p;
	int rc;
	MC_CONFLICTACTION act = MC_CONFLICTACT_UNKNOWN;
	bool actR = false, actD = false;
	if (srv) { MC_INFL("Found conflict at file " << srv->id << ": " << printname(srv)) }
	else { MC_INFL("Found conflict at file ?: " << printname(fs)); }

	//check for previous
	if (ctx->dirconflictact != MC_CONFLICTACT_UNKNOWN) act = ctx->dirconflictact;
	if (act == MC_CONFLICTACT_KEEP) act = MC_CONFLICTACT_UNKNOWN; // can't keep, this is noremote

	if (act == MC_CONFLICTACT_UNKNOWN){
#ifdef MC_QTCLIENT
		p.assign(ctx->sync->path).append(path).append(fs->name);
		ostringstream local, server;
		string localstr, serverstr;
		local << "Name: " << shortname(fs->name, MC_QTCONFLICTMAXLEN) << "\n";
		if (fs->is_dir) local << "Directory\n";
		else local << "Size: " << BytesToSize(fs->size) << "\n";
		local << "Last Modification: " << TimeToString(fs->mtime);
		if (srv){
			server << "Name: " << shortname(srv->name, MC_QTCONFLICTMAXLEN) << "\n";
			if (srv->status == MC_FILESTAT_DELETED) server << "deleted\n";
			if (srv->is_dir) server << "Directory\n";
			else server << "Size: " << BytesToSize(srv->size) << "\n";
			if (srv->status == MC_FILESTAT_DELETED) server << "Deleted at: " << TimeToString(srv->mtime);
			else server << "Last Modification: " << TimeToString(srv->mtime);
		}
		else {
			server << "File never seen on Server\nParent directory is deleted";
		}

		localstr = local.str();
		serverstr = server.str();
		rc = QtClient::execConflictDialog(&p, &localstr, &serverstr, 0, false);
		switch (rc){
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
			MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED, "Unexpected ConflictDialog return value: " << rc);
		}
		if (rc & 16) {
			actD = true;
			if (rc & 32) actR = true;
		}
#else
		char choice;
		cout << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;
		cout << "A conflict was detected between the following files:" << endl;
		cout << "Local filesystem:" << endl;
		cout << fs->name << " (" << fs->size << " bytes)" << endl << "Last modification: " << TimeToString(fs->mtime) << endl;
		cout << "Server:" << endl;
		if (srv)
			cout << srv->name << " (deleted, last had " << srv->size << " bytes)" << endl << "Last known modification: " << TimeToString(srv->mtime) << endl;
		else
			cout << "File never seen on server" << endl;
		cout << "Type 1 for the local version, 2 for the remote version or 4 to skip" << endl;
		cout << "As the local directory is deleted, you probably want to choose 1 to delete the file" << endl;


		while (true){
			cin >> choice;
			if (atoi(&choice) == 1){
				cout << "Uploading the local version" << endl;
				act = MC_CONFLICTACT_UP;
				break;
			}
			else if (atoi(&choice) == 2){
				cout << "Downloading the server version" << endl;
				act = MC_CONFLICTACT_DOWN;
				break;
			}
			else if (atoi(&choice) == 4){
				cout << "Skipping" << endl;
				act = MC_CONFLICTACT_SKIP;
				break;
			}
		}
#endif
	}

	if (actR) ctx->rconflictact = true;
	if (actD) ctx->dirconflictact = act; //Will be undone in walk
	switch (act){
	case MC_CONFLICTACT_DOWN:
		rc = download(ctx, path, fs, db, srv, hashstr);
		break;
	case MC_CONFLICTACT_UP:
		p.assign(path);
		parent.id = db->parent;
		while (parent.id > 0){
			//rc = db_select_file_id(&parent);
			rc = srv_getmeta(parent.id, &parent);
			MC_CHKERR(rc);
			rc = crypt_file_fromsrv(ctx, p, &parent);
			MC_CHKERR(rc);
			if (parent.status != MC_FILESTAT_DELETED) break;
			p = p.substr(0, p.length() - 1);
			p.assign(p.substr(0, p.find_last_of("/") + 1));
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
		while (tit != tend){
			dbdummy.id = tit->id;
			rc = db_select_file_id(&dbdummy);
			MC_CHKERR(rc);
			rc = fs_filestats(&fsdummy, pit->c_str(), tit->name.c_str());
			MC_CHKERR(rc);
			rc = upload(ctx, *pit, &fsdummy, &dbdummy, &*tit, hashstr, 0, false);
			MC_CHKERR(rc);
			++tit;
			++pit;
		}
		rc = upload(ctx, path, fs, db, srv, hashstr);
		break;
	case MC_CONFLICTACT_SKIP:
		rc = MC_ERR_CONFLICT_SKIP;
		break;
	default:
		MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED, "Unexpected ACT value: " << act);
	}
	return rc;
}