#include "mc_watch.h"
#ifdef MC_WATCHMODE
#ifdef MC_QTCLIENT
#	include "qtClient.h"
#else
#	include "mc_workerthread.h"
#endif
#define CAFILE "trustCA.crt"


QtWatcher::QtWatcher(const QStringList &paths, list<mc_sync_db> *syncs, const QString& url, const QString& certfile, bool acceptall){
	connect(this,SIGNAL(_startLocalWatch()),this,SLOT(__startLocalWatch()));
	watcher = new QFileSystemWatcher(paths);
	connect(watcher,SIGNAL(directoryChanged(const QString &)),this,SLOT(directoryChanged(const QString &)));
	connect(watcher,SIGNAL(fileChanged(const QString &)),this,SLOT(fileChanged(const QString &)));
	watchsyncs = syncs;
	QString _url = "https://";
	_url.append(url);
	_url.append("/bin.php");
	performer = new QtNetworkPerformer(_url,certfile,acceptall,true,40); //30sec is half-open time
	connect(performer,SIGNAL(finished(int)),this,SLOT(remoteChange(int)));
	SetBuf(&netibuf);
	SetBuf(&netobuf);
	timer.setSingleShot(true);
	connect(&timer,SIGNAL(timeout()),&loop,SLOT(quit()));
	evttimer.setSingleShot(true);
	evttimer.setInterval(5000);
	quittimer.setInterval(2000);
	restarttimer.setSingleShot(true);
	restarttimer.setInterval(30000);
	watchlocaltimer.setSingleShot(true);
	watchlocaltimer.setInterval(0);
	connect(&quittimer,SIGNAL(timeout()),this,SLOT(checkquit()));
	connect(&evttimer,SIGNAL(timeout()),this,SLOT(changeTimeout()));
	connect(&restarttimer,SIGNAL(timeout()),this,SLOT(startRemoteWatch()));
	connect(&watchlocaltimer,SIGNAL(timeout()),this,SLOT(__startLocalWatch()));
}

QtWatcher::~QtWatcher(){
	if(performer)
		delete performer;
	if(watcher)
		delete watcher;
	ClearBuf(&netibuf);
	ClearBuf(&netobuf);
}

void QtWatcher::run(int timeout){
	timer.start(timeout*1000);
	quittimer.start();
	repeatcounter = 0;
	startLocalWatch();
	startRemoteWatch();
	loop.exec();
	timer.stop();
	quittimer.stop();
}

void QtWatcher::checkquit(){
	if(MC_TERMINATING()){
		MC_INF("Abort from GUI");
		performer->abort();
		loop.quit();
	}
}

void QtWatcher::startLocalWatch(){
	//emit _startLocalWatch(); //We need this, so the watcher events in the queue are discarded first
	watchlocaltimer.start();
}
void QtWatcher::__startLocalWatch(){
	watchfs = true;
}
void QtWatcher::startRemoteWatch(){
	srv_notifychange_async(&netibuf,&netobuf,performer,watchsyncs);
}
void QtWatcher::stopLocalWatch(){
	watchfs = false;
}
void QtWatcher::stopRemoteWatch(){
	performer->abort();
}

void QtWatcher::directoryChanged(const QString &path){
	if(watchfs){
		MC_DBG(qPrintable(path) << " has changed");
		crypt_seed(time(NULL) % path.length()*137351);
		if(!changepaths.contains(path)){
			changepaths.append(path);
			repeatcounter = 0;
		} else {
			repeatcounter++;
			if(repeatcounter > 50){
				MC_INF("FileSystemWatcher deletion bug?, canceling");
				delete watcher;
				watcher = NULL;
				loop.quit();
			}
		}
		evttimer.start();
	} else {
		MC_DBG("Ignoring FS change");
	}
}
void QtWatcher::fileChanged(const QString &path){
	QString dirpath;
	if(watchfs){
		MC_DBG(qPrintable(path) << " has changed");
		dirpath = path.mid(0,path.lastIndexOf("/")+1); //Just add the dir, we don't want to walk files
		crypt_seed(time(NULL) % path.length()*137347);
		if(!changepaths.contains(dirpath)){
			changepaths.append(dirpath);
			repeatcounter = 0;
		} else {
			repeatcounter++;
			if(repeatcounter > 50){
				MC_INF("FileSystemWatcher deletion bug?, canceling");
				delete watcher;
				watcher = NULL;
				loop.quit();
			}
		}
		evttimer.start();
	} else {
		MC_DBG("Ignoring FS change");
	}
}

