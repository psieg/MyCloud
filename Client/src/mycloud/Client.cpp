
#include <iostream>
#include "mc.h"
#include "mc_db.h"
#include "mc_fs.h"
#include "mc_srv.h"
#include "mc_walk.h"
#include "mc_watch.h"
#include "mc_helpers.h"
#include <regex>
#ifdef MC_QTCLIENT
#	include "qtClient.h"
#else
#	include "mc_workerthread.h"
#endif


using namespace std;
#define CAFILE "trustCA.crt"

ofstream mc_logfile;

int recoverReset(int olduid, int newuid) {
	int rc;
	MC_WRN("Server has newer basedate, resetting");
	rc = db_execstr("DELETE FROM files");
	if (rc) throw rc;
	rc = db_execstr("UPDATE syncs SET filterversion = 0");
	if (rc) throw rc;
	rc = db_execstr("DELETE FROM filters");
	if (rc) throw rc;
	//keep old users and syncs in memory and try to identify them in the new lists
	std::list<mc_user> dbusers, srvusers;
	std::list<mc_sync> srvsyncs;
	std::list<mc_sync_db> dbsyncs;
	rc = db_list_user(&dbusers);
	if (rc) throw rc;
	rc = db_execstr("DELETE FROM users");
	if (rc) throw rc;
	rc = db_list_sync(&dbsyncs);
	if (rc) throw rc;
	rc = db_execstr("DELETE FROM syncs");
	if (rc) throw rc;

	rc = srv_listsyncs(&srvsyncs);								
	if (rc) throw rc;

	list<int> newids;
	for (mc_sync& srv : srvsyncs) {
		bool found = false;
		for (int id : newids) {
			if (id == srv.uid) {
				found = true;
				break;
			}
		}
		if (!found)
			newids.push_back(srv.uid);
	}
	rc = srv_idusers(newids, &srvusers);
	if (rc) throw rc;

	int recovered = 0;
	int notfound = 0;
	for (mc_sync_db& db : dbsyncs) {
		bool found = false;
		for (mc_sync& srv : srvsyncs) {
			if (db.name == srv.name && db.crypted == srv.crypted) {
				// find old and new user
				mc_user *olduser = NULL;
				mc_user *newuser = NULL;
				for (mc_user& user : dbusers) {
					if (user.id == db.uid) {
						olduser = &user;
						break;
					}
				}
				for (mc_user& user : srvusers) {
					if (user.id == srv.uid) {
						newuser = &user;
						break;
					}
				}
				if (olduser && newuser && olduser->name == newuser->name) {
					db.id = srv.id;
					db.uid = srv.uid;
					db.filterversion = 0;
					memset(db.hash, 0, 16);
					db.status = MC_SYNCSTAT_UNKOWN;
					rc = db_insert_sync(&db);
					if (rc) throw rc;
					found = true;
					MC_INF("Recovered Sync " << db.name << " with new id " << srv.id);
					recovered++;
					break;
				}
			}
		}
		if (!found) {
			MC_WRN("Failed to find Sync " << db.id << ": " << db.name << " on Server");
			notfound++;
		}
	}
	stringstream s; s << recovered << " / " << recovered+notfound;
	MC_NOTIFY(MC_NT_SERVERRESET, s.str());
	return 0;
}

