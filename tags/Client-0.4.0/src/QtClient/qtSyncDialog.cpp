#include "qtSyncDialog.h"
#include "qtClient.h"

QtSyncDialog::QtSyncDialog(QWidget *parent, int editID)
	: QDialog(parent)
{
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	ui.filterTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
	ui.filterTable->setColumnWidth(0, 23);
	ui.filterTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
	ui.filterTable->setColumnWidth(1, 23);
	ui.filterTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
	QFontMetrics fm(ui.filterTable->font()); 
	ui.filterTable->setColumnWidth(2, fm.width(tr("MFull Name (regex)M")));
	ui.filterTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
	ui.fetchFilterLabel->setVisible(false);
	ui.fetchShareLabel->setVisible(false);
	ui.needSubscribeLabel->setVisible(false);
	connect(ui.filterTable, SIGNAL(itemActivated(QTableWidgetItem*)), ui.editFilterButton, SLOT(click()));
	myparent = parent;
	syncID = editID;
	myUID = 0; //set on auth
	dbindex = -1;
	performer = NULL;
	icon = QIcon(":/Resources/icon.png");
	lock = QIcon(":/Resources/lock.png");
	file = QIcon(":/Resources/file.png");
	directory = QIcon(":/Resources/directory.png");
	add = QIcon(":/Resources/add.png");
	user = QIcon(":/Resources/user.png");
	loadcompleted = false;
	keyringloaded = false;
	SetBuf(&netibuf);
	SetBuf(&netobuf);
	memset(newsync.cryptkey, 0, 32); // for change-comparison of new syncs
}

QtSyncDialog::~QtSyncDialog()
{
	if (performer) delete performer;
	FreeBuf(&netibuf);
	FreeBuf(&netobuf);
}

void QtSyncDialog::showEvent(QShowEvent *event) {
	mc_status s;
	int rc;


	rc = db_select_status(&s);
	if (rc) {
		QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
		return;
	}
	
	if (s.url == "") {
		QMessageBox::warning(myparent, tr("No server configured"), 
			tr("To subscribe syncs, you need to set the server first."));
		QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
		return;
	}

	QString _url = "https://";
	_url.append(s.url.c_str());
	_url.append("/bin.php");
	performer = new QtNetworkPerformer(_url, "trustCA.crt", s.acceptallcerts, true);
		
	connect(performer, SIGNAL(finished(int)), this, SLOT(authed(int)));
	srv_auth_async(&netibuf, &netobuf, performer, s.uname, s.passwd, &authtime);
	
	QDialog::showEvent(event);
}

void QtSyncDialog::authed(int rc) {
	int64 basedate;	
	mc_status s;

	disconnect(performer, SIGNAL(finished(int)), this, SLOT(authed(int)));
	if (rc) {
		reject();
		return;
	}

	rc = srv_auth_process(&netobuf, &authtime, &basedate, &myUID);
	if (rc) {
		if ((rc) == MC_ERR_TIMEDIFF) {	
			QMessageBox::warning(this, tr("Time difference too high"), 
				tr("Syncronisation only works if the Client and Server clocks are synchronized.\nUse NTP (recommended) or set the time manually."));
		}
		reject();
		return;
	}

	rc = db_select_status(&s);
	if (rc) {
		reject();
		return;
	}

	if (s.basedate == 0) {
		s.uid = myUID;
		rc = db_update_status(&s);
		if (rc) {
			reject();
			return;
		}
	} else if (s.basedate != basedate) {
		// we can't do the routine in the UI thread, start the worker
		QMessageBox::warning(this, tr("Server reset"), 
			tr("The server has been reset, which changed all IDs.\n"
			"We will now start the worker to adjust to the changes."));
		QtClient::instance()->startNewRun();
		reject();
		return;
	}

	connect(performer, SIGNAL(finished(int)), this, SLOT(syncListReceived(int)));
	srv_listsyncs_async(&netibuf, &netobuf, performer);
	
}

