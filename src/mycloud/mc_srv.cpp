#include "mc_srv.h"
//#include <curl/curl.h>
#include "mc_util.h"
#include "mc_protocol.h"
#include "mc_crypt.h" //seed
#ifdef MC_QTCLIENT
#	include "qtClient.h"
#else
#	include "mc_workerthread.h"
#endif

QMutex srv_mutex;
QtNetworkPerformer *perf = NULL;
mc_buf ibuf,obuf;
unsigned char authtoken[16];
string reauth_user, reauth_pass;

// the internal versions ignore the srv_mutex, only use when the srv_mutex is already held by you
bool _srv_isopen();
int _srv_open(const string& url, const string& certfile, const string& user, const string& passwd, int64 *basedate, bool acceptall = false);
int _srv_standby();
int _srv_close();
int _srv_auth(const string& user, const string& passwd, int64 *basedate);
int _srv_reauth();
int _srv_timecheck();
int _srv_listsyncs(list<mc_sync> *l);
int _srv_listfilters(list<mc_filter> *l, int sid);
int _srv_listdir(list<mc_file> *l, int parent);
int _srv_getfile(int id, int64 offset, int64 blocksize, FILE *fdesc, int64 *byteswritten, unsigned char hash[16], bool withprogress = true);
int _srv_getfile(int id, int64 offset, int64 blocksize, char *buf, int64 *byteswritten, unsigned char hash[16], bool withprogress = true); 
int _srv_getoffset(int id, int64 *offset);
int _srv_getpreview(int id, unsigned char buf[32]);
int _srv_putfile(mc_file *file, int64 blocksize, FILE *fdesc);
int _srv_putfile(mc_file *file, int64 blocksize, char *buf);
int _srv_addfile(int id, int64 offset, int64 blocksize, FILE *fdesc, unsigned char hash[16]);
int _srv_addfile(int id, int64 offset, int64 blocksize, char *buf, unsigned char hash[16]);
int _srv_patchfile(mc_file *file);
int _srv_delfile(mc_file *file);
int _srv_getmeta(int id, mc_file *file);
int _srv_purgefile(int id);
int _srv_notifychange(list<mc_sync_db> *l, int *id);

#define SAFEFUNC(name,param,call)	int name(param){	\
	int rc;												\
	srv_mutex.lock();									\
	MC_NOTIFYIOSTART(MC_NT_SRV);						\
	rc = _##name(call);									\
	MC_NOTIFYIOEND(MC_NT_SRV);							\
	srv_mutex.unlock();									\
	return rc;											\
}
#define SAFEFUNC2(name,param1,param2,call1,call2)	int name(param1,param2){	\
	int rc;												\
	srv_mutex.lock();									\
	MC_NOTIFYIOSTART(MC_NT_SRV);						\
	rc = _##name(call1,call2);							\
	MC_NOTIFYIOEND(MC_NT_SRV);							\
	srv_mutex.unlock();									\
	return rc;											\
}
#define SAFEFUNC3(name,p1,p2,p3,c1,c2,c3)	int name(p1,p2,p3){	\
	int rc;												\
	srv_mutex.lock();									\
	MC_NOTIFYIOSTART(MC_NT_SRV);						\
	rc = _##name(c1,c2,c3);								\
	MC_NOTIFYIOEND(MC_NT_SRV);							\
	srv_mutex.unlock();									\
	return rc;											\
}
#define SAFEFUNC4(name,p1,p2,p3,p4,c1,c2,c3,c4)	int name(p1,p2,p3,p4){	\
	int rc;												\
	srv_mutex.lock();									\
	MC_NOTIFYIOSTART(MC_NT_SRV);						\
	rc = _##name(c1,c2,c3,c4);							\
	MC_NOTIFYIOEND(MC_NT_SRV);							\
	srv_mutex.unlock();									\
	return rc;											\
}
#define SAFEFUNC5(name,p1,p2,p3,p4,p5,c1,c2,c3,c4,c5)	int name(p1,p2,p3,p4,p5){	\
	int rc;												\
	srv_mutex.lock();									\
	MC_NOTIFYIOSTART(MC_NT_SRV);						\
	rc = _##name(c1,c2,c3,c4,c5);						\
	MC_NOTIFYIOEND(MC_NT_SRV);							\
	srv_mutex.unlock();									\
	return rc;											\
}
#define SAFEFUNC6(name,p1,p2,p3,p4,p5,p6,c1,c2,c3,c4,c5,c6)	int name(p1,p2,p3,p4,p5,p6){	\
	int rc;												\
	srv_mutex.lock();									\
	MC_NOTIFYIOSTART(MC_NT_SRV);						\
	rc = _##name(c1,c2,c3,c4,c5,c6);					\
	MC_NOTIFYIOEND(MC_NT_SRV);							\
	srv_mutex.unlock();									\
	return rc;											\
}
#define SAFEFUNC7(name,p1,p2,p3,p4,p5,p6,p7,c1,c2,c3,c4,c5,c6,c7)	int name(p1,p2,p3,p4,p5,p6,p7){	\
	int rc;												\
	srv_mutex.lock();									\
	MC_NOTIFYIOSTART(MC_NT_SRV);						\
	rc = _##name(c1,c2,c3,c4,c5,c6,c7);					\
	MC_NOTIFYIOEND(MC_NT_SRV);							\
	srv_mutex.unlock();									\
	return rc;											\
}
#define SAFEFUNC8(name,p1,p2,p3,p4,p5,p6,p7,p8,c1,c2,c3,c4,c5,c6,c7,c8)	int name(p1,p2,p3,p4,p5,p6,p7,p8){	\
	int rc;												\
	srv_mutex.lock();									\
	MC_NOTIFYIOSTART(MC_NT_SRV);						\
	rc = _##name(c1,c2,c3,c4,c5,c6,c7,c8);				\
	MC_NOTIFYIOEND(MC_NT_SRV);							\
	srv_mutex.unlock();									\
	return rc;											\
}

