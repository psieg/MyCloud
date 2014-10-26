#include "qtUpdateChecker.h"

#define MC_UPDATECERT_DATA "-----BEGIN CERTIFICATE-----\n" \
"MIIFyzCCA7OgAwIBAgIJALReD3txP7e8MA0GCSqGSIb3DQEBBQUAMHwxCzAJBgNV\n"\
"BAYTAkRFMQ8wDQYDVQQIDAZCZXJsaW4xDzANBgNVBAcMBkJlcmxpbjEUMBIGA1UE\n"\
"CgwLcGFkaW1haWwuZGUxEzARBgNVBAMMCnBhZGltYWlsQ0ExIDAeBgkqhkiG9w0B\n"\
"CQEWEWFkbWluQHBhZGltYWlsLmRlMB4XDTEzMDkxMjA4NTkwMFoXDTIzMDkxMDA4\n"\
"NTkwMFowfDELMAkGA1UEBhMCREUxDzANBgNVBAgMBkJlcmxpbjEPMA0GA1UEBwwG\n"\
"QmVybGluMRQwEgYDVQQKDAtwYWRpbWFpbC5kZTETMBEGA1UEAwwKcGFkaW1haWxD\n"\
"QTEgMB4GCSqGSIb3DQEJARYRYWRtaW5AcGFkaW1haWwuZGUwggIiMA0GCSqGSIb3\n"\
"DQEBAQUAA4ICDwAwggIKAoICAQC0dHZeywdYhFxwmqHeZz0h3KnM09lTV0DIyo6c\n"\
"lZegnX76ij++406hATU9veExOPOc4jskLIvWJi4pBtC/JS91YFcSGuF/D4zT9zbC\n"\
"3pPWBWp9VAopshpDLchsqihlbK+91DsCKEYBnGLitNQDGdzPI5SHc4uqQ9pgV3na\n"\
"1/aFpsoZB85V5QW5BWGyeSGF+dQKDUU54D7V+bmuYmn/YClmslNpJb7nmemYs2Wa\n"\
"YAIAdSMzu3E5Zu/w3wniMjJtovEhcMXeyEBfwuQX4YEOCWVSTDsgIG7sLX5e9M3S\n"\
"1UD/4yQ+79F7gonS8rprEd6O92Lc0keZy97qzcxDmEl90cvHyoZCJvfUoZbU/L4J\n"\
"8Y5v7is4jS0DPwVrZ70ADLhY2NFoKHtx8hMPhOvRxcP2knyQGXvtJY52PE0XViGT\n"\
"Wmp8tMMURG0YMEAk4g+gRllWRVZji+lxPEQo1KnzW67X53KphRheTfKFewan6R1S\n"\
"1e6QVHSO6c1m5JdBPYK4lprFdq75cr4iZQTOy+y75Za21XulAeJGbpqHBl/McqcX\n"\
"DwcjbYGjw3APj6COFB2k0GVyi0SmFUSstN3jVTOXMsSX4HQ7pxNkpjy7kT4ZWcln\n"\
"pHHK18Ti6JxEBGfuRYxegqOGhunmB9o6zAu2871whAWPS35h0KONaPa+aoky37LV\n"\
"uNN0KQIDAQABo1AwTjAdBgNVHQ4EFgQUiPCq+C9QWUh4yo97v04oCmy0WHMwHwYD\n"\
"VR0jBBgwFoAUiPCq+C9QWUh4yo97v04oCmy0WHMwDAYDVR0TBAUwAwEB/zANBgkq\n"\
"hkiG9w0BAQUFAAOCAgEAVrTT3a3dC3Txto5NDlA7A0ujq+4ScjfDb8dHd9H01qKK\n"\
"OTOotcSUCrn9JiRjjzWP8uvRhq98EiVB0RU7LgzPbx/sK9atKsaeXxxHDE0BfiCH\n"\
"VtZTeWGizhz1qH147grvWwLGOGjG32VDs+/M8WJ4u80QPNWR3HPh51egdX5QcnDn\n"\
"t+kjskXiNCmb+M/BFqlWirkYkTiVdqStiNS3+wKggIjjeMF7HNwSe9H6E/ZP83L8\n"\
"Od2rzMsUB5Ki7wEfaUhl+X2PjybiRT+PZDCBXugwy8ijxrwb/HeBlow/N+aXIqAV\n"\
"Amwgm3tP7fUxFe7umNdmODJBShRDmWEKm0nBmdPdqxZXHwE3uX+o8mkRkD3hUnW1\n"\
"7RO8huiiXyqhlJcGr9MxmwnM/zTskdtS/XJJmzMjWMG/gJf+qzoMVTVgJNJI8mOR\n"\
"urecVOcGURF/NvjozaKy8I8toGJGMsSmLzEDxMX+USMui0LDh6cRCZl3jy7bCkLg\n"\
"hpoIgSucXP1py10gl2Mr7RT2vZV02/mR4POj9k8C7n9ce2KjQR3tLu4JEnAqWcca\n"\
"D8BUMKgAAkEyS1r033YeZDAY2YrF/FP1UKF8muR1Z0kAfCSv1ZqknahT/zhjDmRX\n"\
"+kWh57rrWkx0BsZ4sJyj39iPM94Q5N706KDZSaEvY2I5QX9HCBjZNjCAdOO9zIw=\n"\
"-----END CERTIFICATE-----"