void QtSyncDialog::syncListReceived(int rc) {
	std::list<mc_sync> list;
	std::list<mc_sync_db> sl;
	int i;
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(syncListReceived(int)));
	if (rc) {
		reject();
		return;
	}

	rc = srv_listsyncs_process(&netobuf, &list);
	if (rc) {
		reject();
		return;
	}

	if (list.size() > 0) {
		//add to ComboBox
		i = 0;
		srvsynclist = vector<mc_sync>(list.begin(), list.end());
		for (mc_sync& s : srvsynclist) {
			QString details;

			if (s.crypted)
				if (s.uid != myUID) ui.nameBox->addItem(lock, QString(s.name.c_str())+tr(" (shared)"));
				else ui.nameBox->addItem(lock, QString(s.name.c_str()));
			else
				if (s.uid != myUID) ui.nameBox->addItem(icon, QString(s.name.c_str())+tr(" (shared)"));
				else ui.nameBox->addItem(icon, QString(s.name.c_str()));
			if (s.id == syncID) {
				//Select the one to edit
				ui.nameBox->setCurrentIndex(i);
			}
			i++;
		}
		ui.nameBox->addItem(add, tr("New..."));
		ui.keyEdit->setEnabled(srvsynclist[ui.nameBox->currentIndex()].crypted);

		//populate db list
		rc = db_list_sync(&sl);
		if (rc) { 
			reject();
			return; 
		}
		dbsynclist.assign(sl.begin(), sl.end());

		//find matching db sync
		loadcompleted = true;
		filldbdata();
		
		ui.fetchSyncLabel->setVisible(false);
		ui.nameBox->setEnabled(true);
		ui.deleteSyncButton->setEnabled(true);
		ui.pathEdit->setEnabled(true);
		ui.browseButton->setEnabled(true);
		ui.okButton->setEnabled(true);
		ui.globalFilterButton->setEnabled(true); //might cause simultaneous use of performer but only if user wants it to
		ui.nameBox->setFocus();
	} else {
		//Directly pop the "new" dialog
		ui.fetchSyncLabel->setVisible(false);
		QtNewSyncDialog d(this, performer, &netibuf, &netobuf);
		if (d.exec()) {
			startOver();
		} else {
			//When the new dialog is rejected, this dialog is rejected too
			//Otherwise the user ends up in a Loop
			reject();
		}

	}
}

void QtSyncDialog::globalFilterListReceived(int rc) {
	std::list<mc_filter> list;
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(globalFilterListReceived(int)));
	
	rc = srv_listfilters_process(&netobuf, &list);
	if (rc) {
		reject();
		return;
	}

	rc = update_filters(0, &list);
	if (rc) {
		reject();
		return;
	}
	
	connect(performer, SIGNAL(finished(int)), this, SLOT(filterListReceived(int)));
	srv_listfilters_async(&netibuf, &netobuf, performer, dbsynclist[dbindex].id);
		
}

void QtSyncDialog::filterListReceived(int rc) {
	std::list<mc_filter> list;
	mc_sync_ctx ctx;
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(filterListReceived(int)));
	init_sync_ctx(&ctx, &dbsynclist[dbindex], &list);
	if (rc) {
		reject();
		return;
	}

	rc = srv_listfilters_process(&netobuf, &list);
	if (rc) {
		reject();
		return;
	}

	rc = crypt_filterlist_fromsrv(&ctx, dbsynclist[dbindex].name, &list);
	if (rc) {
		reject();
		return;
	}

	rc = update_filters(dbsynclist[dbindex].id, &list);
	if (rc) {
		reject();
		return;
	}

	dbsynclist[dbindex].filterversion = srvsynclist[ui.nameBox->currentIndex()].filterversion;
	rc = db_update_sync(&dbsynclist[dbindex]);
	if (rc) {
		reject();
		return;
	}

	ui.fetchFilterLabel->setVisible(false);
	listFilters();
}

void QtSyncDialog::shareListReceived(int rc) {
	std::list<mc_share> list;
	std::list<int> idlist;
	mc_user u;
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(shareListReceived(int)));
	
	rc = srv_listshares_process(&netobuf, &list);
	if (rc) {
		reject();
		return;
	}

	rc = db_delete_share_sid(dbsynclist[dbindex].id);
	if (rc) {
		reject();
		return;
	}
	
	// Find out whether we know the owner
	u.id = dbsynclist[dbindex].uid;
	rc = db_select_user(&u);
	if (rc == SQLITE_DONE) idlist.push_back(u.id);
	else if (rc) { 
		reject(); 
		return; 
	}

	for (mc_share& s : list) {
		rc = db_insert_share(&s);
		if (rc) {
			reject();
			return;
		}

		// Find out which users we don't know yet
		u.id = s.uid;
		rc = db_select_user(&u);
		if (rc == SQLITE_DONE) idlist.push_back(u.id);
		else if (rc) { 
			reject(); 
			return; 
		}
	}

	if (idlist.size() != 0) { // some unknown users		
		connect(performer, SIGNAL(finished(int)), this, SLOT(userListReceived(int)));
		srv_idusers_async(&netibuf, &netobuf, performer, idlist);
	} else {
		dbsynclist[dbindex].shareversion = srvsynclist[ui.nameBox->currentIndex()].shareversion;
		rc = db_update_sync(&dbsynclist[dbindex]);
		if (rc) {
			reject();
			return;
		}

		ui.fetchShareLabel->setVisible(false);
		listShares();
	}
}

