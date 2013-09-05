#include "mc_db.h"
#ifdef MC_QTCLIENT
#	ifdef MC_IONOTIFY
#		include "qtClient.h"
#	endif
#endif


sqlite3 *db;
QMutex db_mutex;
/* sqlite3_stmts initialized by db_open instead of passing the same query strings over and over again */
sqlite3_stmt *stmt_select_status, *stmt_update_status, \
				*stmt_select_sync, *stmt_list_sync, *stmt_insert_sync, *stmt_update_sync, *stmt_delete_sync, \
				*stmt_select_filter, *stmt_list_filter_sid, *stmt_insert_filter, *stmt_update_filter, *stmt_delete_filter, *stmt_delete_filter_sid, \
				*stmt_select_file_name, *stmt_select_file_id, *stmt_list_file_parent, *stmt_insert_file, *stmt_update_file, *stmt_delete_file;
/* the respective queries */
#define QRY_SELECT_STATUS "SELECT locked,action,url,uname,passwd,acceptallcerts,watchmode,basedate,updatecheck,updateversion FROM status"
#define QRY_UPDATE_STATUS "UPDATE status SET locked = ?, action = ?, url = ?, uname = ?, passwd = ?, acceptallcerts = ?, watchmode = ?, basedate = ?, updatecheck = ?, updateversion = ?"
#define QRY_SELECT_SYNC "SELECT id,uid,priority,name,path,filterversion,crypted,status,lastsync,hash,cryptkey FROM syncs WHERE id = ?"
#define QRY_LIST_SYNC "SELECT id,uid,priority,name,path,filterversion,crypted,status,lastsync,hash,cryptkey FROM syncs"
#define QRY_INSERT_SYNC "INSERT INTO syncs (id,uid,priority,name,path,filterversion,crypted,status,lastsync,hash,cryptkey) VALUES (?,?,?,?,?,?,?,?,?,?,?)"
#define QRY_UPDATE_SYNC "UPDATE syncs SET priority = ?, name = ?, path = ?, filterversion = ?, crypted = ?, status = ?, lastsync = ?, hash = ?, cryptkey = ? WHERE id = ?"
#define QRY_DELETE_SYNC "DELETE FROM syncs WHERE id = ?"
#define QRY_SELECT_FILTER "SELECT id,sid,files,directories,type,rule FROM filters WHERE id = ?"
#define QRY_LIST_FILTER_SID "SELECT id,sid,files,directories,type,rule FROM filters WHERE sid = ?"
#define QRY_INSERT_FILTER "INSERT INTO filters (id,sid,files,directories,type,rule) VALUES (?,?,?,?,?,?)"
#define QRY_UPDATE_FILTER "UPDATE filters SET sid = ?, files = ?, directories = ?, type = ?, rule = ? WHERE id = ?"
#define QRY_DELETE_FILTER "DELETE FROM filters WHERE id = ?"
#define QRY_DELETE_FILTER_SID "DELETE FROM filters WHERE sid = ?"
#define QRY_SELECT_FILE_NAME "SELECT id,name,cryptname,ctime,mtime,size,is_dir,parent,hash,status FROM files WHERE parent = ? AND name = ?"
#define QRY_SELECT_FILE_ID "SELECT id,name,cryptname,ctime,mtime,size,is_dir,parent,hash,status FROM files WHERE id = ?"
#define QRY_LIST_FILE_PARENT "SELECT id,name,cryptname,ctime,mtime,size,is_dir,parent,hash,status FROM files WHERE parent = ?"
#define QRY_INSERT_FILE "INSERT INTO files (id,name,cryptname,ctime,mtime,size,is_dir,parent,hash,status) VALUES (?,?,?,?,?,?,?,?,?,?)"
#define QRY_UPDATE_FILE "UPDATE files SET name = ?, cryptname = ?, ctime = ?, mtime = ?, size = ?, is_dir = ?, parent = ?, hash = ?, status = ? WHERE id = ?"
#define QRY_DELETE_FILE "DELETE FROM files WHERE id = ?"

// the internal versions ignore the mutex, only use when the mutex is already held by you
bool _db_isopen();
int _db_open(const string& fname);
int _db_close();
int _db_execstr(const string& query); /* For administrative and one-time stuff only */

int _db_select_status(mc_status *var);
int _db_update_status(mc_status *var);