QtUpdateChecker::QtUpdateChecker() {
	rep = NULL;
	rerunTimer.setSingleShot(true);
	rerunTimer.setInterval(1000*MC_UPDATEFREQ);
	connect(&rerunTimer, SIGNAL(timeout()), this, SLOT(checkForUpdate()));
}
QtUpdateChecker::~QtUpdateChecker() {
	if (rep) delete rep;
}
int QtUpdateChecker::checkForUpdate() {
	int rc;
	mc_status s;
	rc = db_select_status(&s);
	MC_CHKERR(rc);
	if (s.updatecheck >= 0) {
		if (time(NULL)-s.updatecheck >= MC_UPDATEFREQ) {
			MC_INF("Checking for updates");
			sslconfig.setProtocol(QSsl::TlsV1SslV3);
			sslconfig.setCaCertificates(QSslCertificate::fromData(MC_UPDATECERT_DATA));
			req.setSslConfiguration(sslconfig);
			req.setUrl(QUrl(MC_UPDATECHECKURL));
			rep = manager.get(req);
			connect(rep, SIGNAL(finished()), this, SLOT(checkResult()));
		} else {
			if (s.updateversion != "") {
				if (s.updateversion == MC_VERSION) {
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

int QtUpdateChecker::checkResult() {
	QString newver;
	QStringList splitnew, splitme;
	int rc;
	mc_status s;
	bool newer = false;
	if (rep->error()) MC_ERR_MSG(MC_ERR_NETWORK, "Error during updatecheck: " << qPrintable(rep->errorString()));
	disconnect(rep, SIGNAL(finished()), this, SLOT(checkResult()));
	if (rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) MC_ERR_MSG(MC_ERR_NETWORK, "Update Server did not return 200 OK: " << rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
	newver = rep->readAll();
	newver = newver.mid(0, newver.length()-1);
	splitnew = newver.split(".");
	splitme = QString(MC_VERSION_PLAIN).split(".");
	for (int i = 0; i < 3; i++) if (splitnew.at(i).toInt() > splitme.at(i).toInt()) { newer = true; break; } else if (splitnew.at(i).toInt() < splitme.at(i).toInt()) { break; }
	if (newer) {
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

int QtUpdateChecker::getUpdate() {
	QDesktopServices::openUrl(QUrl(MC_UPDATEGETURL));
	return 0;
}