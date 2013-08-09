#include "qtSyncDialog.h"
#include "qtClient.h"

qtSyncDialog::qtSyncDialog(QWidget *parent, int editID)
	: QDialog(parent)
{
	int rc;	
	//std::list<mc_sync_db> sl;
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	ui.tableWidget->setColumnWidth(0,23);
	ui.tableWidget->setColumnWidth(1,23);
	ui.tableWidget->setColumnWidth(2,100);
	ui.tableWidget->setColumnWidth(3,250);
	connect(ui.tableWidget,SIGNAL(itemActivated(QTableWidgetItem*)),ui.editButton,SLOT(click()));
	myparent = parent;
	syncID = editID;
	dbindex = -1;
	performer = NULL;
	icon = QIcon(":/Resources/icon.png");
	lock = QIcon(":/Resources/lock.png");
	file = QIcon(":/Resources/file.png");
	directory = QIcon(":/Resources/directory.png");
	loadcompleted = false;
}

qtSyncDialog::~qtSyncDialog()
{
	if(performer) delete performer;
}

void qtSyncDialog::showEvent(QShowEvent *event){
	mc_status s;
	int rc;


	rc = db_select_status(&s);
	if(rc) QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
	
	if(s.url == ""){		
		QMessageBox b(myparent);
		b.setText("No server configured");
		b.setInformativeText("To subscribe syncs, you need to set the server first.");
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();
		QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
		return;
	} else {
		QString _url = "https://";
		_url.append(s.url.c_str());
		_url.append("/bin.php");
		performer = new QtNetworkPerformer(_url,"trustCA.crt",s.acceptallcerts,true);
		SetBuf(&netibuf);
		SetBuf(&netobuf);
	}
		
	connect(performer,SIGNAL(finished(int)),this,SLOT(authed(int)));
	srv_auth_async(&netibuf,&netobuf,performer,s.uname,s.passwd,&authtime);
	
	QDialog::showEvent(event);
}

void qtSyncDialog::authed(int rc){
	int64 dummy;
	if(rc){
		QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
		return;
	} else {
		disconnect(performer,SIGNAL(finished(int)),this,SLOT(authed(int)));
		rc = srv_auth_process(&netobuf,&authtime,&dummy);
		if(rc){
			QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
			return;
		}
		connect(performer,SIGNAL(finished(int)),this,SLOT(listReceived(int)));
		srv_listsyncs_async(&netibuf,&netobuf,performer);
	}
	
}

void qtSyncDialog::listReceived(int rc){
	std::list<mc_sync> list;
	std::list<mc_sync_db> sl;
	int i;
	if(rc) {
		QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
		return;
	} else {
		rc = srv_listsyncs_process(&netobuf,&list);
		if(rc){
			QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
			return;
		}
	}

	if(list.size() > 0){
		//add to ComboBox
		i = 0;
		srvsynclist = vector<mc_sync>(list.begin(),list.end());
		for(mc_sync s : srvsynclist){
			if(s.crypted)
				ui.nameBox->addItem(lock,QString(s.name.c_str())+tr(" (encrypted)"));
			else
				ui.nameBox->addItem(icon,QString(s.name.c_str()));
			if(s.id == syncID){
				//Select the one to edit
				ui.nameBox->setCurrentIndex(i);
			}
			i++;
		}
		ui.fetchLabel->setVisible(false);
		ui.nameBox->setEnabled(true);
		ui.okButton->setEnabled(true);

		//populate db list		
		rc = db_list_sync(&sl);
		if(rc) QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
		dbsynclist.assign(sl.begin(),sl.end());

		//find matching db sync
		loadcompleted = true;
		filldbdata();

	} else {
		QMessageBox b(myparent);
		b.setText(tr("No Sync available on server"));
		b.setInformativeText(tr("There are no syncs on the server, you have to set them up on the server first."));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();
		QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
		return;
	}
}


