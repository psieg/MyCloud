#ifndef QTWORKERTHREAD_H
#define QTWORKERTHREAD_H
#include <iostream>
#include <QtCore/QThread>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include "Client.h"
#include "mc_db.h"
#include "mc_srv.h"

class QtWorkerThread : public QThread
{
	Q_OBJECT

public:
	QtWorkerThread(){
		Q_ASSERT_X((QtWorkerThread::_instance == NULL), "QtWorkerThread", "There should only be one QtWorkerThread Instance");
		QtWorkerThread::_instance = this;
		terminating = false;
		//moveToThread(this); //?
	};
	~QtWorkerThread(){
		QtWorkerThread::_instance = NULL;
	};
	
	enum WorkerCommand {
		NormalRun = 0,
		DeleteSync = 1
	};

	void run(){
		switch(command){
			case NormalRun:
				runmc();
				break;
			case DeleteSync:
				break;
			default:
				;
		}
	};
	void quit(){
		if(this->isRunning()){
			QEventLoop loop;
			QTimer timer;
			terminating = true;
			cout << "Waiting 10 sec for Worker Thread termination" << endl;
			timer.setSingleShot(true);
			connect(&timer,SIGNAL(timeout()),&loop,SLOT(quit()));
			connect(this,SIGNAL(finished()),&loop,SLOT(quit()));
			timer.start(10000);
			loop.exec();
			if(this->isRunning()){
				cout << "Killing Worker Thread" << endl;
				this->terminate();
				db_execstr("UPDATE syncs SET status = " STRX(MC_SYNCSTAT_ABORTED) " WHERE status = " STRX(MC_SYNCSTAT_RUNNING));
				//srv_mutex.unlock();
				//srv_close();
				//db_mutex.unlock();
			} else {
				cout << "Worker Thread exited" << endl;
			}
			terminating = false;
		}
	};

	static QtWorkerThread* instance(){ return _instance; }
	static QtWorkerThread *_instance;
	bool terminating;
	int command;
	void* command_param;

};

#endif /* QTWORKERTHREAD_H */