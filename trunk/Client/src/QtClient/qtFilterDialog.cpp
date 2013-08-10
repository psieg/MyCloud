#include "qtFilterDialog.h"

qtFilterDialog::qtFilterDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf, int syncID, int editID)
	: QDialog(parent)
{
	int rc;	
	//std::list<mc_sync_db> sl;
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	myparent = parent;
	performer = parentperf;
	netibuf = parentibuf;
	netobuf = parentobuf;
	filter.id = editID;
	filter.sid = syncID;
	ui.sendLabel->setVisible(false);
}

qtFilterDialog::~qtFilterDialog()
{
}

void qtFilterDialog::showEvent(QShowEvent *event){
	int rc;

	if(filter.id != -1){
		rc = db_select_filter(&filter);
		if(rc) return;
		ui.filesBox->setChecked(filter.files);
		ui.directoriesBox->setChecked(filter.directories);
		ui.typeBox->setCurrentIndex(typeToIndex(filter.type));
		ui.valueEdit->setText(filter.rule.c_str());
	}

	QDialog::showEvent(event);
}

void qtFilterDialog::accept(){
	filter.files = ui.filesBox->isChecked();
	filter.directories = ui.directoriesBox->isChecked();
	filter.type = indexToType(ui.typeBox->currentIndex());
	filter.rule = qPrintable(ui.valueEdit->text());

	connect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	srv_putfilter_async(netibuf,netobuf,performer,&filter);
	ui.filesBox->setEnabled(false);
	ui.directoriesBox->setEnabled(false);
	ui.typeBox->setEnabled(false);
	ui.valueEdit->setEnabled(false);
	ui.browseButton->setEnabled(false);
	ui.okButton->setVisible(false);
	ui.sendLabel->setVisible(true);

	//replyReceived does the accept
}

void qtFilterDialog::replyReceived(int rc){
	mc_sync_db s;
	int id = filter.id;
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	if(rc){
		reject();
		return;
	} else {
		rc = srv_putfilter_process(netobuf,&filter.id);
		if(rc){
			reject();
			return;
		}
	}

	if(id == -1) rc = db_insert_filter(&filter);
	else rc = db_update_filter(&filter);
	if(rc){
		reject();
		return;
	}

	//if successful, the server will have increased the filterversion by one
	s.id = filter.sid;
	rc = db_select_sync(&s);
	if(rc){
		reject();
		return;
	}
	s.filterversion += 1;
	rc = db_update_sync(&s);
	if(rc){
		reject();
		return;
	}

	QDialog::accept();
}


void qtFilterDialog::on_typeBox_currentIndexChanged(int index){
	//ui.browseButton->setEnabled(indexToType(index) == MC_FILTERT_MATCH_PATH);
}

void qtFilterDialog::on_browseButton_clicked(){
	QFileDialog d(this);
	//d.setFileMode(QFileDialog::Directory);
	//d.setNameFilter(tr("Directories"));
	//d.setOption(QFileDialog::ShowDirsOnly,true);
	if(d.exec()){
		ui.valueEdit->setText(d.selectedFiles()[0]);
	};
}

int qtFilterDialog::typeToIndex(MC_FILTERTYPE type){
	switch(type){
		case MC_FILTERT_MATCH_NAME:
			return 0;
		case MC_FILTERT_MATCH_EXTENSION:
			return 1;
		case MC_FILTERT_MATCH_FULLNAME:
			return 2;
		case MC_FILTERT_MATCH_PATH:
			return 3;
		case MC_FILTERT_REGEX_NAME:
			return 4;
		case MC_FILTERT_REGEX_EXTENSION:
			return 5;
		case MC_FILTERT_REGEX_FULLNAME:
			return 6;
		case MC_FILTERT_REGEX_PATH:
			return 7;
		default:
			return 0;
	}
}

MC_FILTERTYPE qtFilterDialog::indexToType(int index){
	switch(index){
		case 0:
			return MC_FILTERT_MATCH_NAME;
		case 1:
			return MC_FILTERT_MATCH_EXTENSION;
		case 2:
			return MC_FILTERT_MATCH_FULLNAME;
		case 3:
			return MC_FILTERT_MATCH_PATH;
		case 4:
			return MC_FILTERT_REGEX_NAME;
		case 5:
			return MC_FILTERT_REGEX_EXTENSION;
		case 6:
			return MC_FILTERT_REGEX_FULLNAME;
		case 7:
			return MC_FILTERT_REGEX_PATH;
		default:
			return MC_FILTERT_MATCH_NAME;
	}
}