int _db_select_sync(mc_sync_db *var);
int _db_list_sync(list<mc_sync_db> *l);
int _db_insert_sync(mc_sync_db *var);
int _db_update_sync(mc_sync_db *var);
int _db_delete_sync(int id);

int _db_select_filter(mc_filter *var);
int _db_list_filter_sid(list<mc_filter> *l, int sid);
int _db_insert_filter(mc_filter *var);
int _db_update_filter(mc_filter *var);
int _db_delete_filter(int id);
int _db_delete_filter_sid(int sid);

int _db_select_file_name(mc_file *var);
int _db_select_file_id(mc_file *var);
int _db_list_file_parent(list<mc_file> *l, int parent);
int _db_insert_file(mc_file *var);
int _db_update_file(mc_file *var);
int _db_delete_file(int id);
//Helper macro to create safety wrapper those funcs
#define SAFEFUNC(name,param,call)		int name(param){			\
	int rc;															\
	db_mutex.lock();												\
	MC_NOTIFYIOSTART(MC_NT_DB);										\
	if(!db){														\
		MC_DBG("Query on closed db, opening");						\
		rc = _db_open("state.db");									\
		if(!rc) rc = _##name(call);									\
		if(rc) _db_close();											\
		else rc = _db_close();										\
	} else {														\
		rc = _##name(call);											\
	}																\
	MC_NOTIFYIOEND(MC_NT_DB);										\
	db_mutex.unlock();												\
	return rc;														\
}
#define SAFEFUNC2(name,param1,param2,call1,call2)	int name(param1, param2){	\
	int rc;															\
	db_mutex.lock();												\
	MC_NOTIFYIOSTART(MC_NT_DB);										\
	if(!db){														\
		MC_DBG("Query on closed db, opening");						\
		rc = _db_open("state.db");									\
		if(!rc) rc = _##name(call1, call2);							\
		if(rc) _db_close();											\
		else rc = _db_close();										\
	} else {														\
		rc = _##name(call1, call2);									\
	}																\
	MC_NOTIFYIOEND(MC_NT_DB);										\
	db_mutex.unlock();												\
	return rc;														\
}
#define SAFEFUNC_NOOPEN(name,paramlist,call) int name(paramlist){	\
	int rc;															\
	db_mutex.lock();												\
	MC_NOTIFYIOSTART(MC_NT_DB);										\
	rc = _##name(call);												\
	MC_NOTIFYIOEND(MC_NT_DB);										\
	db_mutex.unlock();												\
	return rc;														\
}


bool _db_isopen(){
	return (db != NULL);
}
bool db_isopen(){
	bool rc;
	db_mutex.lock();
	rc = _db_isopen();
	db_mutex.unlock();
	return rc;
}

