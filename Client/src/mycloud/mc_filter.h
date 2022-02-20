#ifndef MC_FILTER_H
#define MC_FILTER_H
#include "mc.h"
#include "mc_transfer.h" //for purging invalid files

#include <regex>

// Functions for filtering directory listings, so we can ignore files
/* update filter lists for sync from server	*/
int update_filters(int sid, std::list<mc_filter> *l);

/* match the full path against all filters, returns whether hit	*/
bool match_full(const std::string& path, mc_file *file, std::list<mc_filter> *filter);
bool match_full(const std::string& path, mc_file_fs *file, std::list<mc_filter> *filter);
/* filter the listings of files we want to ignore */
int filter_lists(const std::string& path, std::list<mc_file_fs> *onfs, std::list<mc_file> *indb, std::list<mc_file> *onsrv, std::list<mc_filter> *filter);

#endif /* MC_FILTER_H */