void QtSyncDialog::userListReceived(int rc) {
	std::list<mc_user> list;
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(userListReceived(int)));
	if (rc) {
		reject();
		return;
	}
	
	rc = srv_idusers_process(&netobuf, &list);
	if (rc) {
		reject();
		return;
	}

	for (mc_user& u : list) {
		rc = db_insert_user(&u);
		if (rc) {
			reject();
			return;
		}
	}

	// update the shareversion only once the users are known
	dbsynclist[dbindex].shareversion = srvsynclist[ui.nameBox->currentIndex()].shareversion;
	rc = db_update_sync(&dbsynclist[dbindex]);
	if (rc) {
		reject();
		return;
	}
	
	ui.fetchShareLabel->setVisible(false);
	listShares();

}

void QtSyncDialog::filterDeleteReceived(int rc) {
	std::list<mc_filter> list;
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(filterDeleteReceived(int)));
	if (rc) {
		reject();
		return;
	}

	rc = srv_delfilter_process(&netobuf);
	if (rc) {
		reject();
		return;
	}

	rc = db_delete_filter(deletingfilterid);
	if (rc) {
		reject();
		return;
	}

	//if successful, the server will have increased the filterversion by one
	rc = db_select_sync(&dbsynclist[dbindex]);
	if (rc) {
		reject();
		return;
	}
	dbsynclist[dbindex].filterversion += 1;
	rc = db_update_sync(&dbsynclist[dbindex]);
	if (rc) {
		reject();
		return;
	}

	ui.statusLabel->clear();
	listFilters();
}

void QtSyncDialog::shareDeleteReceived(int rc) {
	std::list<mc_share> list;
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(shareDeleteReceived(int)));
	if (rc) {
		reject();
		return;
	}

	rc = srv_delshare_process(&netobuf);
	if (rc) {
		reject();
		return;
	}

	rc = db_delete_share(&deletingshare);
	if (rc) {
		reject();
		return;
	}

	//if successful, the server will have increased the shareversion by one
	rc = db_select_sync(&dbsynclist[dbindex]);
	if (rc) {
		reject();
		return;
	}
	dbsynclist[dbindex].shareversion += 1;
	rc = db_update_sync(&dbsynclist[dbindex]);
	if (rc) {
		reject();
		return;
	}

	ui.statusLabel->clear();
	listShares();
}

void QtSyncDialog::syncDeleteReceived(int rc) {
	const int sid = srvsynclist[ui.nameBox->currentIndex()].id;
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(syncDeleteReceived(int)));
	if (rc) {
		reject();
		return;
	}

	rc = srv_delsync_process(&netobuf);
	if (rc) {
		reject();
		return;
	}

	ui.statusLabel->setText(tr("<i>deleting locally...</i>"));
	QApplication::processEvents();


	std::list<mc_file> l;
	std::list<mc_file>::iterator lit, lend;
	rc = db_list_file_parent(&l, -sid);
	if (rc) {
		reject();
		return;
	}
	lit = l.begin();
	lend = l.end();
	rc = db_execstr("BEGIN TRANSACTION");
	if (rc) {
		reject();
		return;
	}

	while (lit != lend) {
		QApplication::processEvents();
		rc = purge(&*lit, NULL);
		if (rc) {
			reject();
			return;
		}
		++lit;
	}
	rc = db_execstr("COMMIT");
	if (rc) {
		reject();
		return;
	}
	rc = db_delete_filter_sid(sid);
	if (rc) {
		reject();
		return;
	}
	rc = db_delete_share_sid(sid);
	if (rc) {
		reject();
		return;
	}

	rc = db_delete_sync(sid);
	if (rc) {
		reject();
		return;
	}


	ui.statusLabel->clear();
	startOver();
}

void QtSyncDialog::userReceived(int rc) {	
	std::list<mc_user> list;
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(userReceived(int)));
	if (rc) {
		reject();
		return;
	}
	
	rc = srv_idusers_process(&netobuf, &list);
	if (rc) {
		reject();
		return;
	}

	if (list.size() != 1) { // owner doesn't exist or exist multiple times?!
		MC_WRN("Failed to identify owner, probably a server reset");
		reject();
		return;
	}
	worksyncowner = list.front();

	rc = db_insert_user(&worksyncowner);	
	if (rc) {
		reject();
		return;
	}

	accept_step2();
}