SAFEFUNC_NOOPEN(db_open,const string& fname,fname)
int _db_open(const string& fname){
	int rc;
	if(db == NULL){
		MC_DBG("Opening db at " << fname);
		rc = sqlite3_open_v2(fname.c_str(),&db,SQLITE_OPEN_READWRITE,NULL);
		if(rc == SQLITE_CANTOPEN){
			/* Contrary to documentation rc will be SQLITE_OK when the file is writelocked
			test = fopen(fname.c_str(),"wb");
			if(!test) MC_ERR_MSG(-1, "db file not writeable")
			else {*/
				MC_INF("No db found, Creating a new db");
				/* db does not exist yet, so we create a new one */
				rc = sqlite3_open_v2(fname.c_str(),&db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,NULL);
				MC_CHKERR_MSG(rc, "Can't create db file");
				rc = _db_execstr("CREATE TABLE status (locked BOOLEAN NOT NULL, action INTEGER NOT NULL, url TEXT NOT NULL, uname TEXT NOT NULL, passwd TEXT NOT NULL, \
										acceptallcerts BOOLEAN NOT NULL, watchmode INTEGER NOT NULL, basedate INTEGER NOT NULL, updatecheck INTEGER NOT NULL, updateversion TEXT NOT NULL)");
				MC_CHKERR_MSG(rc, "Failed to setup new db.");
				rc = _db_execstr("INSERT INTO status (locked,action,url,uname,passwd,acceptallcerts,watchmode,basedate,updatecheck,updateversion) VALUES (0,0,'','','',0,300,0,0,'')");
				MC_CHKERR_MSG(rc, "Failed to setup new db.");
				rc = _db_execstr("CREATE TABLE syncs (id INTEGER PRIMARY KEY, uid INTEGER NOT NULL, priority INTEGER NOT NULL, name TEXT NOT NULL, path TEXT NOT NULL, \
									filterversion INTEGER NOT NULL, crypted INTEGER NOT NULL, status INTEGER NOT NULL, lastsync INTEGER NOT NULL, hash BLOB, cryptkey BLOB)");
				MC_CHKERR_MSG(rc, "Failed to setup new db.");
				rc = _db_execstr("CREATE TABLE filters (id INTEGER PRIMARY KEY, sid INTEGER NOT NULL, files BOOLEAN NOT NULL, directories BOOLEAN NOT NULL, \
									type INTEGER NOT NULL, rule TEXT NOT NULL)");
				MC_CHKERR_MSG(rc, "Failed to setup new db.");
				rc = _db_execstr("CREATE TABLE files (id INTEGER PRIMARY KEY, name TEXT NOT NULL, cryptname TEXT NOT NULL, ctime INTEGER NOT NULL, mtime INTERGER NOT NULL, size INTEGER NOT NULL, \
									is_dir BOOLEAN NOT NULL, parent INTEGER NOT NULL, hash BLOB, status INTEGER NOT NULL)");
				MC_CHKERR_MSG(rc, "Failed to setup new db.");
				rc = _db_execstr("CREATE INDEX byparent ON files (parent)");
				MC_CHKERR_MSG(rc, "Failed to setup new db.");
			//}
		} else MC_CHKERR_MSG(rc, "Can't open db file");

		/* prepare the stmts for later use */
		rc = sqlite3_prepare_v2(db, QRY_SELECT_STATUS, sizeof(QRY_SELECT_STATUS), &stmt_select_status, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_UPDATE_STATUS, sizeof(QRY_UPDATE_STATUS), &stmt_update_status, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_SELECT_SYNC, sizeof(QRY_SELECT_SYNC), &stmt_select_sync, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_LIST_SYNC, sizeof(QRY_LIST_SYNC), &stmt_list_sync, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_INSERT_SYNC, sizeof(QRY_INSERT_SYNC), &stmt_insert_sync, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_UPDATE_SYNC, sizeof(QRY_UPDATE_SYNC), &stmt_update_sync, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_DELETE_SYNC, sizeof(QRY_DELETE_SYNC), &stmt_delete_sync, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_SELECT_FILTER, sizeof(QRY_SELECT_FILTER), &stmt_select_filter, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_LIST_FILTER_SID, sizeof(QRY_LIST_FILTER_SID), &stmt_list_filter_sid, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_INSERT_FILTER, sizeof(QRY_INSERT_FILTER), &stmt_insert_filter, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_UPDATE_FILTER, sizeof(QRY_UPDATE_FILTER), &stmt_update_filter, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_DELETE_FILTER, sizeof(QRY_DELETE_FILTER), &stmt_delete_filter, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_DELETE_FILTER_SID, sizeof(QRY_DELETE_FILTER_SID), &stmt_delete_filter_sid, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_SELECT_FILE_NAME, sizeof(QRY_SELECT_FILE_NAME), &stmt_select_file_name, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_SELECT_FILE_ID, sizeof(QRY_SELECT_FILE_ID), &stmt_select_file_id, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_LIST_FILE_PARENT, sizeof(QRY_LIST_FILE_PARENT), &stmt_list_file_parent, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_INSERT_FILE, sizeof(QRY_INSERT_FILE), &stmt_insert_file, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_UPDATE_FILE, sizeof(QRY_UPDATE_FILE), &stmt_update_file, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));
		rc = sqlite3_prepare_v2(db, QRY_DELETE_FILE, sizeof(QRY_DELETE_FILE), &stmt_delete_file, NULL);
		MC_CHKERR_MSG(rc, "Failed to prepare statement: " << sqlite3_errmsg(db));

		/* check wether the db was closed properly */
		mc_status status;
		rc = _db_select_status(&status);
		MC_CHKERR_MSG(rc, "Couldn't determine db status");
		if(status.locked){ //TODO: cleanup?
			MC_INF("Unclean shutdown detected");
			rc = _db_execstr(string("UPDATE syncs SET status = ") + to_string(MC_SYNCSTAT_ABORTED) + " WHERE status = " + to_string(MC_SYNCSTAT_RUNNING));
			MC_CHKERR_MSG(rc, "Failed to cleanup db");
		}

		/* check wether we can write to the db */
		status.locked = true;
		//rc = _db_update_status(&status);
		rc = _db_execstr("UPDATE status SET locked = 1");
		if(rc == SQLITE_READONLY) {
			_db_close();
			MC_CHKERR_MSG(rc, "db file is read only");
		} else MC_CHKERR_MSG(rc, "Failed to lock db");

		MC_INF("db opened and initialized");
	} else {
		MC_DBG("db already open");
	}
	return 0;
}

