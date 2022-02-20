#include "qtUpdateChecker.h"

//padimailCA, startSSL CA, IdenTrust Commercial Root CA 1, ISRG Root X1
#define MC_UPDATECERT_DATA "-----BEGIN CERTIFICATE-----\n" \
"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\n"\
"-----END CERTIFICATE-----\n"\
"-----BEGIN CERTIFICATE-----\n"\
"MIIHhzCCBW+gAwIBAgIBLTANBgkqhkiG9w0BAQsFADB9MQswCQYDVQQGEwJJTDEW\n"\
"MBQGA1UEChMNU3RhcnRDb20gTHRkLjErMCkGA1UECxMiU2VjdXJlIERpZ2l0YWwg\n"\
"Q2VydGlmaWNhdGUgU2lnbmluZzEpMCcGA1UEAxMgU3RhcnRDb20gQ2VydGlmaWNh\n"\
"dGlvbiBBdXRob3JpdHkwHhcNMDYwOTE3MTk0NjM3WhcNMzYwOTE3MTk0NjM2WjB9\n"\
"MQswCQYDVQQGEwJJTDEWMBQGA1UEChMNU3RhcnRDb20gTHRkLjErMCkGA1UECxMi\n"\
"U2VjdXJlIERpZ2l0YWwgQ2VydGlmaWNhdGUgU2lnbmluZzEpMCcGA1UEAxMgU3Rh\n"\
"cnRDb20gQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUA\n"\
"A4ICDwAwggIKAoICAQDBiNsJvGxGfHiflXu1M5DycmLWwTYgIiRezul38kMKogZk\n"\
"pMyONvg45iPwbm2xPN1yo4UcodM9tDMr0y+v/uqwQVlntsQGfQqedIXWeUyAN3rf\n"\
"OQVSWff0G0ZDpNKFhdLDcfN1YjS6LIp/Ho/u7TTQEceWzVI9ujPW3U3eCztKS5/C\n"\
"Ji/6tRYccjV3yjxd5srhJosaNnZcAdt0FCX+7bWgiA/deMotHweXMAEtcnn6RtYT\n"\
"Kqi5pquDSR3l8u/d5AGOGAqPY1MWhWKpDhk6zLVmpsJrdAfkK+F2PrRt2PZE4XNi\n"\
"HzvEvqBTViVsUQn3qqvKv3b9bZvzndu/PWa8DFaqr5hIlTpL36dYUNk4dalb6kMM\n"\
"Av+Z6+hsTXBbKWWc3apdzK8BMewM69KN6Oqce+Zu9ydmDBpI125C4z/eIT574Q1w\n"\
"+2OqqGwaVLRcJXrJosmLFqa7LH4XXgVNWG4SHQHuEhANxjJ/GP/89PrNbpHoNkm+\n"\
"Gkhpi8KWTRoSsmkXwQqQ1vp5Iki/untp+HDH+no32NgN0nZPV/+Qt+OR0t3vwmC3\n"\
"Zzrd/qqc8NSLf3Iizsafl7b4r4qgEKjZ+xjGtrVcUjyJthkqcwEKDwOzEmDyei+B\n"\
"26Nu/yYwl/WL3YlXtq09s68rxbd2AvCl1iuahhQqcvbjM4xdCUsT37uMdBNSSwID\n"\
"AQABo4ICEDCCAgwwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYD\n"\
"VR0OBBYEFE4L7xqkQFulF2mHMMo0aEPQQa7yMB8GA1UdIwQYMBaAFE4L7xqkQFul\n"\
"F2mHMMo0aEPQQa7yMIIBWgYDVR0gBIIBUTCCAU0wggFJBgsrBgEEAYG1NwEBATCC\n"\
"ATgwLgYIKwYBBQUHAgEWImh0dHA6Ly93d3cuc3RhcnRzc2wuY29tL3BvbGljeS5w\n"\
"ZGYwNAYIKwYBBQUHAgEWKGh0dHA6Ly93d3cuc3RhcnRzc2wuY29tL2ludGVybWVk\n"\
"aWF0ZS5wZGYwgc8GCCsGAQUFBwICMIHCMCcWIFN0YXJ0IENvbW1lcmNpYWwgKFN0\n"\
"YXJ0Q29tKSBMdGQuMAMCAQEagZZMaW1pdGVkIExpYWJpbGl0eSwgcmVhZCB0aGUg\n"\
"c2VjdGlvbiAqTGVnYWwgTGltaXRhdGlvbnMqIG9mIHRoZSBTdGFydENvbSBDZXJ0\n"\
"aWZpY2F0aW9uIEF1dGhvcml0eSBQb2xpY3kgYXZhaWxhYmxlIGF0IGh0dHA6Ly93\n"\
"d3cuc3RhcnRzc2wuY29tL3BvbGljeS5wZGYwEQYJYIZIAYb4QgEBBAQDAgAHMDgG\n"\
"CWCGSAGG+EIBDQQrFilTdGFydENvbSBGcmVlIFNTTCBDZXJ0aWZpY2F0aW9uIEF1\n"\
"dGhvcml0eTANBgkqhkiG9w0BAQsFAAOCAgEAjo/n3JR5fPGFf59Jb2vKXfuM/gTF\n"\
"wWLRfUKKvFO3lANmMD+x5wqnUCBVJX92ehQN6wQOQOY+2IirByeDqXWmN3PH/UvS\n"\
"Ta0XQMhGvjt/UfzDtgUx3M2FIk5xt/JxXrAaxrqTi3iSSoX4eA+D/i+tLPfkpLst\n"\
"0OcNOrg+zvZ49q5HJMqjNTbOx8aHmNrs++myziebiMMEofYLWWivydsQD032ZGNc\n"\
"pRJvkrKTlMeIFw6Ttn5ii5B/q06f/ON1FE8qMt9bDeD1e5MNq6HPh+GlBEXoPBKl\n"\
"CcWw0bdT82AUuoVpaiF8H3VhFyAXe2w7QSlc4axa0c2Mm+tgHRns9+Ww2vl5GKVF\n"\
"P0lDV9LdJNUso/2RjSe15esUBppMeyG7Oq0wBhjA2MFrLH9ZXF2RsXAiV+uKa0hK\n"\
"1Q8p7MZAwC+ITGgBF3f0JBlPvfrhsiAhS90a2Cl9qrjeVOwhVYBsHvUwyKMQ5bLm\n"\
"KhQxw4UtjJixhlpPiVktucf3HMiKf8CdBUrmQk9io20ppB+Fq9vlgcitKj1MXVuE\n"\
"JnHEhV5xJMqlG2zYYdMa4FTbzrqpMrUi9nNBCV24F10OD5mQ1kfabwo6YigUZ4LZ\n"\
"8dCAWZvLMdibD4x3TrVoivJs9iQOLWxwxXPR3hTQcY+203sC9uO41Alua551hDnm\n"\
"fyWl8kgAwKQB2j8=\n"\
"-----END CERTIFICATE-----\n"\
"-----BEGIN CERTIFICATE-----\n"\
"MIIDSjCCAjKgAwIBAgIQRK+wgNajJ7qJMDmGLvhAazANBgkqhkiG9w0BAQUFADA/\n"\
"MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n"\
"DkRTVCBSb290IENBIFgzMB4XDTAwMDkzMDIxMTIxOVoXDTIxMDkzMDE0MDExNVow\n"\
"PzEkMCIGA1UEChMbRGlnaXRhbCBTaWduYXR1cmUgVHJ1c3QgQ28uMRcwFQYDVQQD\n"\
"Ew5EU1QgUm9vdCBDQSBYMzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n"\
"AN+v6ZdQCINXtMxiZfaQguzH0yxrMMpb7NnDfcdAwRgUi+DoM3ZJKuM/IUmTrE4O\n"\
"rz5Iy2Xu/NMhD2XSKtkyj4zl93ewEnu1lcCJo6m67XMuegwGMoOifooUMM0RoOEq\n"\
"OLl5CjH9UL2AZd+3UWODyOKIYepLYYHsUmu5ouJLGiifSKOeDNoJjj4XLh7dIN9b\n"\
"xiqKqy69cK3FCxolkHRyxXtqqzTWMIn/5WgTe1QLyNau7Fqckh49ZLOMxt+/yUFw\n"\
"7BZy1SbsOFU5Q9D8/RhcQPGX69Wam40dutolucbY38EVAjqr2m7xPi71XAicPNaD\n"\
"aeQQmxkqtilX4+U9m5/wAl0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNV\n"\
"HQ8BAf8EBAMCAQYwHQYDVR0OBBYEFMSnsaR7LHH62+FLkHX/xBVghYkQMA0GCSqG\n"\
"SIb3DQEBBQUAA4IBAQCjGiybFwBcqR7uKGY3Or+Dxz9LwwmglSBd49lZRNI+DT69\n"\
"ikugdB/OEIKcdBodfpga3csTS7MgROSR6cz8faXbauX+5v3gTt23ADq1cEmv8uXr\n"\
"AvHRAosZy5Q6XkjEGB5YGV8eAlrwDPGxrancWYaLbumR9YbK+rlmM6pZW87ipxZz\n"\
"R8srzJmwN0jP41ZL9c8PDHIyh8bwRLtTcm1D9SZImlJnt1ir/md2cXjbDaJWFBM5\n"\
"JDGFoqgCWjBH4d1QB7wCCZAA62RjYJsWvIjJEubSfZGL+T0yjWW06XyxV3bqxbYo\n"\
"Ob8VZRzI9neWagqNdwvYkQsEjgfbKbYK7p2CNTUQ                        \n"\
"-----END CERTIFICATE-----\n"\
"-----BEGIN CERTIFICATE-----\n"\
"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\n"\
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
			sslconfig.setProtocol(QSsl::SecureProtocols);
			sslconfig.setCaCertificates(QSslCertificate::fromData(MC_UPDATECERT_DATA));
			req.setSslConfiguration(sslconfig);
			req.setUrl(QUrl(MC_UPDATECHECKURL));
			rep = manager.get(req);
			connect(rep, SIGNAL(finished()), this, SLOT(checkResult()));
		} else {
			if (s.updateversion != "") {
				if (s.updateversion == MC_VERSION_PLAIN) {
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