QtNetworkPerformer::QtNetworkPerformer(const QString& url, const QString& certfile, bool acceptall, bool async, int timeout){
	this->async = async;
	rep = NULL;
	timeouttimer.setSingleShot(true);
	timeouttimer.setInterval(timeout*1000);
	quittimer.setInterval(2000);
	connect(&quittimer,SIGNAL(timeout()),this,SLOT(checkquit()));
	config.setProtocol(QSsl::TlsV1SslV3);
	config.setCaCertificates(QSslCertificate::fromPath(certfile));
	if(acceptall) config.setPeerVerifyMode(QSslSocket::QueryPeer);
	else config.setPeerVerifyMode(QSslSocket::VerifyPeer);
	req.setUrl(url);
	req.setHeader(QNetworkRequest::UserAgentHeader, "MyCloud Client");
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
	req.setSslConfiguration(config);
	connect(&manager,SIGNAL(finished(QNetworkReply*)),this,SLOT(requestFinished(QNetworkReply*)));
}

QtNetworkPerformer::~QtNetworkPerformer(){
	if(rep) delete rep;
}

void QtNetworkPerformer::checkquit(){
	if(MC_TERMINATING()){
		loop.quit();
	}
}

void QtNetworkPerformer::requestFinished(QNetworkReply *r){
	timeouttimer.stop();
	quittimer.stop();
	
	if(async){
		emit finished(processReply());
	} else {
		loop.quit();
	}
}
void QtNetworkPerformer::requestTimedout(){
	timedout = true;
	if(rep && rep->isRunning()) rep->abort();
}
void QtNetworkPerformer::uploadProgress(qint64 sent, qint64 total){
	timeouttimer.start();
	crypt_seed(sent);
	MC_NOTIFYSUBPROGRESS(sent,sent);
}
void QtNetworkPerformer::downloadProgress(qint64 received, qint64 total){
	timeouttimer.start();
	crypt_seed(received);
	MC_NOTIFYSUBPROGRESS(received,received);
}

void QtNetworkPerformer::abort(){
	if(async){
		if(rep && rep->isRunning()){
			rep->abort();
		}
	} else MC_WRN("Ignoring abort request for async performer");
}

int QtNetworkPerformer::perform(mc_buf *inbuf, mc_buf *outbuf, bool withprogress){
	if(rep) MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"NetworkPerformer can only handle one query at a time");
	req.setHeader(QNetworkRequest::ContentLengthHeader, inbuf->used);
	rep = manager.post(req,QByteArray(inbuf->mem,inbuf->used));
	if(withprogress){
		connect(rep, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(downloadProgress(qint64,qint64)));
		connect(rep, SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(uploadProgress(qint64,qint64)));
	}
	connect(&timeouttimer, SIGNAL(timeout()), this, SLOT(requestTimedout()));
	this->outbuf = outbuf;
	timedout = false;
	timeouttimer.start();
	quittimer.start();
	if(async){
		// caller has to run a QEventLoop and catch finished(), we're done
	} else {
		loop.exec();
		MC_CHECKTERMINATING(); //From quittimer
		return processReply();
	}
	return 0;
}
int QtNetworkPerformer::processReply(){		
	if(rep->error()){
		if(rep->error() == QNetworkReply::OperationCanceledError){
			if(timedout){
				MC_WRN("Network failure: Timeout (Code " << QNetworkReply::TimeoutError << ")");
				if(rep) if(async) rep->deleteLater(); else delete rep;
				rep = NULL;
				return QNetworkReply::TimeoutError;
			} else {
				if(rep) if(async) rep->deleteLater(); else delete rep;
				rep = NULL;
				return QNetworkReply::OperationCanceledError; //User abort
			}
		} else {
			int rc = MC_ERR_NETWORK; //rep->error();
			MC_WRN("Network failure: " << qPrintable(rep->errorString()) << " (Code " << rep->error() << ")");
			if(rep) if(async) rep->deleteLater(); else delete rep;
			rep = NULL;
			return rc;
		}
	}
	if(rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
		int val = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if(rep) if(async) rep->deleteLater(); else delete rep;
		rep = NULL;
		MC_ERR_MSG(MC_ERR_SERVER,"Server did not send 200 OK: " << val);
	}
	QByteArray data = rep->readAll();
	MatchBuf(outbuf,data.size());
	outbuf->used = data.size();
	memcpy(outbuf->mem,data.constData(),data.size());
	if(rep) if(async) rep->deleteLater(); else delete rep;
	rep = NULL;
	return 0;
}

