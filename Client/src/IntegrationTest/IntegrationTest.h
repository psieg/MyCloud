#include <QtTest/QtTest>
#include "mc_workerthread.h"

class IntegrationTest : public QObject
{
	Q_OBJECT

	QString uniqueName;
	QDir testDir;
	std::string testHost;
	int syncID = -1;
	int userID = -1;
	QtWorkerThread thread; // needed for MC_CHECKTERMINATE

private:
	int setupDB();
	int openConnection(int* uid);
	int setupClient(std::string path);

private slots:

	void initTestCase();
	void cleanupTestCase();

	void init();
	void cleanup();

	void basicUpDownload();
};