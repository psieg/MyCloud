#include "QtPasswordChangeDialog.h"

QtPasswordChangeDialog::QtPasswordChangeDialog(QWidget *parent)
	: QDialog(parent)
{
	int rc;
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	ui.statusLabel->setText("");

	rc = db_select_status(&s);
	if(!rc){
		ui.urlLabel->setText(s.url.c_str());
		ui.nameLabel->setText(s.uname.c_str());
	}
}

QtPasswordChangeDialog::~QtPasswordChangeDialog()
{
}

void QtPasswordChangeDialog::showEvent(QShowEvent *event){
	/*connect(performer,SIGNAL(finished(int)),this,SLOT(filterListReceived(int)));
	srv_listfilters_async(netibuf,netobuf,performer,0);*/
}

void QtPasswordChangeDialog::authed(int rc){
	int64 dummy;
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(authed(int)));
	if(rc){
		reject();
		return;
	}

	rc = srv_auth_process(&netobuf,&authtime,&dummy);
	if(rc){
		if(rc == MC_ERR_LOGIN){
			QMessageBox b(this);
			b.setText(tr("Old login rejected"));
			b.setInformativeText(tr("The server did not accept your old password."));
			b.setStandardButtons(QMessageBox::Ok);
			b.setDefaultButton(QMessageBox::Ok);
			b.setIcon(QMessageBox::Warning);
			b.exec();
			//no reject here
			return;
		}
		if(rc == MC_ERR_TIMEDIFF){	
			QMessageBox b(this);
			b.setText(tr("Time difference too high"));
			b.setInformativeText(tr("Syncronisation only works if the Client and Server clocks are synchronized.\nUse NTP (recommended) or set the time manually."));
			b.setStandardButtons(QMessageBox::Ok);
			b.setDefaultButton(QMessageBox::Ok);
			b.setIcon(QMessageBox::Warning);
			b.exec();
		}
		reject();
		return;
	}
	
	ui.statusLabel->setText(tr("<i>Sending request...</i>"));
	QDialog::accept();
}
void QtPasswordChangeDialog::replyReceived(int rc){
	/*std::list<mc_filter> list;
	mc_sync_db s;
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(filterListReceived(int)));
	if(rc) {
		reject();
		return;
	}

	rc = srv_listfilters_process(netobuf,&list);
	if(rc){
		reject();
		return;
	}

	//global filters not encrypted

	rc = update_filters(0,&list);
	if(rc){
		reject();
		return;
	}
	
	ui.fetchFilterLabel->setVisible(false);
	ui.filterTable->setEnabled(true);
	listFilters()*/
}

void QtPasswordChangeDialog::accept(){
	QString _url = "https://";
	_url.append(s.url.c_str());
	_url.append("/bin.php");
	performer = new QtNetworkPerformer(_url,"trustCA.crt",s.acceptallcerts,true);
	SetBuf(&netibuf);
	SetBuf(&netobuf);
		
	connect(performer,SIGNAL(finished(int)),this,SLOT(authed(int)));
	srv_auth_async(&netibuf,&netobuf,performer,s.uname,s.passwd,&authtime);
	ui.okButton->setVisible(false);
	ui.statusLabel->setText(tr("<i>Authenticating...</i>"));
	//QDialog::accept();
}