SAFEFUNC(db_close,,)
int _db_close(){
	int rc;
	if(db != NULL){
		MC_DBGL("Closing db");
		sqlite3_finalize(stmt_select_status);
		sqlite3_finalize(stmt_update_status);
		sqlite3_finalize(stmt_select_sync);
		sqlite3_finalize(stmt_list_sync);
		sqlite3_finalize(stmt_insert_sync);
		sqlite3_finalize(stmt_update_sync);
		sqlite3_finalize(stmt_delete_sync);
		sqlite3_finalize(stmt_select_filter);
		sqlite3_finalize(stmt_list_filter_sid);
		sqlite3_finalize(stmt_insert_filter);
		sqlite3_finalize(stmt_update_filter);
		sqlite3_finalize(stmt_delete_filter);
		sqlite3_finalize(stmt_delete_filter_sid);
		sqlite3_finalize(stmt_select_file_name);
		sqlite3_finalize(stmt_select_file_id);
		sqlite3_finalize(stmt_list_file_parent);
		sqlite3_finalize(stmt_insert_file);
		sqlite3_finalize(stmt_update_file);
		sqlite3_finalize(stmt_delete_file);
		rc = _db_execstr("UPDATE status SET locked = 0");
		if(rc) if(rc != SQLITE_READONLY) MC_WRN("Failed to close db cleanly");
		rc = sqlite3_close_v2(db);
		if(rc) MC_WRN("Failed to close db");
		db = NULL;
		MC_DBG("db closed");
	} else {
		MC_DBG("db not open");
	}
	return 0;
}

SAFEFUNC(db_execstr,const string& query,query)
int _db_execstr(const string& query){
	int rc;
	char* errmsg;
	MC_DBGL("Executing " << query);
	rc = sqlite3_exec(db,query.c_str(),NULL,0,&errmsg);
	MC_CHKERR_MSG(rc,"Query failed: " << sqlite3_errmsg(db))
	return 0;
}

SAFEFUNC(db_select_status,mc_status *var,var)
int _db_select_status(mc_status *var){
	int rc;
	MC_DBGL("Selecting status of database");
	rc = sqlite3_reset(stmt_select_status);
	MC_CHKERR_MSG(rc,"Reset failed");
	rc = sqlite3_step(stmt_select_status);
	if(rc != SQLITE_DONE && rc != SQLITE_ROW) MC_ERR_MSG(rc,"Query failed: " << sqlite3_errmsg(db))
	else if(rc == SQLITE_DONE) return rc; /* Empty result set */
	else if(rc == SQLITE_ROW){ /* We expect only one result set */
		var->locked = sqlite3_column_int(stmt_select_status,0) != 0;
		var->action = sqlite3_column_int(stmt_select_status,1);
		var->url.assign((const char*)sqlite3_column_text(stmt_select_status,2));
		var->uname.assign((const char*)sqlite3_column_text(stmt_select_status,3));
		var->passwd.assign((const char*)sqlite3_column_text(stmt_select_status,4));
		var->acceptallcerts = sqlite3_column_int(stmt_select_status,5) != 0;
		var->watchmode = sqlite3_column_int(stmt_select_status,6);
		var->basedate = sqlite3_column_int64(stmt_select_status,7);
		var->updatecheck = sqlite3_column_int64(stmt_select_status,8);
		var->updateversion.assign((const char*)sqlite3_column_text(stmt_select_status,9));
	}
	return 0;
}

SAFEFUNC(db_update_status,mc_status *var,var)
int _db_update_status(mc_status *var){
	int rc;
	MC_DBGL("Setting status");
	rc = sqlite3_reset(stmt_update_status);
	rc = sqlite3_clear_bindings(stmt_update_status);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_update_status,1,var->locked);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_status,2,var->action);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_update_status,3,var->url.c_str(),var->url.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_update_status,4,var->uname.c_str(),var->uname.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_update_status,5,var->passwd.c_str(),var->passwd.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_status,6,var->acceptallcerts);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_status,7,var->watchmode);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int64(stmt_update_status,8,var->basedate);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int64(stmt_update_status,9,var->updatecheck);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_update_status,10,var->updateversion.c_str(),var->updateversion.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_update_status);
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	return 0;
}

