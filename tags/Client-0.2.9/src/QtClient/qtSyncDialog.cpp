#include "QtSyncDialog.h"
#include "QtClient.h"

QtSyncDialog::QtSyncDialog(QWidget *parent, int editID)
	: QDialog(parent)
{
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	ui.filterTable->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Fixed);
	ui.filterTable->setColumnWidth(0,23);
	ui.filterTable->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Fixed);
	ui.filterTable->setColumnWidth(1,23);
	ui.filterTable->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Fixed);
	QFontMetrics fm(ui.filterTable->font()); 
	ui.filterTable->setColumnWidth(2,fm.width(tr("MFull Name (regex)M")));
	ui.filterTable->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);
	ui.fetchFilterLabel->setVisible(false);
	ui.needSubscribeLabel->setVisible(false);
	ui.deleteLabel->setVisible(false);
	ui.sendLabel->setVisible(false);
	connect(ui.filterTable,SIGNAL(itemActivated(QTableWidgetItem*)),ui.editButton,SLOT(click()));
	myparent = parent;
	syncID = editID;
	dbindex = -1;
	performer = NULL;
	icon = QIcon(":/Resources/icon.png");
	lock = QIcon(":/Resources/lock.png");
	file = QIcon(":/Resources/file.png");
	add = QIcon(":/Resources/add.png");
	directory = QIcon(":/Resources/directory.png");
	loadcompleted = false;
}

QtSyncDialog::~QtSyncDialog()
{
	if(performer) delete performer;
}

void QtSyncDialog::showEvent(QShowEvent *event){
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
		QMetaObject::invokeMethod(this,"reject",Qt::QueuedConnection);
		return;
	}

	QString _url = "https://";
	_url.append(s.url.c_str());
	_url.append("/bin.php");
	performer = new QtNetworkPerformer(_url,"trustCA.crt",s.acceptallcerts,true);
	SetBuf(&netibuf);
	SetBuf(&netobuf);
		
	connect(performer,SIGNAL(finished(int)),this,SLOT(authed(int)));
	srv_auth_async(&netibuf,&netobuf,performer,s.uname,s.passwd,&authtime);
	
	QDialog::showEvent(event);
}

void QtSyncDialog::authed(int rc){
	int64 dummy;
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(authed(int)));
	if(rc){
		reject();
		return;
	}

	rc = srv_auth_process(&netobuf,&authtime,&dummy);
	if(rc){
		if((rc) == MC_ERR_TIMEDIFF){	
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
	connect(performer,SIGNAL(finished(int)),this,SLOT(syncListReceived(int)));
	srv_listsyncs_async(&netibuf,&netobuf,performer);
	
}

void QtSyncDialog::syncListReceived(int rc){
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
		ui.nameBox->addItem(add,tr("New..."));
		ui.keyEdit->setEnabled(srvsynclist[0].crypted);

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
		ui.deleteButton->setEnabled(true);
		ui.pathEdit->setEnabled(true);
		ui.browseButton->setEnabled(true);
		ui.okButton->setEnabled(true);
		ui.globalButton->setEnabled(true); //might cause simultaneous use of performer but only if user wants it to
		ui.nameBox->setFocus();
	} else {
		//Directly pop the "new" dialog
		ui.fetchSyncLabel->setVisible(false);
		QtNewSyncDialog d(this,performer,&netibuf,&netobuf);
		if(d.exec()){
			startOver();
		} else {
			//When the new dialog is rejected, this dialog is rejected too
			//Otherwise the user ends up in a Loop
			reject();
		}

	}
}

void QtSyncDialog::filterListReceived(int rc){
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
	ui.filterTable->setEnabled(true);
	ui.addButton->setEnabled(true);
	listFilters();
}

void QtSyncDialog::filterDeleteReceived(int rc){
	std::list<mc_filter> list;
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(filterDeleteReceived(int)));
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
	ui.filterTable->setEnabled(true);
	ui.addButton->setEnabled(true);
	ui.removeButton->setEnabled(true);
	ui.editButton->setEnabled(true);
	listFilters();
}

void QtSyncDialog::syncDeleteReceived(int rc){
	const int sid = srvsynclist[ui.nameBox->currentIndex()].id;
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(syncDeleteReceived(int)));
	if(rc) {
		reject();
		return;
	}

	rc = srv_delsync_process(&netobuf);
	if(rc){
		reject();
		return;
	}

	ui.deleteLabel->setText(tr("<i>deleting locally...</i>"));


	std::list<mc_file> l;
	std::list<mc_file>::iterator lit,lend;
	rc = db_list_file_parent(&l, -sid);
	if(rc){
		reject();
		return;
	}
	lit = l.begin();
	lend = l.end();
	rc = db_execstr("BEGIN TRANSACTION");
	if(rc){
		reject();
		return;
	}

	while(lit != lend){
		QApplication::processEvents();
		rc = purge(&*lit,NULL);
		if(rc){
			reject();
			return;
		}
		++lit;
	}
	rc = db_execstr("COMMIT");
	if(rc){
		reject();
		return;
	}
	rc = db_delete_filter_sid(sid);
	if(rc){
		reject();
		return;
	}

	rc = db_delete_sync(sid);
	if(rc){
		reject();
		return;
	}


	ui.deleteLabel->setVisible(false);
	startOver();
}