int srv_eval(int requiredcode, int altcode, mc_buf *outbuf){
	if(outbuf->used==0) MC_ERR_MSG(MC_ERR_PROTOCOL,"Empty server response");
	if (*((int*) outbuf->mem) == requiredcode)	return 0; //Server replied what we expected, handling can go on
	if(altcode != -1 && *((int*) outbuf->mem) == altcode) return MC_ERR_ALTCODE;
	//The server did not sent what we expected
	switch(*((int*) outbuf->mem)){
		case MC_SRVSTAT_BADQRY:
			MC_ERR_MSG(MC_ERR_BADQRY,"Server sent BADQRY");
		case MC_SRVSTAT_INTERROR:
			MatchBuf(outbuf,outbuf->used+1);
			outbuf->used++;
			outbuf->mem[outbuf->used-1] = '\0';
			MC_ERR_MSG(MC_ERR_SERVER,"Server Error: " << &outbuf->mem[sizeof(int)]);
		case MC_SRVSTAT_NOTSETUP:
			MC_ERR_MSG(MC_ERR_SERVER,"Server not setup");
		case MC_SRVSTAT_UNAUTH:
			MC_ERR_MSG(MC_ERR_LOGIN,"Unauthorized / Login failed");
		case MC_SRVSTAT_NOEXIST:
			MC_ERR_MSG(MC_ERR_SERVER,"Required object does not exist");
		case MC_SRVSTAT_EXISTS:
			MC_ERR_MSG(MC_ERR_SERVER,"The object exists already. ID is " << *((int*) &outbuf->mem[sizeof(int)]));
		case MC_SRVSTAT_NOTEMPTY:
			MC_ERR_MSG(MC_ERR_SERVER,"Object is not empty");
		case MC_SRVSTAT_VERIFYFAIL:
			MC_ERR_MSG(MC_ERR_VERIFY,"Verify Hash Mismatch");
		case MC_SRVSTAT_OFFSETFAIL:
			MC_ERR_MSG(MC_ERR_VERIFY,"Offset Mismatch");
		case MC_SRVSTAT_INCOMPATIBLE:
			MC_ERR_MSG(MC_ERR_PROTOCOL,"Protocol Mismatch, server uses newer protocol");
		default:
			MatchBuf(&obuf,outbuf->used+1);
			outbuf->used++;
			outbuf->mem[outbuf->used-1] = '\0';
			MC_ERR_MSG(MC_ERR_PROTOCOL,"Unrecongized server response: Code " << *((int*) outbuf->mem) << ": " << outbuf->mem << "-");
	}
}

int srv_perform(MC_SRVSTATUS requiredcode, bool withprogress = false, MC_SRVSTATUS altcode = -1, int rdepth = 0, mc_buf *inbuf = NULL, mc_buf *outbuf = NULL){
	//CURLcode rc;
	int rc;
	long httpstatus = 0;
	int64 dummy;
	if(inbuf == NULL) inbuf = &ibuf;
	if(outbuf == NULL) outbuf = &obuf;

	ClearBuf(outbuf);
	MC_NOTIFYIOSTART(MC_NT_SRV);
	rc = perf->perform(inbuf,outbuf,withprogress);
	MC_NOTIFYIOEND(MC_NT_SRV);
	
	switch(rc){
		case 0:
			break;
		case MC_ERR_TERMINATING:
			return MC_ERR_TERMINATING;
		case QNetworkReply::HostNotFoundError:
		case QNetworkReply::TimeoutError:
		case QNetworkReply::ConnectionRefusedError:
			if(rdepth < 3){
				MC_INF("retrying after 5 sec");
				mc_sleep(5);
				return srv_perform(requiredcode,withprogress,altcode,rdepth+1,inbuf,outbuf);
			} else MC_ERR_MSG(MC_ERR_NETWORK,"3 network failures in a row");
		case QNetworkReply::UnknownNetworkError:
			if(rdepth < 1){
				MC_INF("Network error, retrying after 5 sec");
				mc_sleep(5);
				return srv_perform(requiredcode,withprogress,altcode,rdepth+1,inbuf,outbuf);
			} else MC_ERR_MSG(MC_ERR_NETWORK,"Network error, see previous message");
		default:
			MC_ERR_MSG(MC_ERR_NETWORK,"Unknown network failure, see previous message");

	}
	return srv_eval(requiredcode,altcode,outbuf);
}

bool _srv_isopen(){
	return (perf != NULL);
}
bool srv_isopen(){
	bool rc;
	srv_mutex.lock();
	rc = _srv_isopen();
	srv_mutex.unlock();
	return rc;
}

