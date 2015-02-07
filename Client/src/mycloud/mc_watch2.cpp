#include "mc_watch2.h"
#ifdef MC_WATCHMODE
#include "mc_util.h"
#include "mc_helpers.h"
#include "mc_fs.h"
#include "mc_walk.h"

#ifdef MC_QTCLIENT
#include "qtClient.h"
#endif

#ifdef MC_OS_WIN

#define INTERESTING_FILE_NOTIFY_CHANGES FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE

#include "ReadDirectoryChanges/ReadDirectoryChanges.h"

#else

#include <QtCore/QFileSystemWatcher>
#define MC_MAXRDEPTH	3

#endif

QtLocalWatcher::QtLocalWatcher() {
#ifdef MC_OS_WIN
	watcher = new QtFileSystemWatcher();
	connect(watcher->toQObject(), SIGNAL(pathChanged(const string&)), this, SLOT(pathChanged(const string&)));
#else
	watcher = new QFileSystemWatcher();
	connect(watcher, SIGNAL(directoryChanged(const QString &)), this, SLOT(pathChanged(const QString &)));
	connect(watcher, SIGNAL(fileChanged(const QString &)), this, SLOT(pathChanged(const QString &)));
#endif
}

QtLocalWatcher::~QtLocalWatcher() {
	delete watcher;
}

int QtLocalWatcher::setScope(const list<mc_sync_db>& syncs) {
	this->syncs.assign(syncs.begin(), syncs.end());
	list<string> paths;

	// de-duplicate paths to keep the number of duplicate events down
	for (mc_sync_db& sync : this->syncs) {
		bool found = false;
		for (string& path : paths) {
			if (path.substr(0, sync.path.length()) == sync.path) {
				// path is at least as deep as syncpath
				path.assign(sync.path);
				found = true;
				break;
			}
			if (sync.path.substr(0, path.length()) == path) {
				// syncpath is at deeper as path
				found = true;
				break;
			}
		}
		if (!found) {
			paths.push_back(sync.path);
		}
	}

#ifdef MC_OS_WIN
	watcher->setScope(paths);
#else
	QStringList watchpaths;
	for (string& path : paths) {
		int rc = recurseDirectory(path, &watchpaths, 0);
		MC_CHKERR(rc);
	}

	watcher->removePaths(watcher->directories());
	watcher->addPaths(watchpaths);
#endif

	return 0;
}

