#include "mc_watch.h"
#ifdef MC_WATCHMODE
#include "mc_util.h"
#include "mc_helpers.h"
#include "mc_fs.h"
#include "mc_walk.h"
#include "mc_filter.h"

QtWatcher *QtWatcher::_instance = NULL;

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

#pragma region QtLocalWatcher

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
	if (dirpath.substr(dirpath.length() - 2) == '/') {
		dirpath = dirpath.substr(0, dirpath.length() - 1);
	}
#endif

	// find corresponding sync(s)
	for (mc_sync_db& sync : this->syncs) {
		if (dirpath.substr(0, sync.path.length()) == sync.path) {
			MC_DBGL(sync.name << " -> " << dirpath.substr(sync.path.length()) << " changed");
			emit pathChanged(sync, dirpath.substr(sync.path.length()));
			emit pathChanged(sync, QString(dirpath.substr(sync.path.length()).c_str()));
		}
	}
}

#ifndef MC_OS_WIN
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

#pragma endregion

#ifdef MC_OS_WIN
#pragma region QtFileSystemWatcher

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

#pragma endregion
#endif /* MC_OS_WIN */

#pragma region QtRemoteWatcher

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
	restarttimer.stop();
	return srv_notifychange_async(&netibuf, &netobuf, performer, syncs);
}