SAFEFUNC(db_select_sync,mc_sync_db *var,var)
int _db_select_sync(mc_sync_db *var){
	int rc;
	MC_DBGL("Selecting sync by id " << var->id);
	rc = sqlite3_reset(stmt_select_sync);
	rc = sqlite3_clear_bindings(stmt_select_sync);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_select_sync,1,var->id);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_select_sync);
	if(rc != SQLITE_DONE && rc != SQLITE_ROW) MC_ERR_MSG(rc,"Query failed: " << sqlite3_errmsg(db))
	else if(rc == SQLITE_DONE) return rc; /* Empty result set */
	else if(rc == SQLITE_ROW){ /* We expect only one result set */
		var->id = sqlite3_column_int(stmt_select_sync,0);
		var->uid = sqlite3_column_int(stmt_select_sync,1);
		var->priority = sqlite3_column_int(stmt_select_sync,2);
		var->name.assign((const char*)sqlite3_column_text(stmt_select_sync,3));
		var->path.assign((const char*)sqlite3_column_text(stmt_select_sync,4));
		var->filterversion = sqlite3_column_int(stmt_select_sync,5);
		var->crypted = sqlite3_column_int(stmt_select_sync,6) != 0;
		var->status = (MC_SYNCSTATUS)sqlite3_column_int(stmt_select_sync,7);
		var->lastsync = sqlite3_column_int64(stmt_select_sync,8);
		memcpy(var->hash,sqlite3_column_blob(stmt_select_sync,9),16);
		memcpy(var->cryptkey,sqlite3_column_blob(stmt_select_sync,10),32);
	}
	return 0;
}

SAFEFUNC(db_list_sync,list<mc_sync_db> *l,l)
int _db_list_sync(list<mc_sync_db> *l){
	int rc;
	mc_sync_db var;
	MC_DBGL("Listing syncs");
	rc = sqlite3_reset(stmt_list_sync);
	/* No Conditions, no bindings */
	rc = sqlite3_step(stmt_list_sync);
	while(rc == SQLITE_ROW){	
		var.id = sqlite3_column_int(stmt_list_sync,0);
		var.uid = sqlite3_column_int(stmt_list_sync,1);
		var.priority = sqlite3_column_int(stmt_list_sync,2);
		var.name.assign((const char*)sqlite3_column_text(stmt_list_sync,3));
		var.path.assign((const char*)sqlite3_column_text(stmt_list_sync,4));
		var.filterversion = sqlite3_column_int(stmt_list_sync,5);
		var.crypted = sqlite3_column_int(stmt_list_sync,6) != 0;
		var.status = (MC_SYNCSTATUS)sqlite3_column_int(stmt_list_sync,7);
		var.lastsync = sqlite3_column_int64(stmt_list_sync,8);
		memcpy(var.hash,sqlite3_column_blob(stmt_list_sync,9),16);
		memcpy(var.cryptkey,sqlite3_column_blob(stmt_list_sync,10),32);
		l->push_back(var);
		rc = sqlite3_step(stmt_list_sync);
	}
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	return 0;
}

SAFEFUNC(db_insert_sync,mc_sync_db *var,var)
int _db_insert_sync(mc_sync_db *var){
	int rc;
	MC_DBGL("Inserting sync " << var->id << ": " << var-> name);
	rc = sqlite3_reset(stmt_insert_sync);
	rc = sqlite3_clear_bindings(stmt_insert_sync);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_insert_sync,1,var->id);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_insert_sync,2,var->uid);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_insert_sync,3,var->priority);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_insert_sync,4,var->name.c_str(),var->name.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_insert_sync,5,var->path.c_str(),var->path.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_insert_sync,6,var->filterversion);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_insert_sync,7,var->crypted);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_insert_sync,8,var->status);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int64(stmt_insert_sync,9,var->lastsync);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_blob(stmt_insert_sync,10,var->hash,16,SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_blob(stmt_insert_sync,11,var->cryptkey,32,SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_insert_sync);
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	return 0;
}