int QtWatcher::changeTimeout(){
	int rc,i,lastsyncid;
	mc_sync_ctx context;
	list<mc_sync_db> newsyncs;
	list<mc_sync_db>::iterator dbsyncsit,dbsyncsend;
	stopRemoteWatch();
	stopLocalWatch();
	//MC_DBG("Change timeout, walking");
	MC_INF("Local change detected");
	repeatcounter = 0;

	changepaths.sort();
	//changepaths is never empty if this was triggered
	list<mc_filter> generalfilter,filter;
	rc = db_list_filter_sid(&generalfilter,0);
	MC_CHKERR(rc);

	// we can assume srv is still open and authed
	lastsyncid = 0;

	while(!changepaths.empty()){
		QString s = changepaths.takeFirst();
		//remove all subpaths
		for(i=0;i<changepaths.length();i++) if(changepaths[i].startsWith(s)) { changepaths.removeAt(i); i--; }
		//find sync
		watchsyncs->clear();
		rc = db_list_sync(&newsyncs);
		MC_CHKERR(rc);
		for(mc_sync_db& s : newsyncs){
			if(s.status != MC_SYNCSTAT_DISABLED || s.status != MC_SYNCSTAT_CRYPTOFAIL) watchsyncs->push_back(s);
		}
		dbsyncsit = watchsyncs->begin();
		dbsyncsend = watchsyncs->end();
		for(;dbsyncsit != dbsyncsend; ++dbsyncsit){
			if(s.startsWith(dbsyncsit->path.c_str()) 
				&& dbsyncsit->status != MC_SYNCSTAT_DISABLED
				&& dbsyncsit->status != MC_SYNCSTAT_CRYPTOFAIL) break;
		}
		if(dbsyncsit == dbsyncsend) MC_ERR_MSG(-1,"Path " << qPrintable(s) << " not in any watched sync");

		//update filters
		if(lastsyncid != dbsyncsit->id){
			filter.assign(generalfilter.begin(),generalfilter.end());
			rc = db_list_filter_sid(&filter,dbsyncsit->id);
			MC_CHKERR(rc);
			lastsyncid = dbsyncsit->id;
		}
		//find file
		
		init_sync_ctx(&context,&*dbsyncsit,&filter);
		if(s == dbsyncsit->path.c_str()){ //it's a sync itself
			MC_NOTIFYSTART(MC_NT_SYNC,dbsyncsit->name);
			rc = walk_nochange(&context,"",-dbsyncsit->id,dbsyncsit->hash);
			if(rc == MC_ERR_TERMINATING) dbsyncsit->status = MC_SYNCSTAT_ABORTED;
			else if(rc == MC_ERR_CRYPTOALERT) return cryptopanic();
			else if (MC_IS_CRITICAL_ERR(rc)) dbsyncsit->status = MC_SYNCSTAT_FAILED;
			else dbsyncsit->status = MC_SYNCSTAT_COMPLETED;
			dbsyncsit->lastsync = time(NULL); //TODO: not always a full sync!
			rc = db_update_sync(&*dbsyncsit);
			MC_CHKERR(rc);
			MC_NOTIFYEND(MC_NT_SYNC);
		} else {
			mc_file f;
			int i;
			QString sp = s.mid(dbsyncsit->path.length()); //now relative to sync
			QString ss = sp; //now relative to sync
			i = ss.indexOf("/");
			f.parent = -dbsyncsit->id;
			f.name = qPrintable(ss.left(i));
			rc = db_select_file_name(&f);
			if(rc) MC_ERR_MSG(rc,"Could not find file in db");
			ss = ss.mid(i+1);
			i = ss.indexOf("/");
			while(i != -1){
				f.parent = f.id;
				f.name = qPrintable(ss.left(i));
				rc = db_select_file_name(&f);
				if(rc) MC_ERR_MSG(rc,"Could not find file in db");
				ss = ss.mid(i+1);
				i = ss.indexOf("/");
			}
			sp = sp.left(sp.length()-1); //remove trailing /
			MC_NOTIFYSTART(MC_NT_SYNC,dbsyncsit->name);
			int wrc = walk_nochange(&context,qPrintable(sp),f.id,f.hash);
			if(wrc == MC_ERR_TERMINATING) dbsyncsit->status = MC_SYNCSTAT_ABORTED;
			else if(wrc == MC_ERR_CRYPTOALERT) return cryptopanic();
			else if(MC_IS_CRITICAL_ERR(wrc)) dbsyncsit->status = MC_SYNCSTAT_FAILED;
			else dbsyncsit->status = MC_SYNCSTAT_COMPLETED;
			dbsyncsit->lastsync = time(NULL); 

			rc = db_update_file(&f);
			MC_CHKERR(rc);

			rc = updateHash(&context,&f,&*dbsyncsit);
			MC_CHKERR(rc);

			rc = db_update_sync(&*dbsyncsit); //Hash must be written back to watchsyncs
			MC_CHKERR(rc);
			
			if(wrc == MC_ERR_CRYPTOALERT) return cryptopanic();

			MC_NOTIFYEND(MC_NT_SYNC);

		}
		if(MC_TERMINATING()) return MC_ERR_TERMINATING;
	}

	//Check for sync
	rc = fullsync();
	if(!rc){
		//MC_INFL("Update completed, cloud is fully synced");
		MC_NOTIFY(MC_NT_FULLSYNC,TimeToString(time(NULL)));
	} else if(rc == MC_ERR_NOTFULLYSYNCED){
		//MC_INFL("Update completed");
	} else return rc;
	startRemoteWatch();
	startLocalWatch();
	return 0;
}