SAFEFUNC6(srv_open,const string& url, const string& certfile, const string& user, const string& passwd, int64 *basedate, bool acceptall, \
		  url,certfile,user,passwd,basedate,acceptall)
int _srv_open(const string& url, const string& certfile, const string& user, const string& passwd, int64 *basedate, bool acceptall){
	//CURLcode crc;
	int rc;
	//long httpstatus = 0;
	//struct curl_slist *headers=NULL;
	MC_DBGL("Initializing server module");
	MC_DBG("Opening srv at " << url);
	string _url = "https://";
	_url.append(url);
	_url.append("/bin.php");
	SetBuf(&ibuf);
	SetBuf(&obuf);

	/* Set for all queries */
	/*curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();
	rc = curl_easy_setopt(curl,CURLOPT_USERAGENT,"MyCloud Client");
	MC_CHKERR_MSG(rc,"setopt failed");
	crc = curl_easy_setopt(curl,CURLOPT_CAINFO,certfile.c_str());
	MC_CHKERR_MSG(crc,"setopt failed");
	crc = curl_easy_setopt(curl,CURLOPT_ERRORBUFFER,&errbuf);
	MC_CHKERR_MSG(crc,"setopt failed");
	crc = curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteMemoryCallback);
	MC_CHKERR_MSG(crc,"setopt failed");
#ifdef MC_QTCLIENT
	crc = curl_easy_setopt(curl,CURLOPT_PROGRESSFUNCTION,WriteProgressCallback);
	MC_CHKERR_MSG(crc,"setopt failed");
	//crc = curl_easy_setopt(curl,CURLOPT_NOPROGRESS,1);
	//MC_CHKERR_MSG(crc,"setopt failed");
#endif
	//crc = curl_easy_setopt(curl,CURLOPT_WRITEDATA, (void*) &obuf);
	//MC_CHKERR_MSG(crc,"setopt failed");
	crc = curl_easy_setopt(curl,CURLOPT_URL,_url.c_str());
	MC_CHKERR_MSG(crc,"setopt failed");
	headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
	headers = curl_slist_append(headers, "Connection: keep-alive"); //Though not explicitly it seems to be on by default
	headers = curl_slist_append(headers, "Keep-Alive:300");
	crc = curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
	MC_CHKERR_MSG(crc,"setopt failed");
	crc = curl_easy_setopt(curl,CURLOPT_POST,1);
	MC_CHKERR_MSG(crc,"setopt failed");*/
	
	if(!QSslSocket::supportsSsl()) MC_ERR_MSG(MC_ERR_NETWORK,"No SSL Sockets supported");

	perf = new QtNetworkPerformer(_url.c_str(),certfile.c_str(),acceptall);

	//pack_code(&ibuf,MC_SRVQRY_STATUS);
	//rc = srv_perform(MC_SRVSTAT_OK);
	//MC_CHKERR(rc);
	//MC_DBGL("Server is ready");

	//Authentication and timecheck
	rc = _srv_auth(user,passwd,basedate);
	if(rc) { _srv_close(); return rc; }

	MC_INF("Server connection established");
	return 0;
}

SAFEFUNC(srv_standby,,)
int _srv_standby(){
	MC_DBGL("Srv module going to standby");
	ResetBuf(&ibuf);
	ResetBuf(&obuf);
	return 0;
}

SAFEFUNC(srv_close,,)
int _srv_close(){
	MC_DBGL("Finalizing server module");
	//curl_easy_cleanup(curl);
	//curl_global_cleanup();
	delete perf;
	FreeBuf(&ibuf);
	FreeBuf(&obuf);
	//curl = NULL;
	perf = NULL;
	return 0;
}

SAFEFUNC3(srv_auth,const string& user, const string& passwd, int64 *basedate, \
		  user,passwd,basedate)