void QtSyncDialog::keyringReceived_actual(int rc) {
	string keyringdata;
	if (rc) {
		reject();
		return;
	}

	rc = srv_getkeyring_process(&netobuf, &keyringdata);
	if (rc) {
		reject();
		return;
	}


	if (keyringdata.length() > 0) { // don't ask for a password when there is no ring...
		ui.statusLabel->setText(tr("<i>decrypting...</i>"));
		QString pass;

		if (keyringpass == "") {
			bool ok = false;
			pass = QInputDialog::getText(this, tr("Keyring Password"), 
				tr("Please enter the password to your keyring"), QLineEdit::Password, NULL, &ok, windowFlags() & ~Qt::WindowContextHelpButtonHint);

			if (!ok) {
				return;
			}
		} else {
			pass = keyringpass;
		}

		// decrypt
		rc = crypt_keyring_fromsrv(keyringdata, pass.toStdString(), &keyring);
		if (rc) {
			QMessageBox::critical(this, tr("Keyring Decryption failed"), 
				tr("The keyring could not be decrypted! Re-check your password or enter the key manually."));
			return;
		}
		keyringpass = pass;

	}
	
	keyringloaded = true;
}



void QtSyncDialog::keyringReceivedLooking(int rc) {
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(keyringReceivedLooking(int)));
	
	keyringReceived_actual(rc);

	if (keyringloaded) {

		// find
		for (mc_keyringentry& entry : keyring) {
			if (entry.sname == worksync->name && entry.uname == worksyncowner.name) {
				memcpy(worksync->cryptkey, entry.key, 32);
				accept_step3();
				return;

			}
		}
				
		QMessageBox::information(this, tr("Key not found"), 
			tr("No key for this Sync was found in the keyring. You need to enter it manually. If the Sync is shared, the owner (") +
			worksyncowner.name.c_str() + tr(") can give you the key."));
	
	}

	// getting the ring failed in some way or no key found, _actual might have rejected already
	ui.keyEdit->clear();
	ui.statusLabel->clear();
	ui.okButton->setEnabled(true);

}

void QtSyncDialog::keyringReceivedAdding(int rc) {
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(keyringReceivedAdding(int)));

	keyringReceived_actual(rc);

	if (keyringloaded) {

		bool found = false;
		bool needsend = true;
		for (mc_keyringentry& entry : keyring) {
			if (entry.sname == worksync->name && entry.uname == worksyncowner.name) {
				if (memcmp(entry.key, worksync->cryptkey, 32) == 0) { //exact match
					needsend = false;
				}
			}
		}

		if (needsend) {
			ui.statusLabel->setText(tr("<i>encrypting...</i>"));
			if (keyringpass == "") {
				bool ok = false;
				QString pass, confirm;
				pass = QInputDialog::getText(this, tr("Keyring Password"), 
					tr("Please choose a password for your keyring.\nIt is used to encrypt the keyring and should be as secure as possible, "
						"especially not related to your account password!\nMake sure you do not forget it!"), 
					QLineEdit::Password, NULL, &ok, windowFlags() & ~Qt::WindowContextHelpButtonHint);
				if (ok) confirm = QInputDialog::getText(this, tr("Keyring Password"), 
					tr("Please confirm your keyring password"), QLineEdit::Password, NULL, &ok, windowFlags() & ~Qt::WindowContextHelpButtonHint);
				if (ok && pass != confirm) {
					QMessageBox::warning(this, tr("Password Mismatch"), 
						tr("The passwords didn't match. Try again"));
					ok = false;
				}
				if (ok && pass.length() < 10) {
					QMessageBox::warning(this, tr("Insecure Password"), 
						tr("This is the key to the keys to all your files!\nI can't force you to use a secure password, but..."));
					ok = false;
				}
				if (ok)
					keyringpass = pass;

				if (keyringpass == "") {
					ui.keyEdit->setText(QByteArray::fromRawData((const char*)worksync->cryptkey, 32).toHex().toUpper());
					ui.okButton->setEnabled(true);
					return;
				}
			}

			bool found = false;
			for (mc_keyringentry& entry : keyring) {
				if (entry.sname == worksync->name && entry.uname == worksyncowner.name) {
					memcpy(entry.key, worksync->cryptkey, 32);
					found = true;
				}
			}
			if (!found) {
				mc_keyringentry newentry;
				newentry.sname = worksync->name;
				newentry.uname = worksyncowner.name;
				memcpy(newentry.key, worksync->cryptkey, 32);
				keyring.push_back(newentry);
			}

			string keyringdata;
			rc = crypt_keyring_tosrv(&keyring, keyringpass.toStdString(), &keyringdata);
			if (rc) {
				reject();
				return;
			}

			ui.statusLabel->setText(tr("<i>uploading keyring...</i>"));
			connect(performer, SIGNAL(finished(int)), this, SLOT(keyringSent(int)));
			srv_setkeyring_async(&netibuf, &netobuf, performer, &keyringdata);
		} else {
			accept_step3();
		}
		
	} else {
		// getting the ring failed in some way, _actual might have rejected already
		ui.keyEdit->setText(QByteArray::fromRawData((const char*)worksync->cryptkey, 32).toHex().toUpper());
		ui.okButton->setEnabled(true);
	}	
}

