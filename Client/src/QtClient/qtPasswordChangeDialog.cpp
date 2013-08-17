#include "qtPasswordChangeDialog.h"

qtPasswordChangeDialog::qtPasswordChangeDialog(QWidget *parent)
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

qtPasswordChangeDialog::~qtPasswordChangeDialog()
{
}

void qtPasswordChangeDialog::showEvent(QShowEvent *event){
	/*connect(performer,SIGNAL(finished(int)),this,SLOT(filterListReceived(int)));
	srv_listfilters_async(netibuf,netobuf,performer,0);*/
}

void qtPasswordChangeDialog::authed(int rc){
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
void qtPasswordChangeDialog::replyReceived(int rc){
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

void qtPasswordChangeDialog::accept(){
	QDialog::accept();
}
