#include "qtGeneralFilterDialog.h"

qtGeneralFilterDialog::qtGeneralFilterDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf)
	: QDialog(parent)
{
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	myparent = parent;
	performer = parentperf;
	netibuf = parentibuf;
	netobuf = parentobuf;
	//ui.sendLabel->setVisible(false);
}

qtGeneralFilterDialog::~qtGeneralFilterDialog()
{
}

void qtGeneralFilterDialog::showEvent(QShowEvent *event){
	int rc;
	/*
	if(filter.id != -1){
		rc = db_select_filter(&filter);
		if(rc) return;
		ui.filesBox->setChecked(filter.files);
		ui.directoriesBox->setChecked(filter.directories);
		ui.typeBox->setCurrentIndex(typeToIndex(filter.type));
		ui.valueEdit->setText(filter.rule.c_str());
	}
	*/
	QDialog::showEvent(event);
}

void qtGeneralFilterDialog::accept(){
	QDialog::accept();
}

