#include "mc_helpers.h"
#ifdef MC_QTCLIENT
#	include "qtClient.h"
#endif

/* cascade up the tree and recalc hashes from db */
int directoryHash(mc_sync_ctx *ctx, int id, unsigned char hash[16]) {
	list<mc_file> l;
	string hashstr = "";
	int rc;
	MC_DBGL("Hashing directory " << id);

	rc = db_list_file_parent(&l, id);
	MC_CHKERR(rc);

	l.sort(compare_mc_file_id);

	for (mc_file& f : l) {
		crypt_filestring(ctx, &f, &hashstr);
	}
	rc = strmd5(hash, hashstr);
	MC_CHKERR(rc);
	MC_DBGL("Hash of " << id << " is " << MD5BinToHex(hash));
	//MC_DBGL("Hashdata is " << BinToHex(hashstr));
	return 0;
}
int updateHash(mc_sync_ctx *ctx, mc_file *f, mc_sync_db *s) {
	mc_file t;
	//mc_sync_db s;
	int rc;
	MC_DBG("Recalculating hashes from " << f->id);
	t.parent = f->parent;
	while (t.parent > 0) {
		t.id = t.parent;
		rc = db_select_file_id(&t);
		MC_CHKERR(rc);

		rc = directoryHash(ctx, t.id, t.hash);
		MC_CHKERR(rc);

		rc = db_update_file(&t);
		MC_CHKERR(rc);
	}

	//s.id = -t.parent;
	//rc = db_select_sync(&s);
	//MC_CHKERR(rc);

	rc = directoryHash(ctx, t.parent, s->hash);
	MC_CHKERR(rc);

	rc = db_update_sync(s);
	MC_CHKERR(rc);

	return 0;
}

int updateMtimeIfMismatch(string fpath, mc_file* db) {
	int rc;
	mc_file_fs fsnow;

	if (db) {
		rc = fs_filestats(&fsnow, fpath, db->name);
		MC_CHKERR(rc);

		if (fsnow.mtime != db->mtime) {
			rc = fs_touch(fpath, db->mtime, db->ctime);
			MC_CHKERR(rc);
		}
	}

	return 0;
}

/* retrieve and compare sync hashes */
int fullsync() {
	list<mc_sync_db> dbsyncs;
	int rc;
	rc = db_list_sync(&dbsyncs);
	MC_CHKERR(rc);

	return fullsync(&dbsyncs);
	
}
int fullsync(list<mc_sync_db> *dbsyncs) {
	list<mc_sync> srvsyncs;// = list<mc_sync>();
	list<mc_sync>::iterator srvsyncsit, srvsyncsend;
	list<mc_sync_db>::iterator dbsyncsit, dbsyncsend;
	bool fullsync = true;
	int rc;

	//srvsyncs.clear();
	rc = srv_listsyncs(&srvsyncs);
	MC_CHKERR(rc);

	dbsyncs->clear();
	rc = db_list_sync(dbsyncs);
	MC_CHKERR(rc);

	srvsyncs.sort();	
	dbsyncs->sort();
	dbsyncsit = dbsyncs->begin();
	dbsyncsend = dbsyncs->end();
	srvsyncsend = srvsyncs.end();
	for (;dbsyncsit != dbsyncsend; ++dbsyncsit) { //Foreach db sync
		if (dbsyncsit->status == MC_SYNCSTAT_DISABLED) continue;
		srvsyncsit = srvsyncs.begin();
		while ((srvsyncsit != srvsyncsend) && (dbsyncsit->id != srvsyncsit->id)) ++srvsyncsit;
		if ((srvsyncsit == srvsyncsend) || (dbsyncsit->id != srvsyncsit->id)) {
			cout << dbsyncsit->name << " is not available on server" << endl;
			dbsyncsit->status = MC_SYNCSTAT_UNAVAILABLE;
			rc = db_update_sync(&*dbsyncsit);
			MC_CHKERR(rc);
		} else {
			if (memcmp(dbsyncsit->hash, srvsyncsit->hash, 16)) {
				fullsync = false; 
				//break;
			} else {
				dbsyncsit->status = MC_SYNCSTAT_SYNCED;
				//dbsyncsit->lastsync = time(NULL);
				rc = db_update_sync(&*dbsyncsit);
				MC_CHKERR(rc);
			}
		}
	}
	if (fullsync) return 0;
	else return MC_ERR_NOTFULLYSYNCED;
}

/* panic action when decryption failed */
int cryptopanic() {
	MC_WRN("Crypt Verify Fail. Aborting.");
	srv_close();
	db_execstr("UPDATE status SET url = '" MC_UNTRUSTEDPREFIX "' || url");
	MC_NOTIFYEND(MC_NT_SYNC); //Trigger ListSyncs
	MC_NOTIFY(MC_NT_CRYPTOFAIL, "");
	return MC_ERR_CRYPTOALERT;
}

/* HFS+ (OS X) normalizes file names to a UTF8 NFD variant
*	When listing files, we need to find the denormalized pendant in our database
*	and use that name. It will be normalized back around when passed to the OS */
void denormalize_filenames(list<mc_file_fs>& onfs, list<mc_file> indb) {
	QString fsnormalized;
	list<QString> dbnormalized;
	list<mc_file_fs>::iterator onfsit, onfsend;
	list<mc_file>::iterator indbit, indbend;
	list<QString>::iterator fsit, fsend;
	list<QString>::iterator dbit, dbend;

	for (mc_file& db : indb) {
		dbnormalized.push_back(QString::fromUtf8(db.name.c_str(), db.name.length()).normalized(QString::NormalizationForm_C));
	}

	indbend = indb.end();

	for (mc_file_fs& fs : onfs) {
		indbit = indb.begin();
		dbit = dbnormalized.begin();
		fsnormalized = QString::fromUtf8(fs.name.c_str(), fs.name.length()).normalized(QString::NormalizationForm_C);
		while (indbit != indbend) {
			if (fsnormalized == *dbit) {
				fs.name = indbit->name;
				break;
			}
			indbit++;
			dbit++;
		}
	}
}
