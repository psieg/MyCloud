#include "qtFilterDialog.h"

qtFilterDialog::qtFilterDialog(QWidget *parent, QtNetworkPerformer *parentperf, int editID)
	: QDialog(parent)
{
	int rc;	
	//std::list<mc_sync_db> sl;
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	myparent = parent;
	performer = parentperf;
	filter.id = editID;
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
		switch(filter.type){
			case MC_FILTERT_MATCH_NAME:
				ui.typeBox->setCurrentIndex(0);
				break;
			case MC_FILTERT_MATCH_EXTENSION:
				ui.typeBox->setCurrentIndex(1);
				break;
			case MC_FILTERT_MATCH_FULLNAME:
				ui.typeBox->setCurrentIndex(2);
				break;
			case MC_FILTERT_MATCH_PATH:
				ui.typeBox->setCurrentIndex(3);
				break;
			case MC_FILTERT_REGEX_NAME:
				ui.typeBox->setCurrentIndex(4);
				break;
			case MC_FILTERT_REGEX_EXTENSION:
				ui.typeBox->setCurrentIndex(5);
				break;
			case MC_FILTERT_REGEX_FULLNAME:
				ui.typeBox->setCurrentIndex(6);
				break;
			case MC_FILTERT_REGEX_PATH:
				ui.typeBox->setCurrentIndex(7);
				break;
			default:
				ui.typeBox->setCurrentIndex(8);
		}
		ui.valueEdit->setText(filter.rule.c_str());
	}

	QDialog::showEvent(event);
}

void qtFilterDialog::accept(){
	QDialog::accept();
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