SAFEFUNC(db_update_sync,mc_sync_db *var,var)
int _db_update_sync(mc_sync_db *var){
	int rc;
	MC_DBGL("Updating sync " << var->id << ": " << var-> name);
	rc = sqlite3_reset(stmt_update_sync);
	rc = sqlite3_clear_bindings(stmt_update_sync);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_update_sync,1,var->priority);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_update_sync,2,var->name.c_str(),var->name.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_update_sync,3,var->path.c_str(),var->path.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_sync,4,var->filterversion);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_sync,5,var->crypted);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_sync,6,var->status);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int64(stmt_update_sync,7,var->lastsync);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_blob(stmt_update_sync,8,var->hash,16,SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_blob(stmt_update_sync,9,var->cryptkey,32,SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_sync,10,var->id);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_update_sync);
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	rc = sqlite3_changes(db);
	return 0;
}

SAFEFUNC(db_delete_sync,int id,id)
int _db_delete_sync(int id){
	int rc;
	MC_DBGL("Deleting sync " << id);
	rc = sqlite3_reset(stmt_delete_sync);
	rc = sqlite3_clear_bindings(stmt_delete_sync);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_delete_sync,1,id);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_delete_sync);
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	return 0;
}

SAFEFUNC(db_select_filter,mc_filter *var,var)
int _db_select_filter(mc_filter *var){
	int rc;
	MC_DBGL("Selecting filter by id " << var->id);
	rc = sqlite3_reset(stmt_select_filter);
	rc = sqlite3_clear_bindings(stmt_select_filter);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_select_filter,1,var->id);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_select_filter);
	if(rc != SQLITE_DONE && rc != SQLITE_ROW) MC_ERR_MSG(rc,"Query failed: " << sqlite3_errmsg(db))
	else if(rc == SQLITE_DONE) return rc; /* Empty result set */
	else if(rc == SQLITE_ROW){ /* We expect only one result set */
		var->id = sqlite3_column_int(stmt_select_filter,0);
		var->sid = sqlite3_column_int(stmt_select_filter,1);
		var->files = sqlite3_column_int(stmt_select_filter,2) != 0;
		var->directories = sqlite3_column_int(stmt_select_filter,3) != 0;
		var->type = (MC_FILTERTYPE) sqlite3_column_int(stmt_select_filter,4);
		var->rule.assign((const char*)sqlite3_column_text(stmt_select_filter,5));
	}
	return 0;
}

SAFEFUNC2(db_list_filter_sid, list<mc_filter> *l, int sid, l, sid)
int _db_list_filter_sid(list<mc_filter> *l, int sid){
	int rc;
	mc_filter var;
	MC_DBGL("Listing filter by sid " << sid);
	rc = sqlite3_reset(stmt_list_filter_sid);
	rc = sqlite3_clear_bindings(stmt_list_filter_sid);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_list_filter_sid,1,sid);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_list_filter_sid);
	while(rc == SQLITE_ROW){
		//id,name,ctime,mtime,size,is_dir,parent,hash,status
		var.id = sqlite3_column_int(stmt_list_filter_sid,0);
		var.sid = sqlite3_column_int(stmt_list_filter_sid,1);
		var.files = sqlite3_column_int(stmt_list_filter_sid,2) != 0;
		var.directories = sqlite3_column_int(stmt_list_filter_sid,3) != 0;
		var.type = (MC_FILTERTYPE) sqlite3_column_int(stmt_list_filter_sid,4);
		var.rule.assign((const char*)sqlite3_column_text(stmt_list_filter_sid,5));
		l->push_back(var);
		rc = sqlite3_step(stmt_list_filter_sid);
	}
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	return 0;
}

SAFEFUNC(db_insert_filter,mc_filter *var,var)
int _db_insert_filter(mc_filter *var){
	int rc;
	MC_DBGL("Inserting filter " << var->id << ": " << var->rule);
	rc = sqlite3_reset(stmt_insert_filter);
	rc = sqlite3_clear_bindings(stmt_insert_filter);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_insert_filter,1,var->id);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_insert_filter,2,var->sid);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_insert_filter,3,var->files);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_insert_filter,4,var->directories);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_insert_filter,5,var->type);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_insert_filter,6,var->rule.c_str(),var->rule.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_insert_filter);
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	return 0;
}

