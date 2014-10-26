#ifndef MC_WATCH2_H
#define MC_WATCH2_H
#include <mc.h>
#ifdef MC_WATCHMODE
#include <QtCore/QObject.h>
#include <QtCore/QThread.h>

#ifdef MC_OS_WIN
class CReadDirectoryChanges;
#endif

class QtFileSystemWatcher : protected QThread
{
	Q_OBJECT

public:
	QtFileSystemWatcher();
	~QtFileSystemWatcher();

	inline const QObject* toQObject() const { return this; }

	void setScope(list<mc_sync_db>& syncs);

signals:
	void pathChanged(const string& path);
	void pathChanged(const QString& path);

protected:
	virtual void run();

#ifdef MC_OS_WIN
	CReadDirectoryChanges* changes;
	void* changesWaitHandle;
	void* waitForNewHandleEvent;
	void* gotNewHandleEvent;
	void* shouldQuitEvent;
	void* hasQuitEvent;
#endif
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