void QtSyncDialog::keyringSent(int rc) {
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(keyringSent(int)));
	if (rc) {
		reject();
		return;
	}

	rc = srv_setkeyring_process(&netobuf);
	if (rc) {
		reject();
		return;
	}
	
	accept_step3();
}

void QtSyncDialog::accept() {
	int rc;
	int maxprio=0;

	if (ui.pathEdit->text().length() == 0) {
		QMessageBox::warning(this, tr("No path specified"), 
			tr("Please specify where the files are to be synced."));
		return;
	}

	if (dbindex == -1) { //new sync

		for (mc_sync_db s : dbsynclist) {
			if (s.priority > maxprio) maxprio = s.priority;
		}

		newsync.priority = maxprio+1;
		newsync.filterversion = 0;
		newsync.shareversion = 0;
		memset(newsync.hash, 0, 16);
		newsync.lastsync = 0;
		newsync.id = srvsynclist[ui.nameBox->currentIndex()].id;
		newsync.uid = srvsynclist[ui.nameBox->currentIndex()].uid;
		newsync.name = srvsynclist[ui.nameBox->currentIndex()].name;
		newsync.path = "";

		worksync = &newsync;

	} else {
		worksync = &dbsynclist[dbindex];
		rc = db_select_sync(worksync);
		if (rc) {
			reject();
			return;
		}
	}

	string newpath = qPrintable(ui.pathEdit->text().replace("\\", "/"));
	if (newpath[newpath.length()-1] != '/') newpath.append("/");
	if (worksync->path != "" && newpath != worksync->path) {
		QMessageBox::warning(this, tr("Moving Syncs"), 
			tr("Be sure to move the files to the new path as well, \notherwise the system will assume the files were deleted."));
	}
	worksync->path = newpath;

	worksync->crypted = srvsynclist[ui.nameBox->currentIndex()].crypted;

	// Make sure we know the owner
	worksyncowner.id = srvsynclist[ui.nameBox->currentIndex()].uid;
	rc = db_select_user(&worksyncowner);
	if (rc == SQLITE_DONE) {
		ui.statusLabel->setText(tr("<i>fetching user...</i>"));
		connect(performer, SIGNAL(finished(int)), this, SLOT(userReceived(int)));
		list<int> l;
		l.push_back(worksyncowner.id);
		srv_idusers_async(&netibuf, &netobuf, performer, l);
		//calls step2
	} else if (rc) { 
		reject(); 
		return; 
	} else {
		accept_step2();
	}
}

void QtSyncDialog::accept_step2() {
	if (worksync->crypted) {
		if (ui.keyEdit->text() != "") {
			QRegExp hexMatcher("^[0-9A-F]{64}$", Qt::CaseInsensitive);
			if (hexMatcher.exactMatch(ui.keyEdit->text())) {
				QByteArray ckey = QByteArray::fromHex(ui.keyEdit->text().toLatin1());
				if (memcmp(worksync->cryptkey, ckey.constData(), 32) != 0) {
					memcpy(worksync->cryptkey, ckey.constData(), 32);
					keychanged = true;
				}

				if (keychanged) {				
					QMessageBox b(this);
					b.setText(tr("Keyring"));
					b.setInformativeText(tr("This key might not be in the keyring yet. Do you want to check and add/update it?"));
					b.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
					b.setDefaultButton(QMessageBox::Yes);
					b.setIcon(QMessageBox::Question);
					if (b.exec() == QMessageBox::Yes) {
						ui.statusLabel->setText(tr("<i>downloading keyring...</i>"));
						connect(performer, SIGNAL(finished(int)), this, SLOT(keyringReceivedAdding(int)));
						srv_getkeyring_async(&netibuf, &netobuf, performer);
						ui.okButton->setEnabled(false);

						return;
					}
				}
				// not changed or doesn't want to upload
				accept_step3();
			} else {
				QMessageBox::warning(this, tr("Please enter a valid 256-bit key in hex format"), 
					tr("Alternatively, you can leave the field empty to generate a new key, if you don't have one yet."));
				return;
			}
		} else {
			// fetch keyring and check
			// will call step2
			ui.statusLabel->setText(tr("<i>downloading keyring...</i>"));
			connect(performer, SIGNAL(finished(int)), this, SLOT(keyringReceivedLooking(int)));
			srv_getkeyring_async(&netibuf, &netobuf, performer);
			ui.okButton->setEnabled(false);
		}
	} else {
		memset(worksync->cryptkey, 0, 32);
		accept_step3();
	}
}

