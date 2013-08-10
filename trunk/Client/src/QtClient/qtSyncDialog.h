#ifndef QTSYNCDIALOG_H
#define QTSYNCDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <Qtwidgets/QFileDialog>
#include <QtGui/QShowEvent>
#include "ui_qtSyncDialog.h"
#include "qtFilterDialog.h"
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
	void on_addButton_clicked();
	void on_removeButton_clicked();
	void on_editButton_clicked();
	void on_tableWidget_itemSelectionChanged();

	void authed(int);
	void syncListReceived(int);
	void filterListReceived(int);
	void deleteReceived(int);

protected:
	void showEvent(QShowEvent *event);

private:
	void filldbdata();
	void listFilters();
	void listFilters_actual();

	Ui::qtSyncDialog ui;
	QWidget *myparent;
	int syncID;
	std::vector<mc_sync> srvsynclist;
	std::vector<mc_sync_db> dbsynclist;
	std::vector<mc_filter> filterlist;
	QtNetworkPerformer *performer;
	mc_buf netibuf,netobuf;
	int64 authtime;
	QIcon icon,lock,file,directory;
	int dbindex;
	bool loadcompleted;
	int deletingfilterid;
};

#endif // QTSYNCDIALOG_H
