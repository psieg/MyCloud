// Client.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include "mc.h"
#include "mc_db.h"
#include "mc_fs.h"
#include "mc_srv.h"
#include "mc_walk.h"
#include "mc_watch.h"
#include <regex>
#ifdef MC_QTCLIENT
#	include "qtClient.h"
#else
#	include "mc_workerthread.h"
#endif


using namespace std;
#define CAFILE "trustCA.crt"

ofstream mc_logfile;

int runmc()
{
	int rc,wrc;
	time_t basedate;
	mc_status status;
	unsigned char hash[16];
	
	mc_logfile << "Begin run at " << TimeToString(time(NULL)) << endl;
	//rc = db_open("state.db");
	rc = 0;
	if(!rc){
		rc = db_select_status(&status);
		if(!rc){
			while(true){
				if(status.url == ""){ //TODO: ask
					MC_ERR_MSG(MC_ERR_NOT_CONFIGURED,"No server configured");
				}
				rc = srv_open(status.url,CAFILE,status.uname,status.passwd,&basedate,status.acceptallcerts);
				if(!rc){
					try {
						MC_NOTIFYSTART(MC_NT_CONN,status.url);
						if(status.basedate < basedate){
							if(status.basedate != 0){
								MC_WRN("Server has newer basedate, resetting");
								rc = db_execstr("DELETE FROM files");
								if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
								//rc = db_execstr("UPDATE syncs SET filterversion = 0");
								rc = db_execstr("DELETE FROM filters");
								if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
								rc = db_execstr("DELETE FROM syncs");
								if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
								MC_NOTIFYEND(MC_NT_SYNC); // Trigger listSyncs()
							}
							status.basedate = basedate;
							rc = db_update_status(&status);
							if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
						}
					

						//TODO: resume and stuff
						while(true){
							mc_sync_ctx context;
							list<mc_filter> generalfilter,filter;
							//rc = update_filters(0);
							//if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
							rc = db_list_filter_sid(&generalfilter,0);
							if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }

							//Get list of syncs and compare to local
							list<mc_sync> srvsyncs;// = list<mc_sync>();
							list<mc_sync>::iterator srvsyncsit,srvsyncsend;
							list<mc_sync_db> dbsyncs;// = list<mc_sync_db>();
							list<mc_sync_db>::iterator dbsyncsit,dbsyncsend;
							rc = srv_listsyncs(&srvsyncs);
							if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }

							rc = db_list_sync(&dbsyncs);
							if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
							if(dbsyncs.size() == 0){
								MC_ERR_MSG(MC_ERR_NOT_CONFIGURED,"No syncs configured");
							}
							MC_INFL("Beginning run");

							dbsyncs.sort(compare_mc_sync_db_prio);

							dbsyncsit = dbsyncs.begin();
							dbsyncsend = dbsyncs.end();
							srvsyncsend = srvsyncs.end();
							for(;dbsyncsit != dbsyncsend; ++dbsyncsit){ //Foreach db sync
								if(MC_TERMINATING()) break;
								if(dbsyncsit->status == MC_SYNCSTAT_DISABLED) continue;

								srvsyncsit = srvsyncs.begin();
								while((srvsyncsit != srvsyncsend) && (dbsyncsit->id != srvsyncsit->id)) ++srvsyncsit;
								if((srvsyncsit == srvsyncsend) || (dbsyncsit->id != srvsyncsit->id)){
									MC_WRN(dbsyncsit->name << " is not available on server");
									dbsyncsit->status = MC_SYNCSTAT_UNAVAILABLE;
									rc = db_update_sync(&*dbsyncsit);
									MC_CHKERR(rc);
								} else {
									init_sync_ctx(&context,&*dbsyncsit,&filter);
									if(dbsyncsit->filterversion < srvsyncsit->filterversion){
										list<mc_filter> fl;
										//updates of sid-0-filters count as update to all syncs
										//this will cause multiple download but hopefully happens rarely enough
										//and is better than adding a fake-sync-0
										rc = srv_listfilters(&fl,0);
										if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
										//general filters are not encrypted
										rc = update_filters(0,&fl);
										if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
										generalfilter.clear();
										rc = db_list_filter_sid(&generalfilter,0);
										if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }

										fl.clear();
										rc = srv_listfilters(&fl,dbsyncsit->id);
										if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
										rc = crypt_filterlist_fromsrv(&context,dbsyncsit->name,&fl);
										if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
										rc = update_filters(dbsyncsit->id,&fl);
										if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
										dbsyncsit->filterversion = srvsyncsit->filterversion;
										rc = db_update_sync(&*dbsyncsit);
										if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
									}
									filter.assign(generalfilter.begin(),generalfilter.end());
									rc = db_list_filter_sid(&filter,dbsyncsit->id);
									if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
									dbsyncsit->status = MC_SYNCSTAT_RUNNING;
									rc = db_update_sync(&*dbsyncsit);
									if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
									MC_NOTIFYSTART(MC_NT_SYNC,dbsyncsit->name);
									if(memcmp(dbsyncsit->hash,srvsyncsit->hash,16) == 0){
										MC_INFL(dbsyncsit->name << " has not been updated on server, walking locally");
										wrc = walk_nochange(&context,"",-dbsyncsit->id,hash);
										//MC_INFL("Sync completed (Code " << wrc << ")");
									} else {
										MC_INFL(dbsyncsit->name << " has been updated on server, walking");
										wrc = walk(&context,"",-dbsyncsit->id,hash);
										//MC_INF("Sync completed (Code " << wrc << ")");
									}
									rc = db_select_sync(&*dbsyncsit);
									if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
									memcpy(dbsyncsit->hash,hash,16);
									if(wrc == MC_ERR_TERMINATING) dbsyncsit->status = MC_SYNCSTAT_ABORTED;
									else if (MC_IS_CRITICAL_ERR(wrc)) dbsyncsit->status = MC_SYNCSTAT_FAILED;
									else if(memcmp(dbsyncsit->hash,srvsyncsit->hash,16) == 0) dbsyncsit->status = MC_SYNCSTAT_SYNCED;
									else dbsyncsit->status = MC_SYNCSTAT_COMPLETED;
									dbsyncsit->lastsync = time(NULL);
									rc = db_update_sync(&*dbsyncsit);
									if(rc) { if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
									if(wrc == MC_ERR_CRYPTOALERT) 
										throw MC_ERR_CRYPTOALERT;
									MC_NOTIFYEND(MC_NT_SYNC);
								}
							}

							if(MC_TERMINATING()) break;

							//Check for sync
							rc = fullsync(&dbsyncs);
							if(!rc){
								MC_INFL("Run completed, fully synced");
								MC_NOTIFYSTART(MC_NT_FULLSYNC,TimeToString(time(NULL)));
							} else if(rc == MC_ERR_NOTFULLYSYNCED){
								MC_INFL("Run completed, not synced, running again");
								MC_NOTIFYEND(MC_NT_SYNC); //Trigger ListSyncs
								continue;
							} else if(rc == MC_ERR_NETWORK){
								MC_NOTIFYSTART(MC_NT_ERROR, "Network Error");						
								if(status.watchmode > 0)
									mc_sleep_checkterminate(status.watchmode);
								else
									mc_sleep_checkterminate(-status.watchmode);
								continue;
									//{ if(rc == MC_ERR_CRYPTOALERT) MC_INF("Got cryptoalert"); throw rc; }
							}
							srv_standby();


	#ifdef MC_WATCHMODE
							if(status.watchmode > 0)
								enter_watchmode(status.watchmode);
							else
								mc_sleep_checkterminate(-status.watchmode);
	#else
							mc_sleep_checkterminate(-status.watchmode);
	#endif

							if(MC_TERMINATING()) break;

						}
					} catch (int i){
						switch(i){
							case MC_ERR_TERMINATING:
								break;
							case MC_ERR_PROTOCOL:
								cerr << "Error while processing protocol. Aborting" << endl;
								MC_NOTIFYSTART(MC_NT_ERROR,"Protocol Error");
								break;
							case MC_ERR_CRYPTOALERT:
								cerr << "Crypt Verify Fail. Aborting." << endl;
								srv_close();
								cerr << "Server can't be trusted. Disabling all Syncs." << endl;
								db_execstr(string("UPDATE syncs SET status = ") + to_string(MC_SYNCSTAT_DISABLED));
								cerr << "Changing server url to force manual interaction." << endl;
								db_execstr("UPDATE status SET url = 'UNTRUSTED: ' || url");
								MC_NOTIFYEND(MC_NT_SYNC); //Trigger ListSyncs
								MC_NOTIFYSTART(MC_NT_ERROR,"Crypt Verify Fail: Server can't be trusted");
								return MC_ERR_CRYPTOALERT;
							default:
								cerr << "Internal error: " << i << " Aborting." << endl;
								MC_NOTIFYSTART(MC_NT_ERROR,"Internal error");
						}
					}
					MC_NOTIFYEND(MC_NT_CONN);
				} else {
					cerr << "Error while connecting" << endl;
					MC_NOTIFYSTART(MC_NT_ERROR,"Failed to connect to server");

				}
				srv_close();
				if(MC_TERMINATING()) break;
				
				//Reach here on longer connection fail
				//MC_INF("Retrying in 60 sec");
				mc_sleep_checkterminate(60);
			}
		} else {
			cerr << "Database Error" << endl;
			MC_NOTIFYSTART(MC_NT_ERROR,"Failed to open database");
		}
	}
	//db_close();

	mc_logfile << "End run at " << TimeToString(time(NULL)) << endl;

	return 0;
}