int _srv_auth(const string& user, const string& passwd, int64 *basedate){
	MC_DBGL("Authenticating on server");
	int rc,version;
	int64 rtime,ltimea,ltimeb,diff;
	reauth_user = user;
	reauth_pass = passwd;
	pack_auth(&ibuf,user,passwd);
	ltimea = time(NULL);
	rc = srv_perform(MC_SRVSTAT_AUTHED,false);
	MC_CHKERR(rc);
	ltimeb = time(NULL);

	unpack_authed(&obuf,authtoken,&rtime,basedate,&version);
	MC_DBG("Authenticated, token is " << MD5BinToHex(authtoken));
	diff = abs((long)(ltimea-rtime));
	if(diff > MC_MAXTIMEDIFF+((ltimeb-ltimea)/2)) MC_ERR_MSG(MC_ERR_TIMEDIFF,"Timediff too high: " << diff);
	MC_DBG("Timing good, diff is " << diff);
	if(version < MC_MIN_SERVER_PROTOCOL_VERSION) MC_ERR_MSG(MC_ERR_PROTOCOL,"Protocol Mismatch, server uses too old protocol");

	return 0;
}
//No SAFEFUNC as we don't use srv resources (other than reading authtoken)
int srv_auth_async(mc_buf *ibuf, mc_buf* obuf, QtNetworkPerformer *perf,
				   const string& user, const string& passwd, int64 *ltimea){
	MC_DBGL("Authenticating on server (async)");
	srv_mutex.lock();
	reauth_user = user;
	reauth_pass = passwd;
	srv_mutex.unlock();
	pack_auth(ibuf,user,passwd);
	*ltimea = time(NULL);
	return perf->perform(ibuf,obuf,false);
}
int srv_auth_process(mc_buf *obuf, int64 *ltimea, int64 *basedate){
	int rc,version;
	int64 rtime,ltimeb,diff;
	rc = srv_eval(MC_SRVSTAT_AUTHED,-1,obuf);
	MC_CHKERR(rc);

	ltimeb = time(NULL);

	unpack_authed(obuf,authtoken,&rtime,basedate,&version);
	MC_DBG("Authenticated, token is " << MD5BinToHex(authtoken));
	diff = abs((long)(*ltimea-rtime));
	if(diff > MC_MAXTIMEDIFF+((ltimeb-*ltimea)/2)) MC_ERR_MSG(MC_ERR_TIMEDIFF,"Timediff too high: " << diff);
	MC_DBG("Timing good, diff is " << diff);
	if(version < MC_MIN_SERVER_PROTOCOL_VERSION) MC_ERR_MSG(MC_ERR_PROTOCOL,"Protocol Mismatch, server uses too old protocol");

	return 0;
}

int _srv_reauth(){
	MC_INF("Reauthenticating on server");
	int rc,version;
	int64 rtime,ltimea,ltimeb,diff,basedate;
	mc_buf tmpibuf,tmpobuf;
	SetBuf(&tmpibuf);
	SetBuf(&tmpobuf);
	pack_auth(&tmpibuf,reauth_user,reauth_pass);
	ltimea = time(NULL);
	rc = srv_perform(MC_SRVSTAT_AUTHED,false,-1,0,&tmpibuf,&tmpobuf);
	MC_CHKERR(rc);
	ltimeb = time(NULL);

	unpack_authed(&tmpobuf,authtoken,&rtime,&basedate,&version);
	FreeBuf(&tmpibuf);
	FreeBuf(&tmpobuf);
	MC_DBG("Authenticated, token is " << MD5BinToHex(authtoken));
	diff = abs((long)(ltimea-rtime));
	if(diff > MC_MAXTIMEDIFF+((ltimeb-ltimea)/2)) MC_ERR_MSG(MC_ERR_TIMEDIFF,"Timediff too high: " << diff);
	MC_DBG("Timing good, diff is " << diff);
	if(version < MC_MIN_SERVER_PROTOCOL_VERSION) MC_ERR_MSG(MC_ERR_PROTOCOL,"Protocol Mismatch, server uses too old protocol");

	return 0;
}

SAFEFUNC(srv_listsyncs,list<mc_sync> *l,l)
int _srv_listsyncs(list<mc_sync> *l){
	MC_DBGL("Fetching sync list");
	int rc;
	pack_listsyncs(&ibuf,authtoken);
	rc = srv_perform(MC_SRVSTAT_SYNCLIST);
	if(rc == MC_ERR_LOGIN) { rc = _srv_reauth(); MC_CHKERR(rc); 
		memcpy(&ibuf.mem[sizeof(int)],authtoken,16); rc = srv_perform(MC_SRVSTAT_SYNCLIST); }
	MC_CHKERR(rc);

	unpack_synclist(&obuf,l);
	return 0;
}

//No SAFEFUNC as we don't use srv resources (other than reading authtoken)
int srv_listsyncs_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf){
	MC_DBGL("Fetching sync list (async)");
	srv_mutex.lock();
	pack_listsyncs(ibuf,authtoken);
	srv_mutex.unlock();
	return perf->perform(ibuf,obuf,false);
}
int srv_listsyncs_process(mc_buf *obuf, list<mc_sync> *l){
	int rc;
	rc = srv_eval(MC_SRVSTAT_SYNCLIST,-1,obuf);
	MC_CHKERR(rc);

	unpack_synclist(obuf,l);
	return 0;
}

SAFEFUNC2(srv_listfilters,list<mc_filter> *l, int sid,l,sid)
int _srv_listfilters(list<mc_filter> *l, int sid){
	MC_DBGL("Fetching filter list for sid: " << sid);
	int rc;
	pack_listfilters(&ibuf,authtoken,sid);
	rc = srv_perform(MC_SRVSTAT_FILTERLIST);
	if(rc == MC_ERR_LOGIN) { rc = _srv_reauth(); MC_CHKERR(rc); 
		memcpy(&ibuf.mem[sizeof(int)],authtoken,16); rc = srv_perform(MC_SRVSTAT_FILTERLIST); }
	MC_CHKERR(rc);

	unpack_filterlist(&obuf,l);
	return 0;
}