void QtSyncDialog::accept(){
	int rc;
	int maxprio=0;
	QByteArray ckey;
	mc_sync_db *worksync;
	mc_sync_db newsync;

	if(ui.pathEdit->text().length() == 0){
		QMessageBox b(this);
		b.setText(tr("No path specified"));
		b.setInformativeText(tr("Please specify where the files are to be synced."));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();
		return;
	}

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
		rc = db_select_sync(worksync);
		if(rc){
			reject();
			return;
		}
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
				QMessageBox b(this);
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
				QMessageBox b(this);
				b.setText(tr("Can't generate keys atm"));
				b.setInformativeText(tr("Please enter the CryptKey for this sync."));
				b.setStandardButtons(QMessageBox::Ok);
				b.setDefaultButton(QMessageBox::Ok);
				b.setIcon(QMessageBox::Warning);
				b.exec();
				return;
			} else {
				ui.keyEdit->setText(ckey.toHex());
				QMessageBox b(this);
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

void QtSyncDialog::reject(){
	QDialog::reject();
}

void QtSyncDialog::on_deleteButton_clicked(){
	const int sid = srvsynclist[ui.nameBox->currentIndex()].id;
	bool cando = true;
	QMessageBox b(this);
	b.setText(tr("Are you sure you want to delete the Sync?"));
	b.setInformativeText(tr("It will be entirely deleted on the Server! Hit OK to delete."));
	b.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	b.setDefaultButton(QMessageBox::Ok);
	b.setIcon(QMessageBox::Warning);
	if(b.exec() == QMessageBox::Ok){
		if(QtWorkerThread::instance()->isRunning()){
			QMessageBox b2(this);
			b2.setText(tr("Worker stop required"));
			b2.setInformativeText(tr("Deleting the sync requires the worker to be stopped."));
			b2.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
			b2.setDefaultButton(QMessageBox::Ok);
			b2.setIcon(QMessageBox::Warning);
			if(b2.exec() == QMessageBox::Ok){
				if(QtWorkerThread::instance()->isRunning()){
					MC_INF("Stopping Worker");
					QtWorkerThread::instance()->quit();
				}
			} else {
				cando = false;
			}
		}
		if(cando){
			ui.deleteLabel->setVisible(true);
			ui.deleteLabel->setText(tr("<i>sending request...</i>"));
			disableAll();
			connect(performer,SIGNAL(finished(int)),this,SLOT(syncDeleteReceived(int)));
			srv_delsync_async(&netibuf,&netobuf,performer,sid);
		}
	}
}

void QtSyncDialog::on_browseButton_clicked(){
	QFileDialog d(this);
	d.setFileMode(QFileDialog::Directory);
	d.setNameFilter(tr("Directories"));
	d.setOption(QFileDialog::ShowDirsOnly,true);
	if(d.exec()){
		ui.pathEdit->setText(d.selectedFiles()[0]);
	};
}

void QtSyncDialog::on_nameBox_currentIndexChanged(int index){
	if(loadcompleted){
		if(index == ui.nameBox->count()-1){
			QtNewSyncDialog d(this,performer,&netibuf,&netobuf);
			d.exec();
			//Refresh listing
			startOver();
		} else {
			ui.deleteButton->setEnabled(true);
			ui.keyEdit->setEnabled(srvsynclist[index].crypted);
			filldbdata();
		}
	}
}


void QtSyncDialog::on_addButton_clicked(){
	int rc;
	QtFilterDialog d(this,performer,&netibuf,&netobuf,dbsynclist[dbindex].id);
	d.exec();

	//filterversion updated
	rc = db_select_sync(&dbsynclist[dbindex]);
	if(rc){
		reject();
		return;
	}

	listFilters();
}

void QtSyncDialog::on_removeButton_clicked(){
	const int index = ui.filterTable->selectedItems().at(0)->row();
	ui.filterTable->setEnabled(false);
	ui.addButton->setEnabled(false);
	ui.removeButton->setEnabled(false);
	ui.editButton->setEnabled(false);
	ui.sendLabel->setVisible(true);
	deletingfilterid = filterlist[index].id;
	connect(performer,SIGNAL(finished(int)),this,SLOT(filterDeleteReceived(int)));
	srv_delfilter_async(&netibuf,&netobuf,performer,&filterlist[index]);
}

void QtSyncDialog::on_editButton_clicked(){
	int rc;
	const int index = ui.filterTable->selectedItems().at(0)->row();
	QtFilterDialog d(this,performer,&netibuf,&netobuf,dbsynclist[dbindex].id,filterlist[index].id);
	d.exec();
	
	//filterversion updated
	rc = db_select_sync(&dbsynclist[dbindex]);
	if(rc){
		reject();
		return;
	}
	listFilters();
	ui.filterTable->setRangeSelected(QTableWidgetSelectionRange(index,0,index,ui.filterTable->columnCount()-1),true);
}

void QtSyncDialog::on_filterTable_itemSelectionChanged(){
	if(ui.filterTable->selectedItems().length() == 0){
		ui.removeButton->setEnabled(false);
		ui.editButton->setEnabled(false);
	} else {
		ui.removeButton->setEnabled(true);
		ui.editButton->setEnabled(true);
	}
}

void QtSyncDialog::on_globalButton_clicked(){
	bool refresh = true;
	//check wether a refresh from server is needed
	for(mc_sync_db& d : dbsynclist){
		for(mc_sync& s : srvsynclist){
			//If a single sync is up-to-date the globals must be too
			if(d.id == s.id && d.filterversion >= s.filterversion){
				refresh = false; 
				break;
			}
		}
		if(!refresh) break;
	}
	QtGeneralFilterDialog d(this,performer,&netibuf,&netobuf,refresh);
	d.exec();
}

void QtSyncDialog::filldbdata(){
	int i;
	const int sid = srvsynclist[ui.nameBox->currentIndex()].id;
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
				ui.filterTable->setEnabled(true);
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
		ui.filterTable->clearContents();
		ui.filterTable->setRowCount(0);
		ui.filterTable->setEnabled(false);
		ui.addButton->setEnabled(false);
		ui.needSubscribeLabel->setVisible(true);
	}
}

