#ifndef QTSYNCDIALOG_H
#define QTSYNCDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <Qtwidgets/QFileDialog>
#include <QtGui/QShowEvent>
#include "ui_qtSyncDialog.h"
#include "mc_db.h"
#include "mc_srv.h"

class qtSyncDialog : public QDialog
{
	Q_OBJECT

public:
	qtSyncDialog(QWidget *parent = 0, int editID = -1);
	~qtSyncDialog();

public slots:
	void accept();
	void reject();

private slots:
	void on_browseButton_clicked();
	void on_nameBox_currentIndexChanged(int);
	void authed(int);
	void listReceived(int);

protected:
	void showEvent(QShowEvent *event);

private:
	void filldbdata();

	Ui::qtSyncDialog ui;
	QWidget *myparent;
	int syncID;
	std::vector<mc_sync> srvsynclist;
	std::vector<mc_sync_db> dbsynclist;
	QtNetworkPerformer *performer;
	mc_buf netibuf,netobuf;
	int64 authtime;
	QIcon icon,lock;
	int dbindex;
	bool loadcompleted;
};

#endif // QTSYNCDIALOG_H