void qtSyncDialog::accept(){
	int rc;
	int maxprio=0;
	QByteArray ckey;
	mc_sync_db *worksync;
	mc_sync_db newsync;

	if(dbindex == -1){ //new sync

		for (mc_sync_db s : dbsynclist){
			if(s.priority > maxprio) maxprio = s.priority;
		}

		newsync.priority = maxprio+1;
		newsync.filterversion = 0;
		memset(newsync.hash,0,16);
		newsync.lastsync = 0;
		newsync.id = srvsynclist[ui.nameBox->currentIndex()].id;
		newsync.name = srvsynclist[ui.nameBox->currentIndex()].name;

		worksync = &newsync;

	} else {
		worksync = &dbsynclist[dbindex];
	}
	worksync->path = qPrintable(ui.pathEdit->text().replace("\\","/"));
	if(worksync->path[worksync->path.length()-1] != '/') worksync->path.append("/");
	worksync->crypted = srvsynclist[ui.nameBox->currentIndex()].crypted;

	if(worksync->crypted){
		if(ui.keyEdit->text() != ""){
			QRegExp hexMatcher("^[0-9A-F]{64}$", Qt::CaseInsensitive);
			if (hexMatcher.exactMatch(ui.keyEdit->text())){
				ckey = QByteArray::fromHex(ui.keyEdit->text().toLatin1());
			} else {
				QMessageBox b(myparent);
				b.setText(tr("Please enter a valid 256-bit key in hex format"));
				b.setInformativeText(tr("Alternatively, you can leave the field empty to generate a new key, if you don't have one yet."));
				b.setStandardButtons(QMessageBox::Ok);
				b.setDefaultButton(QMessageBox::Ok);
				b.setIcon(QMessageBox::Warning);
				b.exec();
				return;
			}
		} else {
			ckey = QByteArray(32,'\0');
			if(crypt_randkey((unsigned char*)ckey.data())){
				QMessageBox b(myparent);
				b.setText(tr("Can't generate keys atm"));
				b.setInformativeText(tr("Please enter the CryptKey for this sync."));
				b.setStandardButtons(QMessageBox::Ok);
				b.setDefaultButton(QMessageBox::Ok);
				b.setIcon(QMessageBox::Warning);
				b.exec();
				return;
			} else {
				ui.keyEdit->setText(ckey.toHex());
				QMessageBox b(myparent);
				b.setText(tr("New Key generated"));
				b.setInformativeText(tr("A new key has been generated. Make a copy and store it in a safe place so you can enter it on other computers"));
				b.setStandardButtons(QMessageBox::Ok);
				b.setDefaultButton(QMessageBox::Ok);
				b.setIcon(QMessageBox::Information);
				b.exec();
				return;
			}
		}
		memcpy(worksync->cryptkey,ckey.constData(),32);
	} else memset(worksync->cryptkey,0,32);

	if(dbindex == -1){
		newsync.status = MC_SYNCSTAT_UNKOWN;
		rc = db_insert_sync(&newsync);
		if(rc) reject();
	} else {
		rc = db_update_sync(worksync);
		if(rc) reject();
	}
	QDialog::accept();
}

void qtSyncDialog::reject(){
	QDialog::reject();
}

void qtSyncDialog::on_browseButton_clicked(){
	QFileDialog d(this);
	d.setFileMode(QFileDialog::Directory);
	d.setNameFilter(tr("Directories"));
	d.setOption(QFileDialog::ShowDirsOnly,true);
	if(d.exec()){
		ui.pathEdit->setText(d.selectedFiles()[0]);
	};
}

void qtSyncDialog::on_nameBox_currentIndexChanged(int index){
	ui.keyEdit->setEnabled(srvsynclist[index].crypted);
	filldbdata();
}


