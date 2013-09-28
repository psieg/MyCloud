#ifndef QTFILTERDIALOG_H
#define QTFILTERDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtGui/QShowEvent>
#include "ui_qtFilterDialog.h"
#include "mc_db.h"
#include "mc_srv.h"
#include "mc_types.h"
#include "mc_crypt.h"

class QtFilterDialog : public QDialog
{
	Q_OBJECT

public:
	QtFilterDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf, int syncID, int editID = -1);
	~QtFilterDialog();

public slots:
	void accept();
	
private slots:
	void on_typeBox_currentIndexChanged(int);
	void on_browseButton_clicked();

	void replyReceived(int);

protected:
	void showEvent(QShowEvent *event);

private:
	int typeToIndex(MC_FILTERTYPE type);
	MC_FILTERTYPE indexToType(int index);


	Ui::QtFilterDialog ui;
	QWidget *myparent;
	QtNetworkPerformer *performer;
	mc_buf *netibuf,*netobuf;
	mc_filter filter;
	mc_sync_db sync;

};

#endif // QTFILTERDIALOG_H