#ifdef MC_OS_WIN
void QtLocalWatcher::pathChanged(const string& path) {
	string dirpath = path;
#else
void QtLocalWatcher::pathChanged(const QString& path) {
	string dirpath(qPrintable(path));
#endif

	// we are only interested in directories
	if (!fs_isdir(dirpath)) {
		size_t index = dirpath.find_last_of('/');
		if (index != dirpath.length() - 1) {
			dirpath = dirpath.substr(0, dirpath.find_last_of('/') + 1);
		}
	} else {
		dirpath.append("/");
	}

	// find corresponding sync(s)
	for (mc_sync_db& sync : this->syncs) {
		if (dirpath.substr(0, sync.path.length()) == sync.path) {
			MC_DBGL(sync.name << " -> " << dirpath.substr(sync.path.length()) << " changed");
			emit pathChanged(sync, dirpath.substr(sync.path.length()));
			emit pathChanged(sync, QString(dirpath.substr(sync.path.length()).c_str()));
		}
	}
}

#ifdef MC_OS_UNIX
int QtLocalWatcher::recurseDirectory(string path, QStringList *l, int rdepth) {
	string fpath;
	int rc;
	MC_DBGL("Adding " << path << " to watchlist");
	l->push_back(path.c_str());
	if (rdepth <= MC_MAXRDEPTH) {
		list<mc_file_fs> files;
		rc = fs_listdir(&files, path);
		MC_CHKERR(rc);
		for (mc_file_fs& f : files) {
			if (f.is_dir) {
				fpath.assign(path).append(f.name).append("/");
				rc = recurseDirectory(fpath, l, rdepth + 1);
				MC_CHKERR(rc);
			}
		}
	}
	return 0;
}
#endif


#ifdef MC_OS_WIN
QtFileSystemWatcher::QtFileSystemWatcher() {
	qRegisterMetaType<string>("string");
	qRegisterMetaType<mc_sync_db>("mc_sync_db");
	changes = nullptr;
	changesWaitHandle = NULL;
	waitForNewHandleEvent = CreateEvent(NULL, false, false, NULL);
	gotNewHandleEvent = CreateEvent(NULL, false, false, NULL);
	shouldQuitEvent = CreateEvent(NULL, false, false, NULL);
	hasQuitEvent = CreateEvent(NULL, false, false, NULL);
	start();
}

QtFileSystemWatcher::~QtFileSystemWatcher() {
	SetEvent(shouldQuitEvent);
	int rc = WaitForSingleObject(hasQuitEvent, 500);
	if (rc == WAIT_TIMEOUT) this->terminate();

	delete changes;

	CloseHandle(waitForNewHandleEvent);
	CloseHandle(gotNewHandleEvent);
	CloseHandle(shouldQuitEvent);
	CloseHandle(hasQuitEvent);
}


void QtFileSystemWatcher::setScope(list<string>& paths) {
	SetEvent(waitForNewHandleEvent);
	if (changes)
		delete changes;
	changes = new CReadDirectoryChanges();
	SetEvent(gotNewHandleEvent);
	
	for (string& path : paths) {
		string _path = path.substr(0, path.length() - 1);
		changes->AddDirectory(utf8_to_unicode(_path).c_str(), true, INTERESTING_FILE_NOTIFY_CHANGES);
	}
}

void QtFileSystemWatcher::run() {
	bool terminate = false;
	HANDLE handles[] = { shouldQuitEvent, waitForNewHandleEvent, NULL };
	while (!terminate) {
		int rc = WaitForMultipleObjects(
			handles[2] == NULL ? 2 : 3,
			handles,
			false,
			INFINITE);

		switch (rc) {
		case WAIT_OBJECT_0 + 0:
			terminate = true;
			break;
		case WAIT_OBJECT_0 + 1:
			WaitForSingleObject(gotNewHandleEvent, INFINITE);
			handles[2] = changes->GetWaitHandle();
			break;
		case WAIT_OBJECT_0 + 2:
			{
				DWORD dwAction;
				wstring wstrFilename;
				changes->Pop(dwAction, wstrFilename);
				string path = unicode_to_utf8(wstring(wstrFilename));
				std::replace(path.begin(), path.end(), '\\', '/'); // winapi likes backslashes, we don't

				MC_DBG_BRK(dwAction & (FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE), "non-namechange-change! " << path);
				MC_DBGL(path << " has changed");
				emit pathChanged(path);
			}
		}
	}
}
#endif /* MC_OS_WIN */

QtRemoteWatcher::QtRemoteWatcher(const QString& url, const QString& certfile, bool acceptall) {
	qRegisterMetaType<mc_sync_db>("mc_sync_db");

	SetBuf(&netibuf);
	SetBuf(&netobuf);

	QString _url = "https://";
	_url.append(url);
	_url.append("/bin.php");
	performer = new QtNetworkPerformer(_url, certfile, acceptall, true, 40); //30sec is half-open time
	connect(performer, SIGNAL(finished(int)), this, SLOT(remoteChange(int)));

	restarttimer.setSingleShot(true);
	restarttimer.setInterval(30000);
	connect(&restarttimer, SIGNAL(timeout()), this, SLOT(startSingle()));
}

QtRemoteWatcher::~QtRemoteWatcher() {
	if (performer)
		delete performer;
	ClearBuf(&netibuf);
	ClearBuf(&netobuf);
}

void QtRemoteWatcher::setScope(const list<mc_sync_db>& syncs) {
	this->syncs.assign(syncs.begin(), syncs.end());
}

int QtRemoteWatcher::startSingle() {
	return srv_notifychange_async(&netibuf, &netobuf, performer, syncs);
}

void QtRemoteWatcher::stop() {
	performer->abort();
}

void QtRemoteWatcher::remoteChange(int status) {
	int rc, id;

	if (!status) {
		rc = srv_notifychange_process(&netobuf, &id);
		if (rc == MC_ERR_NOCHANGE) {
			startSingle();
			return; 
		}
		if (rc) {
			MC_WRN("retrying in 30 sec");
			restarttimer.start();
			return;
		}

		for (mc_sync_db& sync : syncs)
		{
			if (sync.id == id)
			{
				emit syncChanged(sync);
				return;
			}
		}
	}
}



QtWatcher2::QtWatcher2(const QString& url, const QString& certfile, bool acceptall) :
	remoteWatcher(url, certfile, acceptall),
	localWatcher()
{
	moveToThread(this);

	connect(&localWatcher, SIGNAL(pathChanged(const mc_sync_db&, const string&)), this, SLOT(localChange(const mc_sync_db&, const string&)));
	connect(&remoteWatcher, SIGNAL(syncChanged(const mc_sync_db&)), this, SLOT(remoteChange(const mc_sync_db&)));

	delaytimer.setSingleShot(true);
	delaytimer.setInterval(5000);
	connect(&delaytimer, SIGNAL(timeout()), this, SLOT(changeTimeout()));

	quittimer.setInterval(2000);
	connect(&quittimer, SIGNAL(timeout()), this, SLOT(checkquit()));

	start();
}

QtWatcher2::~QtWatcher2() {
	QMetaObject::invokeMethod(this, "quit", Qt::BlockingQueuedConnection);
}

int QtWatcher2::setScope(const list<mc_sync_db>& syncs) {
	this->syncs.clear();
	for (const mc_sync_db& sync : syncs) {
		if (sync.status != MC_SYNCSTAT_DISABLED && sync.status != MC_SYNCSTAT_CRYPTOFAIL) {
			this->syncs.push_back(sync);
			this->changedPaths[sync] = list<QString>();
		}
	}

	int rc;
	rc = localWatcher.setScope(this->syncs);
	MC_CHKERR(rc);
	remoteWatcher.setScope(this->syncs);

	excludedPaths.clear();
	changedPaths.clear();

	return 0;
}
void QtWatcher2::beginExcludingLocally(const QString& path) {
	excludedPaths.push_back(path);
}

void QtWatcher2::endExcludingLocally(const QString& path) {
	// remove only the first occurence in case they were stacked
	for (list<QString>::iterator it = excludedPaths.begin(); ; ++it) {
		if (*it == path) {
			excludedPaths.erase(it);
			return;
		}		
	}
}

void QtWatcher2::checkquit() {
	if (MC_TERMINATING()) {
		MC_INF("Abort from GUI");
		remoteWatcher.stop();
		loop.quit();
	}
}

int QtWatcher2::localChange(const mc_sync_db& sync, const string& path) {
	crypt_seed(time(NULL) ^ path.length() * 137351);
	QString _path(path.c_str());

	for (const QString& p : excludedPaths) {
		if (_path.startsWith(p)) {
			return 0;
		}
	}

	mc_sync_db* writableSync = findMine(sync);
	if (!writableSync) MC_ERR_MSG(MC_OK, "Local change notification for non-watched sync");

	changedPaths[*writableSync].push_back(_path);

	delaytimer.start();

	return 0;
}

int QtWatcher2::localChangeTimeout() {
	int rc, i, lastsyncid;
	mc_sync_ctx context;
	list<mc_sync_db> newsyncs;
	list<mc_sync_db>::iterator dbsyncsit, dbsyncsend;

	MC_INF("Local change detected");

	for (mc_sync_db& sync : syncs) {
		if (changedPaths[sync].empty())
			continue;

		changedPaths[sync].sort();

	}/*
	//changepaths is never empty if this was triggered
	list<mc_filter> generalfilter, filter;
	rc = db_list_filter_sid(&generalfilter, 0);
	MC_CHKERR(rc);

	// we can assume srv is still open and authed
	lastsyncid = 0;

	while (!changepaths.empty()) {
		QString s = changepaths.takeFirst();
		//remove all subpaths
		for (i = 0; i<changepaths.length(); i++) if (changepaths[i].startsWith(s)) { changepaths.removeAt(i); i--; }
		//find sync
		watchsyncs->clear();
		rc = db_list_sync(&newsyncs);
		MC_CHKERR(rc);
		for (mc_sync_db& s : newsyncs) {
			if (s.status != MC_SYNCSTAT_DISABLED || s.status != MC_SYNCSTAT_CRYPTOFAIL) watchsyncs->push_back(s);
		}
		dbsyncsit = watchsyncs->begin();
		dbsyncsend = watchsyncs->end();
		for (; dbsyncsit != dbsyncsend; ++dbsyncsit) {
			if (s.startsWith(dbsyncsit->path.c_str())
				&& dbsyncsit->status != MC_SYNCSTAT_DISABLED
				&& dbsyncsit->status != MC_SYNCSTAT_CRYPTOFAIL) break;
		}
		if (dbsyncsit == dbsyncsend) MC_ERR_MSG(-1, "Path " << qPrintable(s) << " not in any watched sync");

		//update filters
		if (lastsyncid != dbsyncsit->id) {
			filter.assign(generalfilter.begin(), generalfilter.end());
			rc = db_list_filter_sid(&filter, dbsyncsit->id);
			MC_CHKERR(rc);
			lastsyncid = dbsyncsit->id;
		}
		//find file

		init_sync_ctx(&context, &*dbsyncsit, &filter);
		if (s == dbsyncsit->path.c_str()) { //it's a sync itself
			MC_NOTIFYSTART(MC_NT_SYNC, dbsyncsit->name);
			rc = walk_nochange(&context, "", -dbsyncsit->id, dbsyncsit->hash);
			if (rc == MC_ERR_TERMINATING) dbsyncsit->status = MC_SYNCSTAT_ABORTED;
			else if (rc == MC_ERR_CRYPTOALERT) return cryptopanic();
			else if (MC_IS_CRITICAL_ERR(rc)) dbsyncsit->status = MC_SYNCSTAT_FAILED;
			else dbsyncsit->status = MC_SYNCSTAT_COMPLETED;
			dbsyncsit->lastsync = time(NULL); //TODO: not always a full sync!
			rc = db_update_sync(&*dbsyncsit);
			MC_CHKERR(rc);
			MC_NOTIFYEND(MC_NT_SYNC);
		}
		else {
			mc_file f;
			int i;
			QString sp = s.mid(dbsyncsit->path.length()); //now relative to sync
			QString ss = sp; //now relative to sync
			i = ss.indexOf("/");
			f.parent = -dbsyncsit->id;
			f.name = qPrintable(ss.left(i));
			rc = db_select_file_name(&f);
			if (rc) MC_ERR_MSG(rc, "Could not find file in db");
			ss = ss.mid(i + 1);
			i = ss.indexOf("/");
			while (i != -1) {
				f.parent = f.id;
				f.name = qPrintable(ss.left(i));
				rc = db_select_file_name(&f);
				if (rc) MC_ERR_MSG(rc, "Could not find file in db");
				ss = ss.mid(i + 1);
				i = ss.indexOf("/");
			}
			sp = sp.left(sp.length() - 1); //remove trailing /
			MC_NOTIFYSTART(MC_NT_SYNC, dbsyncsit->name);
			int wrc = walk_nochange(&context, qPrintable(sp), f.id, f.hash);
			if (wrc == MC_ERR_TERMINATING) dbsyncsit->status = MC_SYNCSTAT_ABORTED;
			else if (wrc == MC_ERR_CRYPTOALERT) return cryptopanic();
			else if (MC_IS_CRITICAL_ERR(wrc)) dbsyncsit->status = MC_SYNCSTAT_FAILED;
			else dbsyncsit->status = MC_SYNCSTAT_COMPLETED;
			dbsyncsit->lastsync = time(NULL);

			rc = db_update_file(&f);
			MC_CHKERR(rc);

			rc = updateHash(&context, &f, &*dbsyncsit);
			MC_CHKERR(rc);

			rc = db_update_sync(&*dbsyncsit); //Hash must be written back to watchsyncs
			MC_CHKERR(rc);

			if (wrc == MC_ERR_CRYPTOALERT) return cryptopanic();

			MC_NOTIFYEND(MC_NT_SYNC);

		}
		if (MC_TERMINATING()) return MC_ERR_TERMINATING;
	}

	//Check for sync
	rc = fullsync();
	if (!rc) {
		//MC_INFL("Update completed, cloud is fully synced");
		MC_NOTIFY(MC_NT_FULLSYNC, TimeToString(time(NULL)));
	}
	else if (rc == MC_ERR_NOTFULLYSYNCED) {
		//MC_INFL("Update completed");
	}
	else return rc;
	startRemoteWatch();
	startLocalWatch();
	return 0;*/
	return 0;
}

int QtWatcher2::remoteChange(const mc_sync_db& sync) {
	int rc;

	mc_sync_db* writableSync = findMine(sync);
	if (!writableSync) MC_ERR_MSG(MC_ERR_SERVER, "Remote change notification for non-watched sync");;

	MC_INF("Remote change detected");
	mc_sync_ctx context;
	list<mc_sync_db> newsyncs;
	list<mc_sync_db>::iterator dbsyncsit, dbsyncsend;

	//Filters
	list<mc_filter> generalfilter, filter;
	rc = db_list_filter_sid(&generalfilter, 0);
	MC_CHKERR(rc);

	filter.assign(generalfilter.begin(), generalfilter.end());
	rc = db_list_filter_sid(&filter, dbsyncsit->id);
	MC_CHKERR(rc);

	//Run
	init_sync_ctx(&context, &*dbsyncsit, &filter);
	MC_NOTIFYSTART(MC_NT_SYNC, dbsyncsit->name);
	int wrc = walk(&context, "", -dbsyncsit->id, dbsyncsit->hash);
	if (wrc == MC_ERR_TERMINATING) dbsyncsit->status = MC_SYNCSTAT_ABORTED;
	else if (wrc == MC_ERR_CRYPTOALERT) dbsyncsit->status = MC_SYNCSTAT_CRYPTOFAIL;
	else if (MC_IS_CRITICAL_ERR(wrc)) dbsyncsit->status = MC_SYNCSTAT_FAILED;
	else dbsyncsit->status = MC_SYNCSTAT_COMPLETED;
	dbsyncsit->lastsync = time(NULL);
	rc = db_update_sync(&*dbsyncsit);
	MC_CHKERR(rc);
	if (wrc == MC_ERR_CRYPTOALERT) return cryptopanic();
	MC_NOTIFYEND(MC_NT_SYNC);

	if (MC_TERMINATING()) return MC_ERR_TERMINATING;

	//Check for sync
	rc = fullsync();
	if (!rc) {
		MC_NOTIFY(MC_NT_FULLSYNC, TimeToString(time(NULL)));
	} else if (rc == MC_ERR_NOTFULLYSYNCED) {
	} else return rc;

	remoteWatcher.startSingle();
	return 0;
}

int QtWatcher2::catchUpAndWatch(int timeout) {
	return 0;
}

void QtWatcher2::run() {
	QEventLoop loop;
	loop.exec();
}

mc_sync_db* QtWatcher2::findMine(const mc_sync_db& sync) {
	for (mc_sync_db& s : syncs) {
		if (s.id == sync.id) {
			return &s;
		}
	}
	return nullptr;
}

#endif /* MC_WATCHMODE */