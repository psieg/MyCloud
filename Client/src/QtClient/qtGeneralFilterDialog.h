#ifndef qtGeneralFilterDialog_H
#define qtGeneralFilterDialog_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <Qtwidgets/QFileDialog>
#include <QtGui/QShowEvent>
#include "ui_qtGeneralFilterDialog.h"
#include "mc_db.h"
#include "mc_srv.h"
#include "mc_types.h"

class qtGeneralFilterDialog : public QDialog
{
	Q_OBJECT

public:
	qtGeneralFilterDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf);
	~qtGeneralFilterDialog();

public slots:
	void accept();
	
private slots:

protected:
	void showEvent(QShowEvent *event);

private:

	Ui::qtGeneralFilterDialog ui;
	QWidget *myparent;
	QtNetworkPerformer *performer;
	mc_buf *netibuf,*netobuf;
	std::list<mc_filter> systemfilterlist;
	std::list<mc_filter> userfilterlist;

};

#endif // qtGeneralFilterDialog_H
