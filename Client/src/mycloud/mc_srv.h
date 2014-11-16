#ifndef MC_SRV_H
#define MC_SRV_H
#include "mc.h"
#include <QtCore/QMutex>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslConfiguration>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#define NetworkReplyHardTimeoutError	98 // see QNetworkReply::TimeoutError

// Functions communicating with the server
bool srv_isopen();
int srv_open(const string& url, const string& certfile, const string& user, const string& passwd, int64 *basedate, int *uid, bool acceptall = false);
int srv_standby();
int srv_close();
int srv_auth(const string& user, const string& passwd, int64 *basedate = NULL, int *uid = NULL);
int srv_timecheck();
int srv_listsyncs(list<mc_sync> *l);
int srv_listfilters(list<mc_filter> *l, int sid);
int srv_listshares(list<mc_share> *l, int sid);
int srv_listdir(list<mc_file> *l, int parent);
int srv_getfile(int id, int64 offset, int64 blocksize, FILE *fdesc, int64 *byteswritten, unsigned char hash[16], bool withprogress = true);
int srv_getfile(int id, int64 offset, int64 blocksize, char *buf, int64 *byteswritten, unsigned char hash[16], bool withprogress = true); //requires buf to be blocksize big
int srv_getoffset(int id, int64 *offset);
int srv_putfile(mc_file *file, int64 blocksize, FILE *fdesc);
int srv_putfile(mc_file *file, int64 blocksize, char *buf);
int srv_addfile(int id, int64 offset, int64 blocksize, FILE *fdesc, unsigned char hash[16]);
int srv_addfile(int id, int64 offset, int64 blocksize, char *buf, unsigned char hash[16]);
int srv_patchfile(mc_file *file);
int srv_delfile(mc_file *file);
int srv_getmeta(int id, mc_file *file);
int srv_purgefile(int id);
int srv_notifychange(const list<mc_sync_db>& l, int *id);
int srv_idusers(const list<int>& ids, list<mc_user> *l);

//For internal or async use only
class QtNetworkPerformer : public QObject
{
	Q_OBJECT

public slots:
	void abort();
public:
	QtNetworkPerformer(const QString& url, const QString& certfile, bool acceptall, bool async = false, int timeout = MC_NETTIMEOUT);
	~QtNetworkPerformer();
	int perform(mc_buf *inbuf, mc_buf *outbuf, bool withprogress);
signals:
	void finished(int status);
private slots:
	void checkquit();
	void requestFinished(QNetworkReply *r);
	void requestTimedout();
	void uploadProgress(qint64 sent, qint64 total);
	void downloadProgress(qint64 received, qint64 total);
private:
	int processReply();
	QNetworkAccessManager manager;
	QNetworkRequest req;
	QNetworkReply *rep;
	QSslConfiguration config;
	QEventLoop loop;
	QTimer timeouttimer, quittimer;
	mc_buf *outbuf;
	bool timedout;
	bool async;

};


//for async use with an external async performer + event loop
int srv_auth_async(mc_buf *ibuf, mc_buf* obuf, QtNetworkPerformer *perf, 
				   const string& user, const string& passwd, int64 *ltimea);
int srv_auth_process(mc_buf *obuf, int64 *ltimea, int64 *basedate = NULL, int *uid = NULL);
int srv_listsyncs_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf);
int srv_listsyncs_process(mc_buf *obuf, list<mc_sync> *l);
int srv_createsync_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, const string& name, bool crypted);
int srv_createsync_process(mc_buf *obuf, int *id);
int srv_delsync_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, int id);
int srv_delsync_process(mc_buf *obuf);
int srv_listfilters_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, int sid);
int srv_listfilters_process(mc_buf *obuf, list<mc_filter> *l);
int srv_putfilter_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, mc_filter *filter);
int srv_putfilter_process(mc_buf *obuf, int *id);
int srv_delfilter_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, mc_filter *filter);
int srv_delfilter_process(mc_buf *obuf);
int srv_listshares_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, int sid);
int srv_listshares_process(mc_buf *obuf, list<mc_share> *l);
int srv_putshare_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, mc_share *share);
int srv_putshare_process(mc_buf *obuf);
int srv_delshare_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, mc_share *share);
int srv_delshare_process(mc_buf *obuf);
int srv_listusers_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf);
int srv_listusers_process(mc_buf *obuf, list<mc_user> *l);
int srv_idusers_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, const list<int>& l);
int srv_idusers_process(mc_buf *obuf, list<mc_user> *l);
int srv_notifychange_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, const list<mc_sync_db>& l);
int srv_notifychange_process(mc_buf *obuf, int *id);
int srv_passchange_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, const string& newpass);
int srv_passchange_process(mc_buf *obuf);
int srv_getkeyring_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf);
int srv_getkeyring_process(mc_buf *obuf, string *data);
int srv_setkeyring_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, string *data);
int srv_setkeyring_process(mc_buf *obuf);
#endif /* MC_SRV_H */