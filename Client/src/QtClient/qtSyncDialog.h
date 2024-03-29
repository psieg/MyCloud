#ifndef QTSYNCDIALOG_H
#define QTSYNCDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtGui/QShowEvent>
#include "ui_qtSyncDialog.h"
#include "qtNewSyncDialog.h"
#include "qtFilterDialog.h"
#include "qtGeneralFilterDialog.h"
#include "qtShareDialog.h"
#include "mc_workerthread.h"
#include "mc_db.h"
#include "mc_srv.h"
#include "mc_filter.h"
#include "mc_crypt.h"

class QtSyncDialog : public QDialog
{
	Q_OBJECT

public:
	QtSyncDialog(QWidget *parent = 0, int editID = -1);
	~QtSyncDialog();

public slots:
	void accept();
	void reject();

private slots:
	void authed(int);
	void syncListReceived(int);
	void globalFilterListReceived(int);
	void filterListReceived(int);
	void shareListReceived(int);
	void userListReceived(int);
	void filterDeleteReceived(int);
	void shareDeleteReceived(int);
	void syncDeleteReceived(int);
	void userReceived(int);
	void keyringReceived_actual(int);
	void keyringReceivedLooking(int);
	void keyringReceivedAdding(int);
	void keyringSent(int);

	void on_deleteSyncButton_clicked();
	void on_browseButton_clicked();
	void on_nameBox_currentIndexChanged(int);
	void on_addFilterButton_clicked();
	void on_removeFilterButton_clicked();
	void on_editFilterButton_clicked();
	void on_addShareButton_clicked();
	void on_removeShareButton_clicked();
	void on_filterTable_itemSelectionChanged();
	void on_shareList_itemSelectionChanged();
	void on_globalFilterButton_clicked();

protected:
	void showEvent(QShowEvent *event);

private:
	void generateNewKey();
	void accept_step2();
	void accept_step3();
	void filldbdata();
	void listFilters();
	void listFilters_actual();
	void listShares();
	void listShares_actual();
	void startOver();
	void disableAll();

	Ui::QtSyncDialog ui;
	QWidget *myparent;
	int syncID;
	int myUID;
	std::vector<mc_sync> srvsynclist;
	std::vector<mc_sync_db> dbsynclist;
	std::vector<mc_filter> filterlist;
	std::vector<mc_share> sharelist;
	QtNetworkPerformer *performer;
	mc_buf netibuf,netobuf;
	int64 authtime;
	QIcon icon,lock,file,directory,add,user;
	int dbindex;
	bool loadcompleted;
	int deletingfilterid;
	mc_share deletingshare;
	mc_sync_db *worksync;
	mc_sync_db newsync;
	mc_user worksyncowner;
	std::list<mc_keyringentry> keyring;
	bool keyringloaded;
	QString keyringpass;
	bool keychanged;
};

#endif // QTSYNCDIALOG_H