void QtSyncDialog::listFilters(){
	std::list<mc_filter> fl;
	
	if(dbsynclist[dbindex].filterversion < srvsynclist[ui.nameBox->currentIndex()].filterversion){		
		ui.filterTable->setEnabled(false);
		ui.addButton->setEnabled(false);
		ui.fetchFilterLabel->setVisible(true);
		connect(performer,SIGNAL(finished(int)),this,SLOT(filterListReceived(int)));
		srv_listfilters_async(&netibuf,&netobuf,performer,dbsynclist[dbindex].id);
	} else {
		listFilters_actual();
	}
}

void QtSyncDialog::listFilters_actual(){
	std::list<mc_filter> fl;
	int rc;
	rc = db_list_filter_sid(&fl,dbsynclist[dbindex].id);
	if(rc) return;
	filterlist.assign(fl.begin(),fl.end());
	ui.filterTable->clearContents();
	ui.filterTable->setRowCount(0);
	for(mc_filter& f : filterlist){
		ui.filterTable->insertRow(ui.filterTable->rowCount());
		if(f.files) ui.filterTable->setItem(ui.filterTable->rowCount()-1,0,new QTableWidgetItem(file,""));
		if(f.directories) ui.filterTable->setItem(ui.filterTable->rowCount()-1,1,new QTableWidgetItem(directory,""));
		switch(f.type){
			case MC_FILTERT_MATCH_NAME:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1,2,new QTableWidgetItem(tr("Name")));
				break;
			case MC_FILTERT_MATCH_EXTENSION:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1,2,new QTableWidgetItem(tr("Extension")));
				break;
			case MC_FILTERT_MATCH_FULLNAME:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1,2,new QTableWidgetItem(tr("Full Name")));
				break;
			case MC_FILTERT_MATCH_PATH:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1,2,new QTableWidgetItem(tr("Path")));
				break;
			case MC_FILTERT_REGEX_NAME:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1,2,new QTableWidgetItem(tr("Name (regex)")));
				break;
			case MC_FILTERT_REGEX_EXTENSION:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1,2,new QTableWidgetItem(tr("Extension (regex)")));
				break;
			case MC_FILTERT_REGEX_FULLNAME:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1,2,new QTableWidgetItem(tr("Full Name (regex)")));
				break;
			case MC_FILTERT_REGEX_PATH:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1,2,new QTableWidgetItem(tr("Path (regex)")));
				break;
			default:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1,2,new QTableWidgetItem(tr("Unrecognized")));
		}
					
		ui.filterTable->setItem(ui.filterTable->rowCount()-1,3,new QTableWidgetItem(f.rule.c_str()));
	}
}

void QtSyncDialog::startOver(){
	loadcompleted = false;
	ui.fetchSyncLabel->setVisible(true);
	ui.nameBox->clear();
	ui.pathEdit->clear();
	ui.keyEdit->clear();
	ui.filterTable->clearContents();
	disableAll();
	connect(performer,SIGNAL(finished(int)),this,SLOT(syncListReceived(int)));
	srv_listsyncs_async(&netibuf,&netobuf,performer);
}

void QtSyncDialog::disableAll(){
	ui.nameBox->setEnabled(false);
	ui.deleteButton->setEnabled(false);
	ui.pathEdit->setEnabled(false);
	ui.browseButton->setEnabled(false);
	ui.keyEdit->setEnabled(false);
	ui.okButton->setEnabled(false);
	ui.globalButton->setEnabled(false); 
	ui.addButton->setEnabled(false);
	ui.editButton->setEnabled(false);
	ui.removeButton->setEnabled(false);
	ui.filterTable->setEnabled(false);
	ui.needSubscribeLabel->setVisible(false);
}