void qtSyncDialog::on_addButton_clicked(){
	int rc;
	if(dbindex == -1){
		//insert sync first?
	}
	qtFilterDialog d(this,performer);

	d.exec();

	//refresh filterversion
	rc = db_select_sync(&dbsynclist[dbindex]);
	if(rc) return;
	listFilters();
}

void qtSyncDialog::on_removeButton_clicked(){
}

void qtSyncDialog::on_editButton_clicked(){
	int rc;
	int index = ui.tableWidget->selectedItems().at(0)->row();
	qtFilterDialog d(this,performer,filterlist[index].id);
	d.exec();

	rc = db_select_sync(&dbsynclist[dbindex]);
	if(rc) return;
	listFilters();
	ui.tableWidget->setRangeSelected(QTableWidgetSelectionRange(index,0,index,ui.tableWidget->columnCount()-1),true);
}

void qtSyncDialog::on_tableWidget_itemSelectionChanged(){
	if(ui.tableWidget->selectedItems().length() == 0){
		ui.removeButton->setEnabled(false);
		ui.editButton->setEnabled(false);
	} else {
		ui.removeButton->setEnabled(true);
		ui.editButton->setEnabled(true);
	}
}

void qtSyncDialog::filldbdata(){
	int i;
	int rc;
	int sid = srvsynclist[ui.nameBox->currentIndex()].id;
	if(loadcompleted){
		i = 0;
		for(mc_sync_db s : dbsynclist){
			if(s.id == sid){
				dbindex = i;
				//Path, Key
				ui.pathEdit->setText(s.path.c_str());
				if(s.crypted){
					ui.keyEdit->setText(QByteArray((const char*)s.cryptkey,32).toHex());
				} else {
					ui.keyEdit->setText("");
				}
				//Filters
				listFilters();
				return;
			}
			i++;
		}
		dbindex = -1; // = there is no matching db entry
	}
}


void qtSyncDialog::listFilters(){
	std::list<mc_filter> fl;
	int rc;
	
	if(dbsynclist[dbindex].filterversion < srvsynclist[ui.nameBox->currentIndex()].filterversion){
		rc = update_filters(dbsynclist[dbindex].id); //refresh when necessary, might block ui but happens rarely enough(?)
		if(rc) return;
	}
	rc = db_list_filter_sid(&fl,dbsynclist[dbindex].id);
	if(rc) return;
	filterlist.assign(fl.begin(),fl.end());
	ui.tableWidget->clearContents();
	ui.tableWidget->setRowCount(0);
	for(mc_filter f : filterlist){
		ui.tableWidget->insertRow(ui.tableWidget->rowCount());
		if(f.files) ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,0,new QTableWidgetItem(file,""));
		if(f.directories) ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,1,new QTableWidgetItem(directory,""));
		switch(f.type){
			case MC_FILTERT_MATCH_NAME:
				ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,2,new QTableWidgetItem(tr("Name")));
				break;
			case MC_FILTERT_MATCH_EXTENSION:
				ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,2,new QTableWidgetItem(tr("Extension")));
				break;
			case MC_FILTERT_MATCH_FULLNAME:
				ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,2,new QTableWidgetItem(tr("Full Name")));
				break;
			case MC_FILTERT_MATCH_PATH:
				ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,2,new QTableWidgetItem(tr("Path")));
				break;
			case MC_FILTERT_REGEX_NAME:
				ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,2,new QTableWidgetItem(tr("Name (regex)")));
				break;
			case MC_FILTERT_REGEX_EXTENSION:
				ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,2,new QTableWidgetItem(tr("Extension (regex)")));
				break;
			case MC_FILTERT_REGEX_FULLNAME:
				ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,2,new QTableWidgetItem(tr("Full Name (regex)")));
				break;
			case MC_FILTERT_REGEX_PATH:
				ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,2,new QTableWidgetItem(tr("Path (regex)")));
				break;
			default:
				ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,2,new QTableWidgetItem(tr("Unrecognized")));
		}
					
		ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,3,new QTableWidgetItem(f.rule.c_str()));
	}
}