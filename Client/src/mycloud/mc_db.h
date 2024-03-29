#ifndef MC_DB_H
#define MC_DB_H
#include "mc.h"

// Functions dealing with the local statedb
#include <sqlite3.h>
#include <QtCore/QMutex>

bool db_isopen();
int db_open(const std::string& fname);
int db_close();
int db_execstr(const std::string& query); /* For administrative and one-time stuff only */

int db_select_status(mc_status *var);
int db_update_status(mc_status *var);

int db_select_sync(mc_sync_db *var);
int db_list_sync(std::list<mc_sync_db> *l);
int db_insert_sync(mc_sync_db *var);
int db_update_sync(mc_sync_db *var);
int db_delete_sync(int id);

int db_select_filter(mc_filter *var);
int db_list_filter_sid(std::list<mc_filter> *l, int sid);
int db_insert_filter(mc_filter *var);
int db_update_filter(mc_filter *var);
int db_delete_filter(int id);
int db_delete_filter_sid(int sid);

int db_list_share_sid(std::list<mc_share> *l, int sid);
int db_insert_share(mc_share *var);
int db_delete_share(mc_share *var);
int db_delete_share_sid(int sid);

int db_select_user(mc_user *var);
int db_list_user(std::list<mc_user> *l);
int db_insert_user(mc_user *var);
// no reason to update / delete users

int db_select_file_name(mc_file *var);
int db_select_file_id(mc_file *var);
int db_list_file_parent(std::list<mc_file> *l, int parent);
int db_insert_file(mc_file *var);
int db_update_file(mc_file *var);
int db_delete_file(int id);
int db_select_min_fileid(int *id);

#endif /* MC_DB_H */