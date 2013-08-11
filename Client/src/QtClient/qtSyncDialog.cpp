#include "qtSyncDialog.h"
#include "qtClient.h"

qtSyncDialog::qtSyncDialog(QWidget *parent, int editID)
	: QDialog(parent)
{
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	ui.tableWidget->setColumnWidth(0,23);
	ui.tableWidget->setColumnWidth(1,23);
	ui.tableWidget->setColumnWidth(2,100);
	ui.tableWidget->setColumnWidth(3,230);
	ui.fetchFilterLabel->setVisible(false);
	ui.needSubscribeLabel->setVisible(false);
	ui.sendLabel->setVisible(false);
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
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(authed(int)));
	if(rc){
		reject();
		return;
	}

	rc = srv_auth_process(&netobuf,&authtime,&dummy);
	if(rc){
		reject();
		return;
	}
	connect(performer,SIGNAL(finished(int)),this,SLOT(syncListReceived(int)));
	srv_listsyncs_async(&netibuf,&netobuf,performer);
	
}

void qtSyncDialog::syncListReceived(int rc){
	std::list<mc_sync> list;
	std::list<mc_sync_db> sl;
	int i;
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(syncListReceived(int)));
	if(rc) {
		reject();
		return;
	}

	rc = srv_listsyncs_process(&netobuf,&list);
	if(rc){
		reject();
		return;
	}

	if(list.size() > 0){
		//add to ComboBox
		i = 0;
		srvsynclist = vector<mc_sync>(list.begin(),list.end());
		for(mc_sync& s : srvsynclist){
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

		//populate db list
		rc = db_list_sync(&sl);
		if(rc){ 
			reject();
			return; 
		}
		dbsynclist.assign(sl.begin(),sl.end());

		//find matching db sync
		loadcompleted = true;
		filldbdata();
		
		ui.fetchSyncLabel->setVisible(false);
		ui.nameBox->setEnabled(true);
		ui.okButton->setEnabled(true);
		ui.globalButton->setEnabled(true); //might cause simultaneous use of performer but only if user wants it to
	} else {
		QMessageBox b(myparent);
		b.setText(tr("No Sync available on server"));
		b.setInformativeText(tr("There are no syncs on the server, you have to set them up on the server first."));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();
		reject();
		return;
	}
}

void qtSyncDialog::filterListReceived(int rc){
	std::list<mc_filter> list;
	mc_sync_ctx ctx;
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(filterListReceived(int)));
	init_sync_ctx(&ctx,&dbsynclist[dbindex],&list);
	if(rc) {
		reject();
		return;
	}

	rc = srv_listfilters_process(&netobuf,&list);
	if(rc){
		reject();
		return;
	}

	rc = crypt_filterlist_fromsrv(&ctx,dbsynclist[dbindex].name,&list);
	if(rc){
		reject();
		return;
	}

	rc = update_filters(dbsynclist[dbindex].id,&list);
	if(rc){
		reject();
		return;
	}

	dbsynclist[dbindex].filterversion = srvsynclist[ui.nameBox->currentIndex()].filterversion;
	rc = db_update_sync(&dbsynclist[dbindex]);
	if(rc){
		reject();
		return;
	}

	ui.fetchFilterLabel->setVisible(false);
	ui.tableWidget->setEnabled(true);
	ui.addButton->setEnabled(true);
	listFilters();
}