SAFEFUNC(db_update_filter,mc_filter *var,var)
int _db_update_filter(mc_filter *var){
	int rc;
	MC_DBGL("Updating filter " << var->id << ": " << var->rule);
	rc = sqlite3_reset(stmt_update_filter);
	rc = sqlite3_clear_bindings(stmt_update_filter);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_update_filter,1,var->sid);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_filter,2,var->files);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_filter,3,var->directories);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_filter,4,var->type);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_update_filter,5,var->rule.c_str(),var->rule.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_filter,6,var->id);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_update_filter);
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	return 0;
}

SAFEFUNC(db_delete_filter,int id,id)
int _db_delete_filter(int id){
	int rc;
	MC_DBGL("Deleting filter " << id);
	rc = sqlite3_reset(stmt_delete_filter);
	rc = sqlite3_clear_bindings(stmt_delete_filter);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_delete_filter,1,id);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_delete_filter);
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	return 0;
}

SAFEFUNC(db_delete_filter_sid,int sid,sid)
int _db_delete_filter_sid(int sid){
	int rc;
	MC_DBGL("Deleting filters by sid " << sid);
	rc = sqlite3_reset(stmt_delete_filter_sid);
	rc = sqlite3_clear_bindings(stmt_delete_filter_sid);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_delete_filter_sid,1,sid);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_delete_filter_sid);
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	return 0;
}

SAFEFUNC(db_select_file_name,mc_file *var,var)
int _db_select_file_name(mc_file *var){
	int rc;
	MC_DBGL("Selecting file by name " << var->name);
	rc = sqlite3_reset(stmt_select_file_name);
	rc = sqlite3_clear_bindings(stmt_select_file_name);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_select_file_name,1,var->parent);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_select_file_name,2,var->name.c_str(),var->name.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_select_file_name);
	if(rc != SQLITE_DONE && rc != SQLITE_ROW) MC_ERR_MSG(rc,"Query failed: " << sqlite3_errmsg(db))
	else if(rc == SQLITE_DONE) return rc; /* Empty result set */
	else if(rc == SQLITE_ROW){ /* We expect only one result set */
		var->id = sqlite3_column_int(stmt_select_file_name,0);
		var->name.assign((const char*)sqlite3_column_text(stmt_select_file_name,1));
		var->cryptname.assign((const char*)sqlite3_column_text(stmt_select_file_name,2));
		var->ctime = sqlite3_column_int64(stmt_select_file_name,3);
		var->mtime = sqlite3_column_int64(stmt_select_file_name,4);
		var->size = sqlite3_column_int64(stmt_select_file_name,5);
		var->is_dir = sqlite3_column_int(stmt_select_file_name,6) != 0;
		var->parent = sqlite3_column_int(stmt_select_file_name,7);
		memcpy(var->hash,sqlite3_column_blob(stmt_select_file_name,8),16);
		var->status = (MC_FILESTATUS) sqlite3_column_int(stmt_select_file_name,9);
	}
	return 0;
}

SAFEFUNC(db_select_file_id,mc_file *var,var)
int _db_select_file_id(mc_file *var){
	int rc;
	MC_DBGL("Selecting file by id " << var->id);
	rc = sqlite3_reset(stmt_select_file_id);
	rc = sqlite3_clear_bindings(stmt_select_file_id);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_select_file_id,1,var->id);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_select_file_id);
	if(rc != SQLITE_DONE && rc != SQLITE_ROW) MC_ERR_MSG(rc,"Query failed: " << sqlite3_errmsg(db))
	else if(rc == SQLITE_DONE) return rc; /* Empty result set */
	else if(rc == SQLITE_ROW){ /* We expect only one result set */
		var->id = sqlite3_column_int(stmt_select_file_id,0);
		var->name.assign((const char*)sqlite3_column_text(stmt_select_file_id,1));
		var->cryptname.assign((const char*)sqlite3_column_text(stmt_select_file_id,2));
		var->ctime = sqlite3_column_int64(stmt_select_file_id,3);
		var->mtime = sqlite3_column_int64(stmt_select_file_id,4);
		var->size = sqlite3_column_int64(stmt_select_file_id,5);
		var->is_dir = sqlite3_column_int(stmt_select_file_id,6) != 0;
		var->parent = sqlite3_column_int(stmt_select_file_id,7);
		memcpy(var->hash,sqlite3_column_blob(stmt_select_file_id,8),16);
		var->status = (MC_FILESTATUS) sqlite3_column_int(stmt_select_file_id,9);
	}
	return 0;
}

