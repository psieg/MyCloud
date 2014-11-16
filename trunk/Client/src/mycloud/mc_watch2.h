#ifndef MC_WATCH2_H
#define MC_WATCH2_H
#include <mc.h>
#ifdef MC_WATCHMODE
#include "mc_srv.h"
#include <QtCore/QObject>

#ifdef MC_OS_WIN

#include <QtCore/QThread>
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

	int startSingle();
	void stop();

signals:
	void syncChanged(const mc_sync_db& sync);

protected slots:
	void remoteChange(int status);

protected:
	list<mc_sync_db> syncs;
	QtNetworkPerformer* performer;
	mc_buf netibuf, netobuf;

};

/*
class QtWatcher2 : public QObject
{
	Q_OBJECT

public:
	QtWatcher2();
	~QtWatcher2();

	void clearAndSetScope(list<mc_sync>& syncs);
	int catchUpAndWatch(int timeout);

	void beginExcluding(const string& path);
	void endExcluding(const string& path);

protected slots:
	void pathChanged(const string& path);

private:
	QtFileSystemWatcher thread;
	list<string> changedPaths;
	list<string> excludedPaths;
};
*/

#endif /* MC_WATCHMODE */
#endif /* MC_WATCH2_H */