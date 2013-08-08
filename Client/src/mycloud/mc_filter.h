#ifndef MC_FILTER_H
#define MC_FILTER_H
#include "mc.h"
#include "mc_transfer.h" //for purging invalid files

#include <regex>

// Functions for filtering directory listings, so we can ignore files
/* update filter lists for sync from server	*/
int update_filters(int sid);

/* match the full path against all filters, returns wether hit	*/
bool match_full(const string& path, mc_file *file, list<mc_filter> *filter);
bool match_full(const string& path, mc_file_fs *file, list<mc_filter> *filter);
/* filter the listings of files we want to ignore */
int filter_lists(const string& path, list<mc_file_fs> *onfs, list<mc_file> *indb, list<mc_file> *onsrv, list<mc_filter> *filter);

#endif /* MC_FILTER_H */