#include "qtClient.h"
#include <QtWidgets/QApplication>
#include <QtCore/QDir>
#include <QtCore/QTextCodec>
#include <QtCore/QCommandLineParser>
#include "qdebugstream.h"
#include "mc_db.h"
#include "mc_crypt.h"

bool printing = false;
void printfunc(QtMsgType t, const QMessageLogContext & c, const QString & s){
	if (printing)
	{
		cerr << qPrintable(s);
	}
	else
	{
		printing = true;
		Q_ASSERT_X(false, "Qt", qPrintable(s));
		printing = false;
	}
}

int main(int argc, char *argv[])
{
	int rc;
	int d;

	QApplication a(argc, argv);
	a.setApplicationName("MyCloud");
	QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
	a.setQuitOnLastWindowClosed(false);

	QCommandLineParser parser;
	parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
	QCommandLineOption startupOption("startup", "Indicates program is run from an automatic startup mechanism");
	parser.addOption(startupOption);
	QCommandLineOption pathOption("dir", "Specifies the working directory where instance-specific files should be", "dir");
	parser.addOption(pathOption);

	parser.process(a);

	if (parser.isSet(startupOption)) {
		d = 15; 
	} else d = 0;

	QString path;
	if (parser.isSet(pathOption)) {
		path = parser.value(pathOption);
	} else {
		path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	}
	QDir dir;
	dir.mkpath(path);
	QDir::setCurrent(path);

#ifdef MC_LOGFILE
	mc_logfile.open("MyCloud.log", ios::app);
	mc_logfile << "####################################################################" << endl;
	mc_logfile << "MyCloud QtClient Version " << MC_VERSION << endl;
#endif

	QtClient w(NULL, d);
	QDebugStream qout(std::cout, &w);
	QDebugStream qerr(std::cerr, &w);

	qInstallMessageHandler(printfunc);
	
	db_open("state.db");
	if (d == 0) {
		w.show();
		//w.listSyncs();
	}
	rc = a.exec();	
	db_close();
#ifdef MC_LOGFILE
	mc_logfile.close();
#endif
	return rc;
}
