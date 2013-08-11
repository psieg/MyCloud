#ifndef qtGeneralFilterDialog_H
#define qtGeneralFilterDialog_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <Qtwidgets/QFileDialog>
#include <QtGui/QShowEvent>
#include "ui_qtGeneralFilterDialog.h"
#include "mc_db.h"
#include "mc_srv.h"
#include "mc_filter.h"
#include "mc_types.h"

class qtGeneralFilterDialog : public QDialog
{
	Q_OBJECT

public:
	qtGeneralFilterDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf, bool needrefresh);
	~qtGeneralFilterDialog();

public slots:
	void accept();
	
private slots:
	void filterListReceived(int);

protected:
	void showEvent(QShowEvent *event);

private:
	void listFilters();

	Ui::qtGeneralFilterDialog ui;
	QWidget *myparent;
	QtNetworkPerformer *performer;
	mc_buf *netibuf,*netobuf;
	QIcon file,directory;
	std::vector<mc_filter> filterlist;
	bool refresh;

};

#endif // qtGeneralFilterDialog_H
