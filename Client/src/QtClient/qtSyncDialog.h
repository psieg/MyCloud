#ifndef QTSYNCDIALOG_H
#define QTSYNCDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtGui/QShowEvent>
#include "ui_qtSyncDialog.h"
#include "qtFilterDialog.h"
#include "qtGeneralFilterDialog.h"
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
	void filterListReceived(int);
	void deleteReceived(int);

	void on_browseButton_clicked();
	void on_nameBox_currentIndexChanged(int);
	void on_addButton_clicked();
	void on_removeButton_clicked();
	void on_editButton_clicked();
	void on_filterTable_itemSelectionChanged();
	void on_globalButton_clicked();

protected:
	void showEvent(QShowEvent *event);

private:
	void filldbdata();
	void listFilters();
	void listFilters_actual();

	Ui::QtSyncDialog ui;
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
