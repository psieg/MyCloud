#include "qtClient.h"
#include <QtWidgets/QApplication>
#include <QtCore/QDir>
#include <QtCore/QTextCodec>
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
	QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
	a.setQuitOnLastWindowClosed(false);
	if (argc>1 && strcmp(argv[1], "-startup") == 0) { 
		d = 15; 
		//On -startup set working directory to own, as Windows starts us at System32
		QDir::setCurrent(QApplication::applicationDirPath());
	} else d = 0;

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
