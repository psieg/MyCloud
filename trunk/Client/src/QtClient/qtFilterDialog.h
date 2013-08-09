#ifndef QTFilterDialog_H
#define QTFilterDialog_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <Qtwidgets/QFileDialog>
#include <QtGui/QShowEvent>
#include "ui_qtFilterDialog.h"
#include "mc_db.h"
#include "mc_srv.h"
#include "mc_types.h"

class qtFilterDialog : public QDialog
{
	Q_OBJECT

public:
	qtFilterDialog(QWidget *parent, QtNetworkPerformer *parentperf, int editID = -1);
	~qtFilterDialog();

public slots:
	void accept();
	
private slots:
	void on_browseButton_clicked();

protected:
	void showEvent(QShowEvent *event);

private:
	Ui::qtFilterDialog ui;
	QWidget *myparent;
	QtNetworkPerformer *performer;
	mc_buf netibuf,netobuf;
	int64 authtime;
	mc_filter filter;

};

#endif // QTFilterDialog_H
