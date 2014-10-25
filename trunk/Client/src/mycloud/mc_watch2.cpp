#include "mc_watch2.h"
#ifdef MC_WATCHMODE
#include "mc_util.h"


#ifdef MC_OS_WIN
#define INTERESTING_FILE_NOTIFY_CHANGES FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE  | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_LAST_ACCESS

#include "ReadDirectoryChanges/ReadDirectoryChanges.h"

QtFileSystemWatcher::QtFileSystemWatcher() {
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


void QtFileSystemWatcher::setScope(list<mc_sync_db>& syncs) {
	SetEvent(waitForNewHandleEvent);
	if (changes)
		delete changes;
	changes = new CReadDirectoryChanges();
	SetEvent(gotNewHandleEvent);
	
	for (mc_sync_db& sync : syncs) {
		string path = sync.path.substr(0, sync.path.length()-1);
		changes->AddDirectory(utf8_to_unicode(path).c_str(), true, INTERESTING_FILE_NOTIFY_CHANGES);
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
				CStringW wstrFilename;
				changes->Pop(dwAction, wstrFilename);
				string path = unicode_to_utf8(wstring(wstrFilename));
				std::replace(path.begin(), path.end(), '\\', '/'); // winapi likes backslashes, we don't

				// remove filename portion for now as we are linked to directoryChanged
				size_t index = path.find_last_of('/');
				if (index != path.length() - 1) {
					path = path.substr(0, path.find_last_of('/')+1);
				}

				emit pathChanged(path);
				emit pathChanged(QString(path.c_str()));
			}
		}
	}
}
#endif

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