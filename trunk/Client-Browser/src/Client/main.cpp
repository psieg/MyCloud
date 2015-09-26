
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QTextCodec>
#include <QtCore/QCommandLineParser>
#include <QtCore/QStandardPaths>
#include "mc_db.h"
#include "mc_crypt.h"
#include "mc_workerthread.h"

#ifdef MC_OS_UNIX
#	include <signal.h>
#endif

#ifdef MC_OS_WIN
BOOL WINAPI WIN32_handleFunc(DWORD signal){
	if(signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT){
		if(QtWorkerThread::instance()){
			QtWorkerThread::instance()->quit();
			return true;
		}
	}
	return false;
}
#else
void POSIX_handleFunc(int signal){
	//if(signal == SIGINT){ // only registering sigint
 		if(QtWorkerThread::instance()) QtWorkerThread::instance()->quit();
	//}
}
#endif

int main(int argc, char *argv[])
{
	int rc;
	int d;

#	ifdef MC_OS_WIN
	SetConsoleCtrlHandler(WIN32_handleFunc, TRUE);
#	else
	signal(SIGINT, POSIX_handleFunc);
#	endif

	QCoreApplication a(argc, argv);
	a.setApplicationName("MyCloud");
	QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

	QCommandLineParser parser;
	QCommandLineOption pathOption("dir", "Specifies the working directory where instance-specific files should be", "dir");
	parser.addOption(pathOption);

	parser.process(a);

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
	mc_logfile.open("MyCloud.log",ios::app);
	mc_logfile << "####################################################################" << endl;
	mc_logfile << "MyCloud Client Version " << MC_VERSION << endl;
#endif
	QtWorkerThread w;
		
	db_open("state.db");

	//rc = a.exec();	
	//rc = runmc();
	w.start();

	QObject::connect(&w,SIGNAL(finished()),&a,SLOT(quit()));
	rc = a.exec();
	
	db_close();
#ifdef MC_LOGFILE
	mc_logfile.close();
#endif
	return rc;
}