int srv_listfilters_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, int sid){
	MC_DBGL("Fetching filter list for sid: " << sid << " (async)");
	srv_mutex.lock();
	pack_listfilters(ibuf,authtoken,sid);
	srv_mutex.unlock();

	return perf->perform(ibuf,obuf,false);
}
int srv_listfilters_process(mc_buf *obuf, list<mc_filter> *l){
	int rc;
	rc = srv_eval(MC_SRVSTAT_FILTERLIST,-1,obuf);
	MC_CHKERR(rc);

	unpack_filterlist(obuf,l);
	return 0;
}

//We don't need these synchronous
int srv_putfilter_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, mc_filter *filter){
	MC_DBGL("Putting filter " << filter->id << ": " << filter->rule << "(async)");
	srv_mutex.lock();
	pack_putfilter(ibuf,authtoken,filter);
	srv_mutex.unlock();

	return perf->perform(ibuf,obuf,false);
}
int srv_putfilter_process(mc_buf *obuf, int *id){
	int rc;
	rc = srv_eval(MC_SRVSTAT_FILTERID,-1,obuf);
	MC_CHKERR(rc);

	unpack_filterid(obuf,id);
	return 0;
}
int srv_delfilter_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, mc_filter *filter){
	MC_DBGL("Deleting filter " << filter->id << ": " << filter->rule << " (async)");
	srv_mutex.lock();
	pack_delfilter(ibuf,authtoken,filter->id);
	srv_mutex.unlock();

	return perf->perform(ibuf,obuf,false);
}
int srv_delfilter_process(mc_buf *obuf){
	return srv_eval(MC_SRVSTAT_OK,-1,obuf);
}

SAFEFUNC2(srv_listdir,list<mc_file> *l, int parent,l,parent)
int _srv_listdir(list<mc_file> *l, int parent){
	MC_DBGL("Fetching dir list for parent: " << parent);
	int rc;
	pack_listdir(&ibuf,authtoken,parent);
	rc = srv_perform(MC_SRVSTAT_DIRLIST);
	if(rc == MC_ERR_LOGIN) { rc = _srv_reauth(); MC_CHKERR(rc); 
		memcpy(&ibuf.mem[sizeof(int)],authtoken,16); rc = srv_perform(MC_SRVSTAT_DIRLIST); }
	MC_CHKERR(rc);

	unpack_dirlist(&obuf,l);
	return 0;
}

int _srv_getfile_actual(int id, int64 offset, int64 blocksize, unsigned char hash[16], int64 *write, bool withprogress){
	int rc;
	pack_getfile(&ibuf,authtoken,id,offset,blocksize,hash);
	rc = srv_perform(MC_SRVSTAT_FILE,withprogress);
	if(rc == MC_ERR_LOGIN) { rc = _srv_reauth(); MC_CHKERR(rc); 
		memcpy(&ibuf.mem[sizeof(int)],authtoken,16); rc = srv_perform(MC_SRVSTAT_FILE); }
	MC_CHKERR(rc);

	unpack_file(&obuf,write);
	if(obuf.used-*write < (int64)(sizeof(int)+sizeof(int64))) MC_ERR_MSG(MC_ERR_PROTOCOL,"Protocol Error: Server did not send enough data"); //Server did not send enough data
	if(obuf.used-*write > (int64)(sizeof(int)+sizeof(int64))) MC_ERR_MSG(MC_ERR_PROTOCOL,"Protocol Error: Server sent too much data"); //Server did not send enough data

	return 0;
}

SAFEFUNC7(srv_getfile,int id, int64 offset, int64 blocksize, FILE *fdesc, int64 *byteswritten, unsigned char hash[16], bool withprogress, \
		 id,offset,blocksize,fdesc,byteswritten,hash,withprogress)
int _srv_getfile(int id, int64 offset, int64 blocksize, FILE *fdesc, int64 *byteswritten, unsigned char hash[16], bool withprogress){
	MC_DBGL("Getting file " << id << " at offset " << offset);
	int rc;
	int64 write,written;
	int64 foffset = 0;

	rc = _srv_getfile_actual(id,offset,blocksize,hash,&write,withprogress);
	MC_CHKERR(rc);

	if(write){
		MC_NOTIFYIOSTART(MC_NT_FS);
		written = fwrite(obuf.mem+sizeof(int)+sizeof(int64)+foffset,write,1,fdesc);
		MC_NOTIFYIOEND(MC_NT_FS);
		if (written != 1) MC_ERR_MSG(MC_ERR_IO,"Writing to file failed: " << ferror(fdesc));
		if(byteswritten) *byteswritten = write;
	}
	return 0;
}

SAFEFUNC7(srv_getfile,int id, int64 offset, int64 blocksize, char *buf, int64 *byteswritten, unsigned char hash[16], bool withprogress, \
		 id,offset,blocksize,buf,byteswritten,hash,withprogress)
int _srv_getfile(int id, int64 offset, int64 blocksize, char *buf, int64 *byteswritten, unsigned char hash[16], bool withprogress){
	MC_DBGL("Getting file " << id << " at offset " << offset);
	int rc;
	int64 write;

	rc = _srv_getfile_actual(id,offset,blocksize,hash,&write,withprogress);
	MC_CHKERR(rc);
	
	if(write){
		memcpy(buf,obuf.mem+sizeof(int)+sizeof(int64),write);
		if(byteswritten) *byteswritten = write;
	}
	return 0;
}

