#include <QtTest/QtTest>
#include "mc_workerthread.h"

class IntegrationTest : public QObject
{
	Q_OBJECT

	QString uniqueName;
	QDir testDir;
	int syncID = -1;
	int userID = -1;
	QtWorkerThread thread; // needed for MC_CHECKTERMINATE

private:
	int IntegrationTest::setupDB();
	int IntegrationTest::openConnection(int* uid);
	int IntegrationTest::setupClient(string path);

private slots:

	void initTestCase();
	void cleanupTestCase();

	void init();
	void cleanup();

	void basicUpDownload();
};