void QtSyncDialog::accept_step3() {
	int rc;

	if (dbindex == -1) {
		newsync.status = MC_SYNCSTAT_UNKOWN;
		rc = db_insert_sync(&newsync);
		if (rc) {
			reject();
			return;
		}
	} else {
		rc = db_update_sync(worksync);
		if (rc) {
			reject();
			return;
		}
	}
	QDialog::accept();
}

void QtSyncDialog::reject() {
	QDialog::reject();
}

void QtSyncDialog::on_deleteSyncButton_clicked() {
	const int sid = srvsynclist[ui.nameBox->currentIndex()].id;
	bool cando = true;
	QMessageBox b(this);
	b.setText(tr("Are you sure you want to delete the Sync?"));
	b.setInformativeText(tr("It will be entirely deleted on the Server! Hit OK to delete."));
	b.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	b.setDefaultButton(QMessageBox::Ok);
	b.setIcon(QMessageBox::Warning);
	if (b.exec() == QMessageBox::Ok) {
		if (QtWorkerThread::instance()->isRunning()) {
			QMessageBox b2(this);
			b2.setText(tr("Worker stop required"));
			b2.setInformativeText(tr("Deleting the sync requires the worker to be stopped."));
			b2.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
			b2.setDefaultButton(QMessageBox::Ok);
			b2.setIcon(QMessageBox::Warning);
			if (b2.exec() == QMessageBox::Ok) {
				if (QtWorkerThread::instance()->isRunning()) {
					MC_INF("Stopping Worker");
					QtWorkerThread::instance()->quit();
				}
			} else {
				cando = false;
			}
		}
		if (cando) {
			ui.statusLabel->setText(tr("<i>sending request...</i>"));
			disableAll();
			connect(performer, SIGNAL(finished(int)), this, SLOT(syncDeleteReceived(int)));
			srv_delsync_async(&netibuf, &netobuf, performer, sid);
		}
	}
}

void QtSyncDialog::on_browseButton_clicked() {
	QFileDialog d(this);
	d.setFileMode(QFileDialog::Directory);
	d.setNameFilter(tr("Directories"));
	d.setOption(QFileDialog::ShowDirsOnly, true);
	if (d.exec()) {
		ui.pathEdit->setText(d.selectedFiles()[0]);
	};
}

void QtSyncDialog::on_nameBox_currentIndexChanged(int index) {
	if (loadcompleted) {
		// Cancel any running requests as the scope now changed
		performer->abort();
		disconnect(performer, SIGNAL(finished(int)));

		if (index == ui.nameBox->count()-1) {
			QtNewSyncDialog d(this, performer, &netibuf, &netobuf);
			int code = d.exec();
			if (code == DialogCode::Accepted) {
				syncID = d.newSyncID();
				startOver();
			}
		} else {
			ui.deleteSyncButton->setEnabled(srvsynclist[index].uid == myUID);
			ui.keyEdit->setEnabled(srvsynclist[index].crypted);
			filldbdata();
		}
	}
}

void QtSyncDialog::on_addFilterButton_clicked() {
	int rc;
	QtFilterDialog d(this, performer, &netibuf, &netobuf, dbsynclist[dbindex].id);
	d.exec();

	//filterversion updated
	rc = db_select_sync(&dbsynclist[dbindex]);
	if (rc) {
		reject();
		return;
	}

	listFilters();
	ui.filterTable->selectRow(ui.filterTable->rowCount()-1);
}

void QtSyncDialog::on_removeFilterButton_clicked() {
	const int index = ui.filterTable->selectedItems().at(0)->row();
	ui.filterTable->setEnabled(false);
	ui.addFilterButton->setEnabled(false);
	ui.removeFilterButton->setEnabled(false);
	ui.editFilterButton->setEnabled(false);
	ui.statusLabel->setText(tr("<i>sending request...</i>"));
	deletingfilterid = filterlist[index].id;
	connect(performer, SIGNAL(finished(int)), this, SLOT(filterDeleteReceived(int)));
	srv_delfilter_async(&netibuf, &netobuf, performer, &filterlist[index]);
}

void QtSyncDialog::on_editFilterButton_clicked() {
	int rc;
	const int index = ui.filterTable->selectedItems().at(0)->row();
	QtFilterDialog d(this, performer, &netibuf, &netobuf, dbsynclist[dbindex].id, filterlist[index].id);
	d.exec();
	
	//filterversion updated
	rc = db_select_sync(&dbsynclist[dbindex]);
	if (rc) {
		reject();
		return;
	}
	listFilters();
	ui.filterTable->selectRow(index);
}

void QtSyncDialog::on_addShareButton_clicked() {
	int rc;
	QtShareDialog d(this, performer, &netibuf, &netobuf, dbsynclist[dbindex].id, myUID);
	d.exec();

	//Shareversion updated
	rc = db_select_sync(&dbsynclist[dbindex]);
	if (rc) {
		reject();
		return;
	}

	listShares();
}