SAFEFUNC2(srv_getoffset,int id,int64 *offset,id,offset)
int _srv_getoffset(int id, int64 *offset){
	MC_DBGL("Getting file offset of " << id);
	int rc;
	pack_getoffset(&ibuf,authtoken,id);
	rc = srv_perform(MC_SRVSTAT_OFFSET);
	if(rc == MC_ERR_LOGIN) { rc = _srv_reauth(); MC_CHKERR(rc); 
		memcpy(&ibuf.mem[sizeof(int)],authtoken,16); rc = srv_perform(MC_SRVSTAT_OFFSET); }
	MC_CHKERR(rc);

	unpack_offset(&obuf,offset);
	return 0;
}

//Caller must seek fdesc to beginning of file
SAFEFUNC3(srv_putfile,mc_file *file, int64 blocksize, FILE *fdesc, \
		 file,blocksize,fdesc)
int _srv_putfile(mc_file *file, int64 blocksize, FILE *fdesc){
	MC_DBGL("Putting file " << file->id << ": " << file->name << (file->is_dir?"/":""));
	int rc;
	size_t read;

	pack_putfile(&ibuf,authtoken,file,blocksize);
		
	MatchBuf(&ibuf,ibuf.used+blocksize);

	if(blocksize > 0){
		MC_NOTIFYIOSTART(MC_NT_FS);
		read = fread(ibuf.mem+ibuf.used,blocksize,1,fdesc);
		MC_NOTIFYIOEND(MC_NT_FS);
		if(read != 1) MC_ERR_MSG(MC_ERR_IO,"Reading from file failed: " << ferror(fdesc));
		ibuf.used += blocksize;
	}

	rc = srv_perform(MC_SRVSTAT_FILEID,true);
	if(rc == MC_ERR_LOGIN) { rc = _srv_reauth(); MC_CHKERR(rc); 
		memcpy(&ibuf.mem[sizeof(int)],authtoken,16); rc = srv_perform(MC_SRVSTAT_FILEID,true); }
	MC_CHKERR(rc);
	
	if(file->id == MC_FILEID_NONE){
		unpack_fileid(&obuf,&file->id);
	}
	return 0;
}

SAFEFUNC3(srv_putfile,mc_file *file, int64 blocksize, char *buf, \
		 file,blocksize,buf)
int _srv_putfile(mc_file *file, int64 blocksize, char *buf){
	MC_DBGL("Putting file " << file->id << ": " << file->name << (file->is_dir?"/":""));
	int rc;
	size_t read;
	pack_putfile(&ibuf,authtoken,file,blocksize);
		
	MatchBuf(&ibuf,ibuf.used+blocksize);


	if(blocksize > 0){
		memcpy(ibuf.mem+ibuf.used,buf,blocksize);
		ibuf.used += blocksize;
	}	

	rc = srv_perform(MC_SRVSTAT_FILEID,true);
	if(rc == MC_ERR_LOGIN) { rc = _srv_reauth(); MC_CHKERR(rc); 
		memcpy(&ibuf.mem[sizeof(int)],authtoken,16); rc = srv_perform(MC_SRVSTAT_FILEID,true); }
	MC_CHKERR(rc);
	
	if(file->id == MC_FILEID_NONE){
		unpack_fileid(&obuf,&file->id);
	}
	return 0;
}

//Caller must seek fdesc to offset
SAFEFUNC5(srv_addfile,int id, int64 offset, int64 blocksize, FILE *fdesc, unsigned char hash[16], \
		  id,offset,blocksize,fdesc,hash)
int _srv_addfile(int id, int64 offset, int64 blocksize, FILE *fdesc, unsigned char hash[16]){

	MC_DBGL("Adding file " << id << " at offset " << offset);
	int rc;
	size_t read;
	pack_addfile(&ibuf,authtoken,id,offset,blocksize,hash);

	MatchBuf(&ibuf,ibuf.used+blocksize);
	//fseek(fdesc,offset,SEEK_SET); //Normally we loop this function anyway
	if(blocksize != 0){
		MC_NOTIFYIOSTART(MC_NT_FS);
		read = fread(ibuf.mem+ibuf.used,blocksize,1,fdesc);
		MC_NOTIFYIOEND(MC_NT_FS);
		if(read != 1) MC_ERR_MSG(MC_ERR_IO,"Reading from file failed: " << ferror(fdesc));
		ibuf.used += blocksize;
	}

	rc = srv_perform(MC_SRVSTAT_OK,true);
	if(rc == MC_ERR_LOGIN) { rc = _srv_reauth(); MC_CHKERR(rc); 
		memcpy(&ibuf.mem[sizeof(int)],authtoken,16); rc = srv_perform(MC_SRVSTAT_OK,true); }
	MC_CHKERR(rc);
	return 0;
}