void qtSyncDialog::deleteReceived(int rc){
	std::list<mc_filter> list;
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(deleteReceived(int)));
	if(rc) {
		reject();
		return;
	}

	rc = srv_delfilter_process(&netobuf);
	if(rc){
		reject();
		return;
	}

	rc = db_delete_filter(deletingfilterid);
	if(rc){
		reject();
		return;
	}

	//if successful, the server will have increased the filterversion by one
	rc = db_select_sync(&dbsynclist[dbindex]);
	if(rc){
		reject();
		return;
	}
	dbsynclist[dbindex].filterversion += 1;
	rc = db_update_sync(&dbsynclist[dbindex]);
	if(rc){
		reject();
		return;
	}

	ui.sendLabel->setVisible(false);
	ui.tableWidget->setEnabled(true);
	ui.addButton->setEnabled(true);
	ui.removeButton->setEnabled(true);
	ui.editButton->setEnabled(true);
	listFilters();
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
		if(rc){
			reject();
			return;
		}
	} else {
		rc = db_update_sync(worksync);
		if(rc){
			reject();
			return;
		}
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
	qtFilterDialog d(this,performer,&netibuf,&netobuf,dbsynclist[dbindex].id);
	d.exec();

	//filterversion updated
	rc = db_select_sync(&dbsynclist[dbindex]);
	if(rc){
		reject();
		return;
	}

	listFilters();
}

void qtSyncDialog::on_removeButton_clicked(){
	const int index = ui.tableWidget->selectedItems().at(0)->row();
	ui.tableWidget->setEnabled(false);
	ui.addButton->setEnabled(false);
	ui.removeButton->setEnabled(false);
	ui.editButton->setEnabled(false);
	ui.sendLabel->setVisible(true);
	deletingfilterid = filterlist[index].id;
	bool ok = connect(performer,SIGNAL(finished(int)),this,SLOT(deleteReceived(int)));
	srv_delfilter_async(&netibuf,&netobuf,performer,&filterlist[index]);
}

void qtSyncDialog::on_editButton_clicked(){
	int rc;
	const int index = ui.tableWidget->selectedItems().at(0)->row();
	qtFilterDialog d(this,performer,&netibuf,&netobuf,dbsynclist[dbindex].id,filterlist[index].id);
	d.exec();
	
	//filterversion updated
	rc = db_select_sync(&dbsynclist[dbindex]);
	if(rc){
		reject();
		return;
	}
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

void qtSyncDialog::on_globalButton_clicked(){
	int rc;
	std::list<mc_sync_db> sl;
	qtGeneralFilterDialog d(this,performer,&netibuf,&netobuf);
	d.exec();
	//re-populate db list
	rc = db_list_sync(&sl);
	if(rc){ 
		reject();
		return; 
	}
	dbsynclist.assign(sl.begin(),sl.end());
	filldbdata();
}

void qtSyncDialog::filldbdata(){
	int i;
	int rc;
	int sid = srvsynclist[ui.nameBox->currentIndex()].id;
	if(loadcompleted){
		i = 0;		
		for(mc_sync_db& s : dbsynclist){
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
				ui.tableWidget->setEnabled(true);
				ui.addButton->setEnabled(true);
				ui.needSubscribeLabel->setVisible(false);
				listFilters();
				return;
			}
			i++;
		}
		//reach here when no local sync found
		dbindex = -1; // = there is no matching db entry
		ui.pathEdit->setText("");
		ui.keyEdit->setText("");
		ui.tableWidget->clearContents();
		ui.tableWidget->setRowCount(0);
		ui.tableWidget->setEnabled(false);
		ui.addButton->setEnabled(false);
		ui.needSubscribeLabel->setVisible(true);
	}
}

void qtSyncDialog::listFilters(){
	std::list<mc_filter> fl;
	int rc;
	
	if(dbsynclist[dbindex].filterversion < srvsynclist[ui.nameBox->currentIndex()].filterversion){		
		ui.tableWidget->setEnabled(false);
		ui.addButton->setEnabled(false);
		ui.fetchFilterLabel->setVisible(true);
		connect(performer,SIGNAL(finished(int)),this,SLOT(filterListReceived(int)));
		srv_listfilters_async(&netibuf,&netobuf,performer,dbsynclist[dbindex].id);
	} else {
		listFilters_actual();
	}
}

void qtSyncDialog::listFilters_actual(){
	std::list<mc_filter> fl;
	int rc;
	rc = db_list_filter_sid(&fl,dbsynclist[dbindex].id);
	if(rc) return;
	filterlist.assign(fl.begin(),fl.end());
	ui.tableWidget->clearContents();
	ui.tableWidget->setRowCount(0);
	for(mc_filter& f : filterlist){
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