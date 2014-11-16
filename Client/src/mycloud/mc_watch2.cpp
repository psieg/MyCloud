#include "mc_watch2.h"
#ifdef MC_WATCHMODE
#include "mc_util.h"
#include "mc_fs.h"


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

int QtLocalWatcher::setScope(list<mc_sync_db>& syncs) {
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

				if (dwAction & (FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE))
					MC_INF("non-namechange-change! " << path)
				MC_DBGL(path << " has changed");
				emit pathChanged(path);
			}
		}
	}
}
#endif /* MC_OS_WIN */

QtRemoteWatcher::QtRemoteWatcher(const QString& url, const QString& certfile, bool acceptall) {
	syncs = nullptr;

	SetBuf(&netibuf);
	SetBuf(&netobuf);

	QString _url = "https://";
	_url.append(url);
	_url.append("/bin.php");
	performer = new QtNetworkPerformer(_url, certfile, acceptall, true, 40); //30sec is half-open time
	connect(performer, SIGNAL(finished(int)), this, SLOT(remoteChange(int)));
}

QtRemoteWatcher::~QtRemoteWatcher() {
	if (performer)
		delete performer;
	ClearBuf(&netibuf);
	ClearBuf(&netobuf);
}

void QtRemoteWatcher::setScope(list<mc_sync_db>* syncs) {
	this->syncs = syncs;
}

int QtRemoteWatcher::startSingle() {
	return srv_notifychange_async(&netibuf, &netobuf, performer, syncs);
}

void QtRemoteWatcher::stop() {
	performer->abort();
}

void QtRemoteWatcher::remoteChange(int status) {
	
}



/*
QtWatcher2::QtWatcher2() {
	connect(&thread, SIGNAL(pathChanged(const string&)), this, SLOT(pathChanged(const string&)));
}

QtWatcher2::~QtWatcher2() {
	thread.quit();
}

void QtWatcher2::clearAndSetScope(list<mc_sync>& syncs) {
	thread.setScope(syncs);
	excludedPaths.clear();
	changedPaths.clear();

	thread.start();
}
void QtWatcher2::beginExcluding(const string& path) {
	//Q_ASSERT_X(std::find(excludedPaths.begin(), excludedPaths.end(), path) == excludedPaths.end(), "QtWatcher", "File is already excluded");
	//excludedPaths.push_back(path);
}

void QtWatcher2::endExcluding(const string& path) {
	//excludedPaths.remove(path);
}

void QtWatcher2::pathChanged(const string& path) {

}

int QtWatcher2::catchUpAndWatch(int timeout) {
	return 0;
}
*/
#endif /* MC_WATCHMODE */