SAFEFUNC5(srv_addfile,int id, int64 offset, int64 blocksize, char *buf, unsigned char hash[16], \
		  id,offset,blocksize,buf,hash)
int _srv_addfile(int id, int64 offset, int64 blocksize, char *buf, unsigned char hash[16]){
	MC_DBGL("Adding file " << id << " at offset " << offset);
	int rc;
	size_t read;
	pack_addfile(&ibuf,authtoken,id,offset,blocksize,hash);
	MatchBuf(&ibuf,ibuf.used+blocksize);

	if(blocksize != 0){
		memcpy(ibuf.mem+ibuf.used,buf,blocksize);
		ibuf.used += blocksize;
	}

	rc = srv_perform(MC_SRVSTAT_OK,true);
	if(rc == MC_ERR_LOGIN) { rc = _srv_reauth(); MC_CHKERR(rc); 
		memcpy(&ibuf.mem[sizeof(int)],authtoken,16); rc = srv_perform(MC_SRVSTAT_OK,true); }
	MC_CHKERR(rc);
	return 0;
}

SAFEFUNC(srv_patchfile,mc_file *file,file)
int _srv_patchfile(mc_file *file){
	MC_DBGL("Patching file " << file->id << ": " << file->name << (file->is_dir?"/":""));
	int rc;
	pack_patchfile(&ibuf,authtoken,file);
	rc = srv_perform(MC_SRVSTAT_OK);
	if(rc == MC_ERR_LOGIN) { rc = _srv_reauth(); MC_CHKERR(rc); 
		memcpy(&ibuf.mem[sizeof(int)],authtoken,16); rc = srv_perform(MC_SRVSTAT_OK); }
	MC_CHKERR(rc);
	return 0;
}

SAFEFUNC(srv_delfile,mc_file *file,file)
int _srv_delfile(mc_file *file){
	MC_DBGL("Deleting file " << file->id << ": " << file->name << (file->is_dir?"/":""));
	int rc;
	pack_delfile(&ibuf,authtoken,file->id,file->mtime);
	rc = srv_perform(MC_SRVSTAT_OK);
	if(rc == MC_ERR_LOGIN) { rc = _srv_reauth(); MC_CHKERR(rc); 
		memcpy(&ibuf.mem[sizeof(int)],authtoken,16); rc = srv_perform(MC_SRVSTAT_OK); }
	MC_CHKERR(rc);
	return 0;
}

SAFEFUNC2(srv_getmeta,int id, mc_file *file, id,file)
int _srv_getmeta(int id, mc_file *file){
	MC_DBGL("Getting metadata for file: " << id);
	int rc;
	pack_getmeta(&ibuf,authtoken,id);
	rc = srv_perform(MC_SRVSTAT_FILEMETA);
	if(rc == MC_ERR_LOGIN) { rc = _srv_reauth(); MC_CHKERR(rc); 
		memcpy(&ibuf.mem[sizeof(int)],authtoken,16); rc = srv_perform(MC_SRVSTAT_FILEMETA); }
	MC_CHKERR(rc);

	unpack_filemeta(&obuf,file);
	return 0;
}

SAFEFUNC(srv_purgefile,int id,id)
int _srv_purgefile(int id){
	MC_DBGL("PURGING file " << id);
	int rc;
	pack_purgefile(&ibuf,authtoken,id);
	rc = srv_perform(MC_SRVSTAT_OK);
	if(rc == MC_ERR_LOGIN) { rc = _srv_reauth(); MC_CHKERR(rc); 
		memcpy(&ibuf.mem[sizeof(int)],authtoken,16); rc = srv_perform(MC_SRVSTAT_OK); }
	MC_CHKERR(rc);
	return 0;
}

SAFEFUNC2(srv_notifychange,list<mc_sync_db> *l, int *id,l,id)
int _srv_notifychange(list<mc_sync_db> *l, int *id){
	MC_DBGL("Watching srv changes");
	int rc;
	pack_notifychange(&ibuf,authtoken,l);
	rc = srv_perform(MC_SRVSTAT_CHANGE,false,MC_SRVSTAT_NOCHANGE);
	if(rc == MC_ERR_ALTCODE) return MC_ERR_NOCHANGE;
	MC_CHKERR(rc);
	unpack_change(&obuf,id);

	return 0;
}

//No SAFEFUNC as we don't use srv resources (other than reading authtoken)
int srv_notifychange_async(mc_buf *ibuf, mc_buf *obuf, QtNetworkPerformer *perf, list<mc_sync_db> *l){
	MC_DBGL("Watching srv changes (async)");
	srv_mutex.lock();
	pack_notifychange(ibuf,authtoken,l);
	srv_mutex.unlock();
	return perf->perform(ibuf,obuf,false);
}
int srv_notifychange_process(mc_buf *obuf, int *id){
	int rc;
	rc = srv_eval(MC_SRVSTAT_CHANGE,MC_SRVSTAT_NOCHANGE,obuf);
	if(rc == MC_ERR_ALTCODE) return MC_ERR_NOCHANGE;
	MC_CHKERR(rc);

	unpack_change(obuf,id);
	return 0;
}