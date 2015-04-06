#include "IntegrationTest.h"
#include <ctime>
#include "mc_db.h"
#include "mc_srv.h"
using namespace std;

#define MCVERIFY(x) QVERIFY(!x)
#define Q_CHKERR(x) if (!x) return 1;

int IntegrationTest::setupDB() {
	mc_status s;
	s.acceptallcerts = true;
	s.basedate = 0;
	s.lastconn = 0;
	s.passwd = "integrationtest";
	s.uid = 10;
	s.uname = "test";
	s.updatecheck = -1;
	s.updateversion = "";
	s.url = "padimail.de/cloud";

	return db_update_status(&s);
}

int IntegrationTest::openConnection(int* uid) {
	return srv_open("padimail.de/cloud", "", "test", "integrationtest", nullptr, uid, true);
}

int IntegrationTest::setupClient(string path)
{
	int rc;
	Q_CHKERR(this->testDir.mkdir(path.c_str()));

	string syncpath = qPrintable(this->testDir.absolutePath());
	syncpath.append("/" + path + "/data/");
	Q_CHKERR(this->testDir.mkdir(syncpath.c_str()));

	MC_CHKERR(db_open(path + "/state.db"));

	MC_CHKERR(setupDB());

	mc_sync_db s;
	s.crypted = true;
	memcpy(s.cryptkey, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x00\x00\x00\x00\x00\x0f\x00\xba\x00\x00\x00\x00\x13\x37\x00\x42", 32);
	s.filterversion = 0;
	memcpy(s.cryptkey, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16);
	s.id = this->syncID;
	s.lastsync = 0;
	s.name = qPrintable(this->uniqueName);
	s.path = syncpath;
	s.priority = 0;

	MC_CHKERR(db_insert_sync(&s));

	MC_CHKERR(db_close());

	return 0;
}

void IntegrationTest::initTestCase() {
}
void IntegrationTest::cleanupTestCase() {
}

void IntegrationTest::init() {
	srand(rand() * time(nullptr) * clock());

	this->uniqueName = "mc_test_" + QString::number(time(nullptr)) + "_" + QString::number(rand());
	QVERIFY(QDir::temp().mkdir(this->uniqueName));
	this->testDir = QDir(QDir::tempPath() + QDir::separator() + this->uniqueName);
	QDir::setCurrent(this->testDir.absolutePath());
	MCVERIFY(openConnection(&this->userID));
	MCVERIFY(srv_createsync(qPrintable(this->uniqueName), true, &this->syncID));
	MCVERIFY(srv_close());

	// Create two clients A and B, each knowing the sync
	MCVERIFY(setupClient("A"));
	MCVERIFY(setupClient("B"));

}

void IntegrationTest::cleanup() {
	if (db_isopen()) {
		db_close();
	}

	if (this->syncID != -1) {
		if (!openConnection(&this->userID)) {
			srv_delsync(this->syncID);
			srv_close();
			this->syncID = -1;
		}
	}

	QDir::setCurrent(QDir::tempPath());
	this->testDir.removeRecursively();
}


void IntegrationTest::basicUpDownload()
{
	QFile f(QDir::tempPath() + "/" + this->uniqueName + "/A/data/test.txt");
	QVERIFY(f.open(QIODevice::OpenModeFlag::WriteOnly));
	f.write("asdfasdffdsa");
	f.close();

	// UPLOAD
	MCVERIFY(db_open("A/state.db"));

	thread.runSingle = true;
	MCVERIFY(runmc());

	MCVERIFY(db_close());

	// DOWNLOAD
	MCVERIFY(db_open("B/state.db"));

	thread.runSingle = true;
	MCVERIFY(runmc());

	MCVERIFY(db_close());


	QFile f2(QDir::tempPath() + "/" + this->uniqueName + "/B/data/test.txt");
	QVERIFY(f2.open(QIODevice::OpenModeFlag::ReadOnly));
	QVERIFY(QString(f2.readAll()) == "asdfasdffdsa");
}