SAFEFUNC2(db_list_file_parent, list<mc_file> *l,int parent, l,parent)
int _db_list_file_parent(list<mc_file> *l, int parent){
	int rc;
	mc_file var;
	MC_DBGL("Listing file by parent " << parent);
	rc = sqlite3_reset(stmt_list_file_parent);
	rc = sqlite3_clear_bindings(stmt_list_file_parent);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_list_file_parent,1,parent);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_list_file_parent);
	while(rc == SQLITE_ROW){
		//id,name,ctime,mtime,size,is_dir,parent,hash,status
		var.id = sqlite3_column_int(stmt_list_file_parent,0);
		var.name.assign((const char*)sqlite3_column_text(stmt_list_file_parent,1));
		var.cryptname.assign((const char*)sqlite3_column_text(stmt_list_file_parent,2));
		var.ctime = sqlite3_column_int64(stmt_list_file_parent,3);
		var.mtime = sqlite3_column_int64(stmt_list_file_parent,4);
		var.size = sqlite3_column_int64(stmt_list_file_parent,5);
		var.is_dir = sqlite3_column_int(stmt_list_file_parent,6) != 0;
		var.parent = sqlite3_column_int(stmt_list_file_parent,7);
		memcpy(var.hash,sqlite3_column_blob(stmt_list_file_parent,8),16);
		var.status = (MC_FILESTATUS) sqlite3_column_int(stmt_list_file_parent,9);
		l->push_back(var);
		rc = sqlite3_step(stmt_list_file_parent);
	}
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	return 0;
}

SAFEFUNC(db_insert_file,mc_file *var,var)
int _db_insert_file(mc_file *var){
	int rc;
	MC_DBGL("Inserting file " << var->id << ": " << var->name);
	rc = sqlite3_reset(stmt_insert_file);
	rc = sqlite3_clear_bindings(stmt_insert_file);
	MC_CHKERR_MSG(rc,"Clear failed");
	// id,name,namehash,ctime,mtime,size,is_dir,parent,hash,status
	rc = sqlite3_bind_int(stmt_insert_file,1,var->id);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_insert_file,2,var->name.c_str(),var->name.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_insert_file,3,var->cryptname.c_str(),var->cryptname.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int64(stmt_insert_file,4,var->ctime);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int64(stmt_insert_file,5,var->mtime);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int64(stmt_insert_file,6,var->size);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_insert_file,7,var->is_dir);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_insert_file,8,var->parent);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_blob(stmt_insert_file,9,var->hash,16,SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_insert_file,10,var->status);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_insert_file);
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	return 0;
}

SAFEFUNC(db_update_file,mc_file *var,var)
int _db_update_file(mc_file *var){
	int rc;
	MC_DBGL("Updating file " << var->id << ": " << var->name);
	rc = sqlite3_reset(stmt_update_file);
	rc = sqlite3_clear_bindings(stmt_update_file);
	MC_CHKERR_MSG(rc,"Clear failed");
	// name,namehash,ctime,mtime,size,is_dir,parent,hash,status
	rc = sqlite3_bind_text(stmt_update_file,1,var->name.c_str(),var->name.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_text(stmt_update_file,2,var->cryptname.c_str(),var->cryptname.length(),SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int64(stmt_update_file,3,var->ctime);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int64(stmt_update_file,4,var->mtime);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int64(stmt_update_file,5,var->size);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_file,6,var->is_dir);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_file,7,var->parent);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_blob(stmt_update_file,8,var->hash,16,SQLITE_STATIC);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_file,9,var->status);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_bind_int(stmt_update_file,10,var->id);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_update_file);
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	return 0;
}

SAFEFUNC(db_delete_file,int id,id)
int _db_delete_file(int id){
	int rc;
	MC_DBGL("Deleting file " << id );
	rc = sqlite3_reset(stmt_delete_file);
	rc = sqlite3_clear_bindings(stmt_delete_file);
	MC_CHKERR_MSG(rc,"Clear failed");
	rc = sqlite3_bind_int(stmt_delete_file,1,id);
	MC_CHKERR_MSG(rc,"Bind failed");
	rc = sqlite3_step(stmt_delete_file);
	MC_CHKERR_EXP(rc,SQLITE_DONE,"Query failed: " << sqlite3_errmsg(db));
	return 0;
}