void QtRemoteWatcher::stop() {
	performer->abort();
	restarttimer.stop();
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

#pragma endregion


QtWatcher::QtWatcher(const QString& url, const QString& certfile, bool acceptall) :
	remoteWatcher(url, certfile, acceptall),
	localWatcher()
{
	Q_ASSERT_X((QtWatcher::_instance == NULL), "QtWatcher", "There should only be one QtWatcher Instance");
	QtWatcher::_instance = this;

	connect(&localWatcher, SIGNAL(pathChanged(const mc_sync_db&, const string&)), this, SLOT(localChange(const mc_sync_db&, const string&)));
	connect(&remoteWatcher, SIGNAL(syncChanged(const mc_sync_db&)), this, SLOT(remoteChange(const mc_sync_db&)));

	delaytimer.setSingleShot(true);
	delaytimer.setInterval(5000);
	connect(&delaytimer, SIGNAL(timeout()), this, SLOT(localChangeTimeout()));

	excludetimer.setSingleShot(true);
	excludetimer.setInterval(100);
	connect(&excludetimer, SIGNAL(timeout()), this, SLOT(endExcludingTimeout()));

	quittimer.setInterval(2000);
	connect(&quittimer, SIGNAL(timeout()), this, SLOT(checkquit()));

	timetimer.setSingleShot(true);
	connect(&timetimer, SIGNAL(timeout()), &loop, SLOT(quit()));

	watching = false;
}

QtWatcher::~QtWatcher() {
	QtWatcher::_instance = NULL;
}

int QtWatcher::setScope(const list<mc_sync_db>& syncs) {
	int rc;

	this->syncs.clear();

	//Filters
	list<mc_filter> generalfilter;
	rc = db_list_filter_sid(&generalfilter, 0);
	MC_CHKERR(rc);

	for (const mc_sync_db& sync : syncs) {
		if (sync.status != MC_SYNCSTAT_DISABLED && sync.status != MC_SYNCSTAT_CRYPTOFAIL) {
			this->syncs.push_back(sync);
			this->changedPaths[sync.id] = QStringList();
			this->filters[sync.id] = list<mc_filter>();

			this->filters[sync.id].assign(generalfilter.begin(), generalfilter.end());
			rc = db_list_filter_sid(&this->filters[sync.id], sync.id);
			MC_CHKERR(rc);
		}
	}

	rc = localWatcher.setScope(this->syncs);
	MC_CHKERR(rc);
	remoteWatcher.setScope(this->syncs);

	excludedPaths.clear();
	changedPaths.clear();

	return 0;
}

void QtWatcher::beginExcludingLocally(const QString& path) {
	// always exclude the whole directory, create/delete modifies its mtime
	exclusionMutex.lock();
	MC_DBG("Excluding " << qPrintable(path.mid(0, path.lastIndexOf("/"))));
	excludedPaths.push_back(path.mid(0, path.lastIndexOf("/")));
	exclusionMutex.unlock();
}

void QtWatcher::endExcludingLocally(const QString& path) {
	QString p = path.mid(0, path.lastIndexOf("/"));
	// unfortunately we don't get the notify immediately when the operation happens in the fs
	// so we have to wait a moment before actually un-excluding
	pendingUnExcludes.push_back(p);
	excludetimer.start();
}

void QtWatcher::endExcludingTimeout() {
	exclusionMutex.lock();
	for (const QString& p : pendingUnExcludes) {
		MC_DBG("No longer excluding " << qPrintable(p));
		excludedPaths.removeOne(p);
	}
	exclusionMutex.unlock();
}

bool QtWatcher::isExcludingLocally(const QString& path) {
	QString p = path.mid(0, path.lastIndexOf("/"));
	return excludedPaths.contains(p);
}

void QtWatcher::checkquit() {
	if (MC_TERMINATING()) {
		MC_INF("Abort from GUI");
		remoteWatcher.stop();
		loop.quit();
	}
}

int QtWatcher::localChange(const mc_sync_db& sync, const string& path) {
	crypt_seed(time(NULL) ^ path.length() * 137351);
	QString _path(path.c_str());
	QString fpath(sync.path.c_str());
	fpath.append(_path);

	for (const QString& p : excludedPaths) {
		if (fpath.startsWith(p)) {
			return 0;
		}
	}

	// match against normal filters
	int p = _path.lastIndexOf("/") + 1;
	QString dirpath = _path.left(p);
	QString name = _path.mid(p);
	string _fpath = qPrintable(fpath);
	mc_file_fs fs;
	if (fs_exists(_fpath)) {
		// shortlived files may be gone by the time we check
		int rc = fs_filestats(&fs, _fpath, qPrintable(name), true);
		MC_CHKERR(rc)
	} else {
		fs.name = qPrintable(name);
		fs.is_dir = true; // speculation
	}
	if (match_full(qPrintable(dirpath), &fs, &this->filters[sync.id])) {
		return 0;
	}
	// the containing directory might already be ignored...
	while (p != 0) {
		dirpath = dirpath.left(dirpath.length() - 1);
		p = dirpath.lastIndexOf("/") + 1;
		name = dirpath.mid(p);
		dirpath = dirpath.left(p);
		fs.is_dir = true;
		fs.name = qPrintable(name);
		if (match_full(qPrintable(dirpath), &fs, &this->filters[sync.id])) {
			return 0;
		}
	}

	MC_INFL("Changed: " << sync.name << " > " << path);
	changedPaths[sync.id].push_back(_path);

	if (watching)
		delaytimer.start();

	return 0;
}

int QtWatcher::localChangeTimeout() {
	int rc, i;
	mc_sync_ctx context;
	list<mc_sync_db> newsyncs;

	MC_INF("Local change detected");

	list<mc_filter> generalfilter, filter;
	rc = db_list_filter_sid(&generalfilter, 0);
	MC_CHKERR(rc);

	watching = false;
	delaytimer.stop();
	remoteWatcher.stop();
	for (mc_sync_db& sync : syncs) {
		auto changepaths = changedPaths[sync.id];
		changedPaths[sync.id].clear();

		if (changepaths.empty())
			continue;
		
		//update filters
		filter.assign(generalfilter.begin(), generalfilter.end());
		rc = db_list_filter_sid(&filter, sync.id);
		MC_CHKERR(rc);

		changepaths.sort();
		while (!changepaths.empty()) {
			QString s = changepaths.takeFirst();
			//remove all subpaths
			for (i = 0; i < changepaths.length(); i++) if (changepaths[i].startsWith(s)) { changepaths.removeAt(i); i--; }


			init_sync_ctx(&context, &sync, &filter);
			if (s.length() == 0) { //it's a sync itself
				MC_NOTIFYSTART(MC_NT_SYNC, sync.name);
				rc = walk_nochange(&context, "", -sync.id, sync.hash);
				if (rc == MC_ERR_TERMINATING) sync.status = MC_SYNCSTAT_ABORTED;
				else if (rc == MC_ERR_CRYPTOALERT) return cryptopanic();
				else if (MC_IS_CRITICAL_ERR(rc)) sync.status = MC_SYNCSTAT_FAILED;
				else sync.status = MC_SYNCSTAT_COMPLETED;
				sync.lastsync = time(NULL); //TODO: not always a full sync!
				rc = db_update_sync(&sync);
				MC_CHKERR(rc);
				MC_NOTIFYEND(MC_NT_SYNC);
			} else {
				mc_file f;
				int i;
				QString sp = s;
				QString ss = s;
				// for the first round, the parent is the sync
				f.id = -sync.id;

				i = 0;
				while (i != -1) {
					int parent = f.parent;
					i = ss.indexOf("/");
					f.parent = f.id;
					f.name = qPrintable(ss.left(i));
					rc = db_select_file_name(&f);
					if (rc == SQLITE_DONE) {
						// if the file is new / not found, walk the parent
						// to do so, reload the parent from db
						rc = db_select_file_id(&f);
						if (rc == SQLITE_DONE)
						{
							// great, it's the root dir aka the sync
							f.id = -sync.id;
							f.is_dir = true;
							sp = "";
						} else MC_CHKERR(rc);
						break;
					}
					if (rc) MC_ERR_MSG(rc, "Could not find file in db");
					ss = ss.mid(i + 1);
				}
				MC_NOTIFYSTART(MC_NT_SYNC, sync.name);
				if (f.is_dir) {
					int wrc = walk_nochange(&context, qPrintable(sp), f.id, f.hash);
					if (wrc == MC_ERR_TERMINATING) sync.status = MC_SYNCSTAT_ABORTED;
					else if (wrc == MC_ERR_CRYPTOALERT) return cryptopanic();
					else if (MC_IS_CRITICAL_ERR(wrc)) sync.status = MC_SYNCSTAT_FAILED;
					else sync.status = MC_SYNCSTAT_COMPLETED;
				} else {
					mc_file_fs fs;
					mc_file_fs* upload_fs;
					string fpath(sync.path.c_str());
					fpath.append(qPrintable(sp));
					if (fs_exists(fpath)) {
						rc = fs_filestats(&fs, fpath, f.name);
						MC_CHKERR(rc);
						upload_fs = &fs;
					} else {
						upload_fs = NULL;
					}
					if (!upload_fs || upload_fs->mtime != f.mtime || f.status != MC_FILESTAT_COMPLETE) {
						sp = sp.left(sp.lastIndexOf("/") + 1); // upload expects the path only up to the dir
						rc = upload(&context, qPrintable(sp), upload_fs, &f, &f, NULL);
						if (rc == MC_ERR_CRYPTOALERT) return cryptopanic();
						else if (MC_IS_CRITICAL_ERR(rc)) return rc;
					}
				}
				sync.lastsync = time(NULL);

				rc = db_update_file(&f);
				MC_CHKERR(rc);

				rc = updateHash(&context, &f, &sync);
				MC_CHKERR(rc);

				rc = db_update_sync(&sync);
				MC_CHKERR(rc);

				MC_NOTIFYEND(MC_NT_SYNC);
			}

			if (MC_TERMINATING()) return MC_ERR_TERMINATING;
		}
	}

	//Check for sync
	rc = fullsync();
	if (!rc) {
		MC_NOTIFY(MC_NT_FULLSYNC, TimeToString(time(NULL)));
	} else if (rc == MC_ERR_NOTFULLYSYNCED) {
	} else return rc;

	watching = true;
	if (localChangesPending())
		delaytimer.start();

	remoteWatcher.setScope(syncs);
	remoteWatcher.startSingle();

	return 0;
}

int QtWatcher::remoteChange(const mc_sync_db& sync) {
	int rc;

	mc_sync_db* writableSync = findMine(sync);
	if (!writableSync) MC_ERR_MSG(MC_ERR_SERVER, "Remote change notification for non-watched sync");;

	MC_INF("Remote change detected");
	mc_sync_ctx context;
	list<mc_sync_db> newsyncs;

	watching = false;
	delaytimer.stop();

	//Run
	init_sync_ctx(&context, writableSync, &this->filters[writableSync->id]);
	MC_NOTIFYSTART(MC_NT_SYNC, writableSync->name);
	int wrc = walk(&context, "", -writableSync->id, writableSync->hash);
	if (wrc == MC_ERR_TERMINATING) writableSync->status = MC_SYNCSTAT_ABORTED;
	else if (wrc == MC_ERR_CRYPTOALERT) writableSync->status = MC_SYNCSTAT_CRYPTOFAIL;
	else if (MC_IS_CRITICAL_ERR(wrc)) writableSync->status = MC_SYNCSTAT_FAILED;
	else writableSync->status = MC_SYNCSTAT_COMPLETED;
	writableSync->lastsync = time(NULL);
	rc = db_update_sync(writableSync);
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

	watching = true;
	if (localChangesPending())
		delaytimer.start();

	remoteWatcher.setScope(syncs);
	remoteWatcher.startSingle();
	return 0;
}

int QtWatcher::catchUpAndWatch(int timeout) {
	int rc;
	MC_INFL("Entering Watchmode for " << timeout << " secs");
	watching = true;

	// before we start, make sure we have the latest hashes
	for (mc_sync_db &sync : this->syncs)
	{
		rc = db_select_sync(&sync);
		MC_CHKERR(rc);
	}
	remoteWatcher.setScope(syncs);

	timetimer.setInterval(timeout * 1000);
	timetimer.start();

	quittimer.start();

	if (localChangesPending())
		localChangeTimeout(); // starts remoteWatcher when done
	else
		remoteWatcher.startSingle();

	loop.exec();
	remoteWatcher.stop();

	delaytimer.stop();
	changedPaths.clear();
	quittimer.stop();

	watching = false;
	return 0;
}

mc_sync_db* QtWatcher::findMine(const mc_sync_db& sync) {
	for (mc_sync_db& s : syncs) {
		if (s.id == sync.id) {
			return &s;
		}
	}
	return nullptr;
}

bool QtWatcher::localChangesPending() {
	for (mc_sync_db& sync : syncs) {
		auto changepaths = changedPaths[sync.id];

		if (!changepaths.empty())
			return true;
	}
	return false;
}

#endif /* MC_WATCHMODE */