#include "qtUpdateChecker.h"



QtUpdateChecker::QtUpdateChecker(){
	rep = NULL;
	rerunTimer.setSingleShot(true);
	rerunTimer.setInterval(1000*MC_UPDATEFREQ);
	connect(&rerunTimer,SIGNAL(timeout()),this,SLOT(checkForUpdate()));
}
QtUpdateChecker::~QtUpdateChecker(){
	if(rep) delete rep;
}
int QtUpdateChecker::checkForUpdate(){
	int rc;
	mc_status s;
	rc = db_select_status(&s);
	MC_CHKERR(rc);
	if(s.updatecheck >= 0){
		if(time(NULL)-s.updatecheck >= MC_UPDATEFREQ){
			MC_INF("Checking for updates");
			sslconfig.setProtocol(QSsl::TlsV1SslV3);
			sslconfig.setCaCertificates(QSslCertificate::fromPath(MC_UPDATECERT));
			req.setSslConfiguration(sslconfig);
			req.setUrl(QUrl(MC_UPDATECHECKURL));
			rep = manager.get(req);
			connect(rep,SIGNAL(finished()),this,SLOT(checkResult()));
		} else {
			if(s.updateversion != ""){
				if(s.updateversion == MC_VERSION){
					s.updateversion = "";
					rc = db_update_status(&s);
				} else {
					emit newVersion(QString(s.updateversion.c_str()));
				}
			}
			rerunTimer.start();
		}

	}
	return 0;
}

int QtUpdateChecker::checkResult(){
	QString newver;
	QStringList splitnew,splitme;
	int rc;
	mc_status s;
	bool newer = false;
	if(rep->error()) MC_ERR_MSG(MC_ERR_NETWORK,"Error during updatecheck: " << qPrintable(rep->errorString()));
	disconnect(rep,SIGNAL(finished()),this,SLOT(checkResult()));
	if(rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) MC_ERR_MSG(MC_ERR_NETWORK,"Update Server did not return 200 OK: " << rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
	newver = rep->readAll();
	newver = newver.mid(0,newver.length()-1);
	splitnew = newver.split(".");
	splitme = QString(MC_VERSION_PLAIN).split(".");
	for(int i = 0; i < 3; i++) if(splitnew.at(i).toInt() > splitme.at(i).toInt()) { newer = true; break; } else if(splitnew.at(i).toInt() < splitme.at(i).toInt()){ break; }
	if(newer){
		MC_INF("New version " << qPrintable(newver) << " available");
		rc = db_select_status(&s);
		MC_CHKERR(rc);
		s.updatecheck = time(NULL);
		s.updateversion = qPrintable(newver);
		rc = db_update_status(&s);
		MC_CHKERR(rc);
		emit newVersion(newver);
	} else {
		MC_INF("No updates available");
		rc = db_select_status(&s);
		MC_CHKERR(rc);
		s.updatecheck = time(NULL);
		rc = db_update_status(&s);
		MC_CHKERR(rc);
	}
	rerunTimer.start();	
	return 0;
}

int QtUpdateChecker::getUpdate(){
	QDesktopServices::openUrl(QUrl(MC_UPDATEGETURL));
	return 0;
}