int QtWatcher::remoteChange(int status){
	int rc,id;

	if(!status){
		rc = srv_notifychange_process(&netobuf,&id);
		if(rc == MC_ERR_NOCHANGE){ startRemoteWatch(); return 0; }
		if(rc){
			MC_WRN("retrying in 30 sec");
			restarttimer.start();
			return 0;
		}
		MC_INF("Remote change detected");
		mc_sync_ctx context;
		list<mc_sync_db> newsyncs;
		list<mc_sync_db>::iterator dbsyncsit,dbsyncsend;
		//id changed
		stopLocalWatch();
		//find sync
		watchsyncs->clear();
		rc = db_list_sync(&newsyncs);
		MC_CHKERR(rc);
		for(mc_sync_db& s : newsyncs){
			if(s.status != MC_SYNCSTAT_DISABLED && s.status != MC_SYNCSTAT_CRYPTOFAIL) watchsyncs->push_back(s);
		}
		dbsyncsit = watchsyncs->begin();
		dbsyncsend = watchsyncs->end();
		for(;dbsyncsit != dbsyncsend; ++dbsyncsit){
			if(id == dbsyncsit->id 
				&& dbsyncsit->status != MC_SYNCSTAT_DISABLED
				&& dbsyncsit->status != MC_SYNCSTAT_CRYPTOFAIL) break;
		}
		if(dbsyncsit == dbsyncsend) MC_ERR_MSG(MC_ERR_SERVER,"Remote Change Notify for non-watched sync");
		//Filters
		list<mc_filter> generalfilter,filter;
		rc = db_list_filter_sid(&generalfilter,0);
		MC_CHKERR(rc);

		filter.assign(generalfilter.begin(),generalfilter.end());
		rc = db_list_filter_sid(&filter,dbsyncsit->id);
		MC_CHKERR(rc);

		//Run
		init_sync_ctx(&context,&*dbsyncsit,&filter);
		MC_NOTIFYSTART(MC_NT_SYNC,dbsyncsit->name);
		int wrc = walk(&context,"",-dbsyncsit->id,dbsyncsit->hash);
		if(wrc == MC_ERR_TERMINATING) dbsyncsit->status = MC_SYNCSTAT_ABORTED;
		else if(wrc == MC_ERR_CRYPTOALERT) dbsyncsit->status = MC_SYNCSTAT_CRYPTOFAIL;
		else if(MC_IS_CRITICAL_ERR(wrc)) dbsyncsit->status = MC_SYNCSTAT_FAILED;
		else dbsyncsit->status = MC_SYNCSTAT_COMPLETED;
		dbsyncsit->lastsync = time(NULL);
		rc = db_update_sync(&*dbsyncsit);
		MC_CHKERR(rc);
		if(wrc == MC_ERR_CRYPTOALERT) return cryptopanic();
		MC_NOTIFYEND(MC_NT_SYNC);
		
		if(MC_TERMINATING()) return MC_ERR_TERMINATING;

		//Check for sync
		rc = fullsync();
		if(!rc){
			//MC_INFL("Update completed, cloud is fully synced");
			MC_NOTIFY(MC_NT_FULLSYNC,TimeToString(time(NULL)));
		} else if(rc == MC_ERR_NOTFULLYSYNCED){
			//MC_INFL("Update completed");
		} else return rc;
		startLocalWatch();
		startRemoteWatch();
	} else return status;
	return 0;
}



