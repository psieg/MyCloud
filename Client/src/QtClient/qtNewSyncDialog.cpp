#include "QtNewSyncDialog.h"

QtNewSyncDialog::QtNewSyncDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf)
	: QDialog(parent)
{
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setFixedSize(sizeHint());
	myparent = parent;
	performer = parentperf;
	netibuf = parentibuf;
	netobuf = parentobuf;
	ui.sendLabel->setVisible(false);
}

QtNewSyncDialog::~QtNewSyncDialog()
{
}

void QtNewSyncDialog::showEvent(QShowEvent *event){
	QDialog::showEvent(event);
}

void QtNewSyncDialog::accept(){
	int rc;
	if(ui.nameEdit->text().length() == 0){
		QMessageBox b(this);
		b.setText(tr("No name set"));
		b.setInformativeText(tr("Please choose a name for the new Sync."));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();
		return;
	}
	
	sync.id = MC_SYNCID_NONE;
	

	//connect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	//srv_putfilter_async(netibuf,netobuf,performer,&filter);

	
	ui.nameEdit->setEnabled(false);
	ui.encryptedBox->setEnabled(false);
	ui.okButton->setVisible(false);
	ui.sendLabel->setVisible(true);

	//replyReceived does the accept
}

void QtNewSyncDialog::replyReceived(int rc){
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	if(rc){
		reject();
		return;
	}

	/*rc = srv_putfilter_process(netobuf,&filter.id);
	if(rc){
		reject();
		return;
	}

	if(id == -1) rc = db_insert_filter(&filter);
	else rc = db_update_filter(&filter);
	if(rc){
		reject();
		return;
	}*/
	
	QDialog::accept();
}