void QtSyncDialog::on_removeShareButton_clicked() {
	const int index = ui.shareList->currentRow();
	ui.shareList->setEnabled(false);
	ui.addShareButton->setEnabled(false);
	ui.removeShareButton->setEnabled(false);
	ui.statusLabel->setText(tr("<i>sending request...</i>"));
	deletingshare.sid = sharelist[index].sid;
	deletingshare.uid = sharelist[index].uid;
	connect(performer, SIGNAL(finished(int)), this, SLOT(shareDeleteReceived(int)));
	srv_delshare_async(&netibuf, &netobuf, performer, &sharelist[index]);
}

void QtSyncDialog::on_filterTable_itemSelectionChanged() {
	if (ui.filterTable->selectedItems().length() == 0) {
		ui.removeFilterButton->setEnabled(false);
		ui.editFilterButton->setEnabled(false);
	} else {
		if (dbsynclist[dbindex].uid == myUID) {
			ui.removeFilterButton->setEnabled(true);
			ui.editFilterButton->setEnabled(true);
		}
	}
}

void QtSyncDialog::on_shareList_itemSelectionChanged() {
	if (ui.shareList->selectedItems().length() == 0) {
		ui.removeShareButton->setEnabled(false);
	} else {
		if (dbsynclist[dbindex].uid == myUID) {
			ui.removeShareButton->setEnabled(true);
		}
	}
}

void QtSyncDialog::on_globalFilterButton_clicked() {
	bool refresh = true;
	//check wether a refresh from server is needed
	for (mc_sync_db& d : dbsynclist) {
		for (mc_sync& s : srvsynclist) {
			//If a single sync is up-to-date the globals must be too
			if (d.id == s.id && d.filterversion >= s.filterversion) {
				refresh = false; 
				break;
			}
		}
		if (!refresh) break;
	}
	QtGeneralFilterDialog d(this, performer, &netibuf, &netobuf, refresh);
	d.exec();
}

void QtSyncDialog::filldbdata() {
	int i;
	const int sid = srvsynclist[ui.nameBox->currentIndex()].id;
	if (loadcompleted) {
		i = 0;		
		
		//disable and clear stuff, when found it will be filled and enabled
		dbindex = -1; // = there is no matching db entry
		keychanged = false;
		ui.pathEdit->clear();
		ui.keyEdit->clear();
		ui.filterTable->clearContents();
		ui.filterTable->setRowCount(0);
		ui.filterTable->setEnabled(false);
		ui.addFilterButton->setEnabled(false);
		ui.removeFilterButton->setEnabled(false);
		ui.editFilterButton->setEnabled(false);
		ui.shareList->clear();
		ui.shareList->setEnabled(false);
		ui.addShareButton->setEnabled(false);
		ui.removeShareButton->setEnabled(false);
		ui.addShareButton->setVisible(true);
		ui.removeShareButton->setVisible(true);
		ui.ownerLabel->setVisible(false);
		
		for (mc_sync_db& s : dbsynclist) {
			if (s.id == sid) {
				dbindex = i;
				//Path, Key
				ui.pathEdit->setText(s.path.c_str());
				if (s.crypted) {
					ui.keyEdit->setText(QByteArray((const char*)s.cryptkey, 32).toHex().toUpper());
				} else {
					ui.keyEdit->clear();
				}
				//Filters
				listFilters();
				//Shares
				//listShares(); //called from listFilters
				return;
			}
			i++;
		}

		//none found
		ui.needSubscribeLabel->setVisible(true);
	}
}

void QtSyncDialog::listFilters() {
	std::list<mc_filter> fl;
	
	if (dbsynclist[dbindex].filterversion < srvsynclist[ui.nameBox->currentIndex()].filterversion) {		
		ui.filterTable->setEnabled(false);
		ui.addFilterButton->setEnabled(false);
		ui.fetchFilterLabel->setVisible(true);
		connect(performer, SIGNAL(finished(int)), this, SLOT(globalFilterListReceived(int)));
		srv_listfilters_async(&netibuf, &netobuf, performer, 0); //global, will then call sync-specific
	} else {
		listFilters_actual();
	}
}

