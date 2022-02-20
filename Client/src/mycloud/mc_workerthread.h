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
	QtWorkerThread() {
		Q_ASSERT_X((QtWorkerThread::_instance == NULL), "QtWorkerThread", "There should only be one QtWorkerThread Instance");
		QtWorkerThread::_instance = this;
		terminating = false;
		runSingle = false;
	};
	~QtWorkerThread() {
		QtWorkerThread::_instance = NULL;
	};

	void quit() {
		if (this->isRunning()) {
			QEventLoop loop;
			QTimer timer;
			terminating = true;
			std::cout << "Waiting 10 sec for Worker Thread termination" << std::endl;
			timer.setSingleShot(true);
			connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
			connect(this, SIGNAL(finished()), &loop, SLOT(quit()));
			timer.start(10000);
			loop.exec();
			if (this->isRunning()) {
				std::cout << "Killing Worker Thread" << std::endl;
				this->terminate();
				db_execstr(std::string("UPDATE syncs SET status = ") + std::to_string(MC_SYNCSTAT_ABORTED) + " WHERE status = " + std::to_string(MC_SYNCSTAT_RUNNING));
				//srv_mutex.unlock();
				//srv_close();
				//db_mutex.unlock();
			} else {
				std::cout << "Worker Thread exited" << std::endl;
			}
			terminating = false;
		}
	};

	static QtWorkerThread* instance() { return _instance; }
	bool terminating;
	bool runSingle;


protected:
	static QtWorkerThread *_instance;

	virtual void run() {
		runmc();
	};
};

#endif /* QTWORKERTHREAD_H */