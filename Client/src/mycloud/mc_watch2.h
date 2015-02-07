#ifndef MC_WATCH2_H
#define MC_WATCH2_H
#include <mc.h>
#ifdef MC_WATCHMODE
#include "mc_srv.h"
#include <QtCore/QObject>
#include <QtCore/QThread>

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


class QtWatcher2 : protected QThread
{
	Q_OBJECT

public:
	QtWatcher2(const QString& url, const QString& certfile, bool acceptall);
	~QtWatcher2();

	int setScope(const list<mc_sync_db>& syncs);
	int catchUpAndWatch(int timeout);

	void beginExcludingLocally(const QString& path);
	void endExcludingLocally(const QString& path);

protected slots:
	void checkquit();
	int localChange(const mc_sync_db& sync, const string& path);
	int localChangeTimeout();
	int remoteChange(const mc_sync_db& sync);

protected:
	virtual void run();

private:
	mc_sync_db* findMine(const mc_sync_db& sync);
	bool localChangesPending();

	QtLocalWatcher localWatcher;
	QtRemoteWatcher remoteWatcher;
	QTimer quittimer, timetimer, delaytimer;
	QEventLoop loop;
	list<mc_sync_db> syncs;
	QStringList excludedPaths;
	map<const mc_sync_db, QStringList> changedPaths;
	bool watching;
};


#endif /* MC_WATCHMODE */
#endif /* MC_WATCH2_H */