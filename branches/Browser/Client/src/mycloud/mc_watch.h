#ifndef MC_WATCH_H
#define MC_WATCH_H
#include <mc.h>
#ifdef MC_WATCHMODE
#include "mc_srv.h"
#include <QtCore/QObject>
#include <QtCore/QThread>
#error
#ifdef MC_OS_WIN
class QtFileSystemWatcher;
class CReadDirectoryChanges;
#else
class QFileSystemWatcher;
#endif

class QtLocalWatcher : public QObject
{
	Q_OBJECT

public:
	QtLocalWatcher();
	~QtLocalWatcher();

	int setScope(const list<mc_sync_db>& syncs);

signals:
	void pathChanged(const mc_sync_db& sync, const string& path);
	// compat. do not use
	void pathChanged(const mc_sync_db& sync, const QString& path);

protected:
	list<mc_sync_db> syncs;

#ifdef MC_OS_WIN
	QtFileSystemWatcher* watcher;
protected slots:
	void pathChanged(const string& path);
#else
	QFileSystemWatcher* watcher;
protected slots:
	void pathChanged(const QString& path);
#endif
protected:
	int recurseDirectory(string path, QStringList *l, int rdepth);
};

#ifdef MC_OS_WIN
// Wrapper around ReadDirectoryChanges
class QtFileSystemWatcher : protected QThread
{
	Q_OBJECT

public:
	QtFileSystemWatcher();
	~QtFileSystemWatcher();

	inline const QObject* toQObject() const { return this; }

	void setScope(list<string>& paths);

signals:
	void pathChanged(const string& path);
	void pathChanged(const QString& path);

protected:
	virtual void run();

	CReadDirectoryChanges* changes;
	void* changesWaitHandle;
	void* waitForNewHandleEvent;
	void* gotNewHandleEvent;
	void* shouldQuitEvent;
	void* hasQuitEvent;
};
#endif

class QtRemoteWatcher : public QObject
{
	Q_OBJECT

public:
	QtRemoteWatcher(const QString& url, const QString& certfile, bool acceptall);
	~QtRemoteWatcher();

	void setScope(const list<mc_sync_db>& syncs);

	void stop();

signals:
	void syncChanged(const mc_sync_db& sync);

public slots:
	int startSingle();

protected slots:
	void remoteChange(int status);

protected:
	list<mc_sync_db> syncs;
	QtNetworkPerformer* performer;
	mc_buf netibuf, netobuf;
	QTimer restarttimer;

};


class QtWatcher : protected QObject
{
	Q_OBJECT

public:
	QtWatcher(const QString& url, const QString& certfile, bool acceptall);
	~QtWatcher();
	static QtWatcher* instance() { return _instance; }

	int setScope(const list<mc_sync_db>& syncs);
	int catchUpAndWatch(int timeout);

	void beginExcludingLocally(const QString& path);
	void endExcludingLocally(const QString& path);
	bool isExcludingLocally(const QString& path);

private slots:
	void checkquit();
	void endExcludingTimeout();
	int localChange(const mc_sync_db& sync, const string& path);
	int localChangeTimeout();
	int remoteChange(const mc_sync_db& sync);

private:
	static QtWatcher* _instance;
	mc_sync_db* findMine(const mc_sync_db& sync);
	bool localChangesPending();

	QtLocalWatcher localWatcher;
	QtRemoteWatcher remoteWatcher;
	QTimer quittimer, timetimer, delaytimer, excludetimer;
	QEventLoop loop;
	list<mc_sync_db> syncs;
	QMutex exclusionMutex;
	QStringList excludedPaths;
	QStringList pendingUnExcludes;
	map<int, QStringList> changedPaths;
	map<int, list<mc_filter>> filters;
	bool watching;
};

#define MC_BEGIN_WRITING_FILE(fpath)	if(QtWatcher::instance()) QtWatcher::instance()->beginExcludingLocally(fpath.c_str());
#define MC_END_WRITING_FILE(fpath)		if(QtWatcher::instance()) QtWatcher::instance()->endExcludingLocally(fpath.c_str());
#else
#define MC_BEGIN_WRITING_FILE(fpath)
#define MC_END_WRITING_FILE(fpath)
#endif /* MC_WATCHMODE */
#endif /* MC_WATCH_H */