void QtSyncDialog::listFilters_actual() {
	std::list<mc_filter> fl;
	int rc;
	rc = db_list_filter_sid(&fl, dbsynclist[dbindex].id);
	if (rc) return;
	filterlist.assign(fl.begin(), fl.end());
	ui.filterTable->clearContents();
	ui.filterTable->setRowCount(0);
	for (mc_filter& f : filterlist) {
		ui.filterTable->insertRow(ui.filterTable->rowCount());
		if (f.files) ui.filterTable->setItem(ui.filterTable->rowCount()-1, 0, new QTableWidgetItem(file, ""));
		if (f.directories) ui.filterTable->setItem(ui.filterTable->rowCount()-1, 1, new QTableWidgetItem(directory, ""));
		switch(f.type) {
			case MC_FILTERT_MATCH_NAME:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Name")));
				break;
			case MC_FILTERT_MATCH_EXTENSION:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Extension")));
				break;
			case MC_FILTERT_MATCH_FULLNAME:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Full Name")));
				break;
			case MC_FILTERT_MATCH_PATH:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Path")));
				break;
			case MC_FILTERT_REGEX_NAME:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Name (regex)")));
				break;
			case MC_FILTERT_REGEX_EXTENSION:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Extension (regex)")));
				break;
			case MC_FILTERT_REGEX_FULLNAME:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Full Name (regex)")));
				break;
			case MC_FILTERT_REGEX_PATH:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Path (regex)")));
				break;
			default:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Unrecognized")));
		}
					
		ui.filterTable->setItem(ui.filterTable->rowCount()-1, 3, new QTableWidgetItem(f.rule.c_str()));
	}

	ui.filterTable->setEnabled(true);
	if (dbsynclist[dbindex].uid == myUID) {
		ui.addFilterButton->setEnabled(true);
	}
	ui.needSubscribeLabel->setVisible(false);
	//chained listShares (because we can't handle parallel network requests)
	listShares();
}


void QtSyncDialog::listShares() {
	std::list<mc_filter> fl;
	
	if (dbsynclist[dbindex].shareversion < srvsynclist[ui.nameBox->currentIndex()].shareversion) {		
		ui.shareList->setEnabled(false);
		ui.addShareButton->setEnabled(false);
		ui.fetchShareLabel->setVisible(true);
		connect(performer, SIGNAL(finished(int)), this, SLOT(shareListReceived(int)));
		srv_listshares_async(&netibuf, &netobuf, performer, dbsynclist[dbindex].id);
	} else {
		listShares_actual();
	}
}

void QtSyncDialog::listShares_actual() {
	std::list<mc_share> sl;
	mc_user u;
	int rc;
	rc = db_list_share_sid(&sl, dbsynclist[dbindex].id);
	if (rc) return;
	sharelist.assign(sl.begin(), sl.end());
	ui.shareList->clear();
	for (mc_share& s : sharelist) {
		if (s.uid == myUID) {
			ui.shareList->addItem(new QListWidgetItem(user, tr("me")));
		} else {
			u.id = s.uid;
			rc = db_select_user(&u);
			if (rc == SQLITE_DONE) u.name = "(unkown)";
			else if (rc) return;
			ui.shareList->addItem(new QListWidgetItem(user, QString::fromStdString(u.name)));
		}
	}

	// owner
	u.id = dbsynclist[dbindex].uid;
	rc = db_select_user(&u);
	if (rc == SQLITE_DONE) u.name = "(unkown)";
	else if (rc) return;
	
	ui.shareList->setEnabled(true);
	if (dbsynclist[dbindex].uid == myUID) {
		ui.addShareButton->setEnabled(true);
		ui.addShareButton->setVisible(true);
		ui.removeShareButton->setVisible(true);
		ui.ownerLabel->setVisible(false);
	} else  {
		ui.addShareButton->setVisible(false);
		ui.removeShareButton->setVisible(false);
		ui.ownerLabel->setVisible(true);
		ui.ownerLabel->setText(tr("Owner: ")+QString::fromStdString(u.name));
	}
}

void QtSyncDialog::startOver() {
	loadcompleted = false;
	ui.fetchSyncLabel->setVisible(true);
	ui.nameBox->clear();
	ui.pathEdit->clear();
	ui.keyEdit->clear();
	ui.filterTable->clearContents();
	ui.shareList->clear();
	disableAll();
	connect(performer, SIGNAL(finished(int)), this, SLOT(syncListReceived(int)));
	srv_listsyncs_async(&netibuf, &netobuf, performer);
}

void QtSyncDialog::disableAll() {
	ui.nameBox->setEnabled(false);
	ui.deleteSyncButton->setEnabled(false);
	ui.pathEdit->setEnabled(false);
	ui.browseButton->setEnabled(false);
	ui.keyEdit->setEnabled(false);
	ui.okButton->setEnabled(false);
	ui.globalFilterButton->setEnabled(false); 
	ui.addFilterButton->setEnabled(false);
	ui.editFilterButton->setEnabled(false);
	ui.removeFilterButton->setEnabled(false);
	ui.filterTable->setEnabled(false);
	ui.needSubscribeLabel->setVisible(false);
	ui.shareList->setEnabled(false);
	ui.addShareButton->setEnabled(false);
	ui.removeShareButton->setEnabled(false);
}