int add_dir(string path, int id, QStringList *l, int rdepth){
	string fpath;
	int rc;
	MC_DBG("Adding " << path << " to watchlist");
	l->append(path.c_str());
	if(rdepth <= MC_MAXRDEPTH){
		list<mc_file> files;
		rc = db_list_file_parent(&files,id);
		MC_CHKERR(rc);
		for(mc_file& f : files){
			if(f.is_dir && f.status != MC_FILESTAT_DELETED){
				fpath.assign(path).append(f.name).append("/");
				rc = add_dir(fpath,f.id,l,rdepth+1);
				MC_CHKERR(rc);
			} else if(f.status != MC_FILESTAT_DELETED){
				//fpath.assign(path).append(f.name);
				//MC_DBGL("Adding " << fpath << " to watchlist");
				//l->append(fpath.c_str());
			}
		}
	}
	return 0;
}

int enter_watchmode(int timeout){
	QStringList l;
	list<mc_sync_db> dbsyncs,watchlist;// = list<mc_sync_db>();
	list<mc_sync_db>::iterator dbsyncsit,dbsyncsend;
	mc_status status;
	int rdepth;
	int rc;
	MC_INFL("Entering Watchmode for " << timeout << " secs");
	
	rc = db_select_status(&status);
	MC_CHKERR(rc);

	rc = db_list_sync(&dbsyncs);
	MC_CHKERR(rc);

	dbsyncs.sort(compare_mc_sync_db); //TODO: why?
	dbsyncsit = dbsyncs.begin();
	dbsyncsend = dbsyncs.end();
	for(;dbsyncsit != dbsyncsend; ++dbsyncsit){ //Foreach db sync
		if(dbsyncsit->status != MC_SYNCSTAT_DISABLED && dbsyncsit->status != MC_SYNCSTAT_CRYPTOFAIL){
			rdepth = 0;
			rc = add_dir(dbsyncsit->path,-dbsyncsit->id,&l,rdepth);
			MC_CHKERR(rc);
			watchlist.push_back(*dbsyncsit);
		}
	}
	if(l.size() == 0){
		MC_WRN("Nothing to watch, sleeping");
		mc_sleep_checkterminate(timeout);
	} else {
		QtWatcher w(l,&watchlist,status.url.c_str(),CAFILE,status.acceptallcerts);
		w.run(timeout);
	}

	return 0;
}

#endif /* MC_WATCHMODE */