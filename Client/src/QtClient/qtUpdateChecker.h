#ifndef QTUPDATECHECKER_H
#define QTUPDATECHECKER_H

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslConfiguration>
#include <QtGui/QDesktopServices>
#include <QtCore/QTimer>

#include "mc_types.h"
#include "mc_db.h"

#define MC_UPDATEFREQ	86400
#define MC_UPDATECHECKURL	"https://padimail.de/mycloud/update/currentver/"
#define MC_UPDATEGETURL		"https://padimail.de/mycloud/update/getpatch/?oldver=" MC_VERSION
#define MC_UPDATECERT	"updateCA.crt"

class QtUpdateChecker : public QObject {
	Q_OBJECT
public:
	QtUpdateChecker();
	~QtUpdateChecker();
	
public slots:
	int checkForUpdate();
	int getUpdate();
private slots:
	int checkResult();

signals:
	void newVersion(QString newver);

private:
	QNetworkAccessManager manager;
	QNetworkRequest req;
	QNetworkReply *rep;
	QSslConfiguration sslconfig;
	QTimer rerunTimer;
};

#endif /* QTUPDATECHECKER_H */