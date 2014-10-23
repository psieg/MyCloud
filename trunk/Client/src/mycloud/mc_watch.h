#ifndef MC_WATCH_H
#define MC_WATCH_H
#include "mc.h"
#include "mc_db.h"
#include "mc_fs.h"
#include "mc_srv.h"
#include "mc_walk.h"
#include "mc_filter.h"
#include "mc_helpers.h"
#include <QtCore/QStringList>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QRegExp>

#ifdef MC_WATCHMODE
#define MC_MAXRDEPTH	3

//For internal use only
class QtWatcher : public QObject
{
	Q_OBJECT

private slots:
	void checkquit();
	void startLocalWatch();
	void __startLocalWatch();
	void startRemoteWatch();
	void stopLocalWatch();
	void stopRemoteWatch();
	void directoryChanged(const QString &path);
	void fileChanged(const QString &path);
	int changeTimeout();
	int remoteChange(int status);
signals:
	void _startLocalWatch();
public:
	QtWatcher(const QStringList &paths, list<mc_sync_db> *syncs, const QString& url, const QString& certfile, bool acceptall);
	~QtWatcher();
	void run(int timeout);
private:
	QFileSystemWatcher *watcher;
	QtNetworkPerformer *performer;
	QEventLoop loop;
	QTimer timer, evttimer, quittimer, restarttimer, watchlocaltimer;
	QStringList changepaths;
	int repeatcounter;
	mc_buf netibuf, netobuf;
	list<mc_sync_db> *watchsyncs;
	bool watchfs;

};

int enter_watchmode(int timeout);

#endif /* MC_WATCHMODE */

#endif /* MC_WATCH_H */