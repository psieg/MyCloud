#ifndef qtPasswordChangeDialog_H
#define qtPasswordChangeDialog_H

#include <QtWidgets/QDialog>
#include <QtGui/QShowEvent>
#include "ui_qtPasswordChangeDialog.h"
#include "mc_db.h"
#include "mc_srv.h"
#include "mc_filter.h"
#include "mc_types.h"

class qtPasswordChangeDialog : public QDialog
{
	Q_OBJECT

public:
	qtPasswordChangeDialog(QWidget *parent);
	~qtPasswordChangeDialog();

public slots:
	void accept();
	
private slots:
	void authed(int);
	void replyReceived(int);

protected:
	void showEvent(QShowEvent *event);

private:

	Ui::qtPasswordChangeDialog ui;
	QtNetworkPerformer *performer;
	mc_buf netibuf,netobuf;
	mc_status s;

};

#endif // qtPasswordChangeDialog_H