int runmc()
{
	int rc, wrc, uid;
	int64 basedate;
	int64 lastnosynccheck = 0;
	mc_status status;
	unsigned char hash[16];
	
	mc_logfile << "Begin run at " << TimeToString(time(NULL)) << endl;
	//rc = db_open("state.db");
	rc = 0;
	if (!rc) {
		rc = db_select_status(&status);
		if (!rc) {
			while (true) {
				if (status.url == "") { //TODO: ask
					MC_ERR_MSG(MC_ERR_NOT_CONFIGURED, "No server configured");
				}
				if (status.url.substr(0, strlen(MC_UNTRUSTEDPREFIX)) == MC_UNTRUSTEDPREFIX) { //If a previous run failed, don't try
					MC_NOTIFY(MC_NT_CRYPTOFAIL, "");
					MC_ERR_MSG(MC_ERR_CRYPTOALERT, "Untrusted server, aborting");
				}
				rc = srv_open(status.url, CAFILE, status.uname, status.passwd, &basedate, &uid, status.acceptallcerts);
				if (!rc) {
					try {
						MC_NOTIFYSTART(MC_NT_CONN, status.url);
						if (status.basedate != basedate) {
							bool cancel = false;
							if (status.basedate != 0) {
								recoverReset(status.uid, uid);
								cancel = true;
							}
							status.basedate = basedate;
							status.uid = uid;
							rc = db_update_status(&status);
							if (rc) throw rc;
							if (cancel) return 0; //RecoverReset has notified the user
						}
						status.lastconn = time(NULL);
						rc = db_update_status(&status);
						if (rc) throw rc;
#ifdef MC_WATCHMODE					
						QtWatcher watcher(status.url.c_str(), CAFILE, status.acceptallcerts);
#endif

						//TODO: resume and stuff
						while (true) {
							mc_sync_ctx context;
							list<mc_filter> generalfilter, filter;
							//rc = update_filters(0);
							//if (rc) throw rc;
							rc = db_list_filter_sid(&generalfilter, 0);
							if (rc) throw rc;

							//Get list of syncs and compare to local
							list<mc_sync> srvsyncs;// = list<mc_sync>();
							list<mc_sync>::iterator srvsyncsit, srvsyncsend;
							list<mc_sync_db> dbsyncs;// = list<mc_sync_db>();
							list<mc_sync_db>::iterator dbsyncsit, dbsyncsend;
							rc = srv_listsyncs(&srvsyncs);
							if (rc) throw rc;

							rc = db_list_sync(&dbsyncs);
							if (rc) throw rc;
							if (dbsyncs.size() == 0) {
								MC_ERR_MSG(MC_ERR_NOT_CONFIGURED, "No syncs configured");
							}
							MC_INFL("Beginning run");

							dbsyncs.sort(compare_mc_sync_db_prio);

#ifdef MC_WATCHMODE
							watcher.setScope(dbsyncs);
#endif

							dbsyncsit = dbsyncs.begin();
							dbsyncsend = dbsyncs.end();
							srvsyncsend = srvsyncs.end();
							for (;dbsyncsit != dbsyncsend; ++dbsyncsit) { //Foreach db sync
								if (MC_TERMINATING()) break;
								if (dbsyncsit->status == MC_SYNCSTAT_DISABLED || dbsyncsit->status == MC_SYNCSTAT_CRYPTOFAIL) continue;

								srvsyncsit = srvsyncs.begin();
								while ((srvsyncsit != srvsyncsend) && (dbsyncsit->id != srvsyncsit->id)) ++srvsyncsit;
								if ((srvsyncsit == srvsyncsend) || (dbsyncsit->id != srvsyncsit->id)) {
									MC_WRN(dbsyncsit->name << " is not available on server");
									dbsyncsit->status = MC_SYNCSTAT_UNAVAILABLE;
									rc = db_update_sync(&*dbsyncsit);
									MC_CHKERR(rc);
								} else {
									init_sync_ctx(&context, &*dbsyncsit, &filter);
									if (dbsyncsit->filterversion < srvsyncsit->filterversion) {
										list<mc_filter> fl;
										//updates of sid-0-filters count as update to all syncs
										//this will cause multiple download but hopefully happens rarely enough
										//and is better than adding a fake-sync-0
										rc = srv_listfilters(&fl, 0);
										if (rc) throw rc;
										//general filters are not encrypted
										rc = update_filters(0, &fl);
										if (rc) throw rc;
										generalfilter.clear();
										rc = db_list_filter_sid(&generalfilter, 0);
										if (rc) throw rc;

										fl.clear();
										rc = srv_listfilters(&fl, dbsyncsit->id);
										if (rc) throw rc;
										rc = crypt_filterlist_fromsrv(&context, dbsyncsit->name, &fl);
										if (rc) throw rc;
										rc = update_filters(dbsyncsit->id, &fl);
										if (rc) throw rc;
										dbsyncsit->filterversion = srvsyncsit->filterversion;
										rc = db_update_sync(&*dbsyncsit);
										if (rc) throw rc;
									}
									filter.assign(generalfilter.begin(), generalfilter.end());
									rc = db_list_filter_sid(&filter, dbsyncsit->id);
									if (rc) throw rc;
									dbsyncsit->status = MC_SYNCSTAT_RUNNING;
									rc = db_update_sync(&*dbsyncsit);
									if (rc) throw rc;
									MC_NOTIFYSTART(MC_NT_SYNC, dbsyncsit->name);
									if (memcmp(dbsyncsit->hash, srvsyncsit->hash, 16) == 0) {
										MC_INFL(dbsyncsit->name << " has not been updated on server, walking locally");
										wrc = walk_nochange(&context, "", -dbsyncsit->id, hash);
										//MC_INFL("Sync completed (Code " << wrc << ")");
									} else {
										MC_INFL(dbsyncsit->name << " has been updated on server, walking");
										wrc = walk(&context, "", -dbsyncsit->id, hash);
										//MC_INF("Sync completed (Code " << wrc << ")");
									}
									rc = db_select_sync(&*dbsyncsit);
									if (rc) throw rc;
									memcpy(dbsyncsit->hash, hash, 16);
									if (wrc == MC_ERR_TERMINATING) dbsyncsit->status = MC_SYNCSTAT_ABORTED;
									else if (wrc == MC_ERR_CRYPTOALERT) dbsyncsit->status = MC_SYNCSTAT_CRYPTOFAIL; 
									else if (MC_IS_CRITICAL_ERR(wrc)) dbsyncsit->status = MC_SYNCSTAT_FAILED;
									else if (memcmp(dbsyncsit->hash, srvsyncsit->hash, 16) == 0) dbsyncsit->status = MC_SYNCSTAT_SYNCED;
									else dbsyncsit->status = MC_SYNCSTAT_COMPLETED;
									dbsyncsit->lastsync = time(NULL);
									rc = db_update_sync(&*dbsyncsit);
									if (rc) throw rc;
									if (wrc == MC_ERR_CRYPTOALERT) 
										throw MC_ERR_CRYPTOALERT;
									MC_NOTIFYEND(MC_NT_SYNC);
								}
							}

							if (MC_TERMINATING()) break;

							//Check for sync
							rc = fullsync(&dbsyncs);
							if (!rc) {
								MC_INFL("Run completed, fully synced");
								MC_NOTIFY(MC_NT_FULLSYNC, TimeToString(time(NULL)));
							} else if (rc == MC_ERR_NOTFULLYSYNCED) {
								MC_INFL("Run completed, not synced, running again");
								MC_NOTIFYEND(MC_NT_SYNC); //Trigger ListSyncs
								continue;
							} else if (rc == MC_ERR_NETWORK) {
								MC_NOTIFY(MC_NT_ERROR, "Network Error");						
								if (status.watchmode > 0)
									mc_sleep_checkterminate(status.watchmode);
								else
									mc_sleep_checkterminate(-status.watchmode);
								continue;
							}
							srv_standby();

							if (MC_TERMINATING()) break;

							if (QtWorkerThread::instance()->runSingle)
								break;

	#ifdef MC_WATCHMODE
							if (status.watchmode > 0)
								enter_watchmode(status.watchmode);
								watcher.catchUpAndWatch(status.watchmode);
							else
								mc_sleep_checkterminate(-status.watchmode);
	#else
							mc_sleep_checkterminate(-status.watchmode);
	#endif

							if (MC_TERMINATING()) break;

						}
					} catch (int i) {
						switch(i) {
							case MC_ERR_TERMINATING:
								break;
							case MC_ERR_PROTOCOL:
								cerr << "Error while processing protocol. Aborting" << endl;
								MC_NOTIFY(MC_NT_ERROR, "Protocol Error");
								break;
							case MC_ERR_CRYPTOALERT:
								return cryptopanic();
							default:
								cerr << "Internal error: " << i << " Aborting." << endl;
								MC_NOTIFY(MC_NT_ERROR, "Internal error");
						}
					}
					MC_NOTIFYEND(MC_NT_CONN);
				} else {
					cerr << "Error while connecting" << endl;
					MC_NOTIFY(MC_NT_ERROR, "Failed to connect to server");

					// No-sync warning
					if (time(NULL) - lastnosynccheck > MC_NOSYNCCHECK) { //only warn every x seconds
						lastnosynccheck = time(NULL);
						rc = db_select_status(&status);
						if (rc) return rc;
						if (status.lastconn > 0 && time(NULL) - status.lastconn > MC_NOSYNCWARN) {
							MC_WRN("No server connection in the last 24 hours!");
							MC_NOTIFY(MC_NT_NOSYNCWARN, TimeToString(status.lastconn));
						}
					}
				}
				srv_close();
				if (MC_TERMINATING()) break;

				if (QtWorkerThread::instance()->runSingle)
					break;

				//Reach here on longer connection fail
				//MC_INF("Retrying in 60 sec");
				mc_sleep_checkterminate(60);
			}
		} else {
			cerr << "Database Error" << endl;
			MC_NOTIFY(MC_NT_ERROR, "Failed to open database");
		}
	}
	//db_close();

	mc_logfile << "End run at " << TimeToString(time(NULL)) << endl;

	return 0;
}

