#include "mc_filter.h"

/* update filter lists for sync from server	*/
int update_filters(int sid, list<mc_filter> *l) {
	;
	int rc;
	MC_DBG("Updating filter lists for sid: " << sid);
	//clear all
	rc = db_delete_filter_sid(sid);
	MC_CHKERR(rc);
	//load new
	for (mc_filter& f : *l) {
		rc = db_insert_filter(&f);
		MC_CHKERR(rc);
	}
	return 0;
}


/* match the full path against all filters, returns wether hit	
*	is_dir will append a / to the path for matching	*/
bool _match_full(const string& path, const string& fname, bool is_dir, list<mc_filter> *filter) {
	regex r;
	string fpath, name, ext;
	size_t dotpos = fname.find_last_of('.');
	name = fname.substr(0, dotpos);
	if (dotpos == string::npos) ext = "";
	else ext = fname.substr(fname.find_last_of('.')+1);
	fpath.assign(path).append(fname);
	MC_DBGL("Matching " << fpath);
	for (mc_filter& f : *filter) {
		if (is_dir && !f.directories) { continue; }
		if (!is_dir && !f.files) { continue; }
		switch(f.type) {
			case MC_FILTERT_MATCH_NAME:
				if (nocase_equals(name, f.rule)) {
					MC_DBG("Matched " << fname << " against " << f.rule);
					return true;
				}
				break;
			case MC_FILTERT_MATCH_EXTENSION:
				if (nocase_equals(ext, f.rule)) {
					MC_DBG("Matched " << fname << " against " << f.rule);
					return true;
				}
				break;
			case MC_FILTERT_MATCH_FULLNAME:
				if (nocase_equals(fname, f.rule)) {
					MC_DBG("Matched " << fname << " against " << f.rule);
					return true;
				}
				break;
			case MC_FILTERT_MATCH_PATH:
				if (nocase_equals(fpath, f.rule)) {
					MC_DBG("Matched " << fname << " against " << f.rule);
					return true;
				}
				break;
			case MC_FILTERT_REGEX_NAME:
				r = regex(f.rule.c_str());
				if (regex_match(name, r, regex_constants::match_default)) {
					MC_DBG("Matched " << fname << " against " << f.rule);
					return true;
				}
				break;
			case MC_FILTERT_REGEX_EXTENSION:
				r = regex(f.rule.c_str());
				if (regex_match(ext, r, regex_constants::match_default)) {
					MC_DBG("Matched " << fname << " against " << f.rule);
					return true;
				}
				break;
			case MC_FILTERT_REGEX_FULLNAME:
				r = regex(f.rule.c_str());
				if (regex_match(fname, r, regex_constants::match_default)) {
					MC_DBG("Matched " << fname << " against " << f.rule);
					return true;
				}
				break;
			case MC_FILTERT_REGEX_PATH:
				r = regex(f.rule.c_str());
				if (regex_match(fpath, r, regex_constants::match_default)) {
					MC_DBG("Matched " << fname << " against " << f.rule);
					return true;
				}
				break;
			default:
				MC_ERR_MSG(false, "Unkown filter type: " << f.type);
		}
	}
	return false;
}
bool match_full(const string& path, mc_file *file, list<mc_filter> *filter) {
	return _match_full(path, file->name, file->is_dir, filter);
}
bool match_full(const string& path, mc_file_fs *file, list<mc_filter> *filter) {
	return _match_full(path, file->name, file->is_dir, filter);
}

/* filter the listings of files we want to ignore	*/
int filter_lists(const string& path, list<mc_file_fs> *onfs, list<mc_file> *indb, list<mc_file> *onsrv, list<mc_filter> *filter) {
	list<mc_file_fs> fsdummy;
	list<mc_file> srvdummy;
	list<mc_file_fs>::iterator onfsit, onfsend;
	list<mc_file>::iterator indbit, indbend;
	list<mc_file>::iterator onsrvit, onsrvend;
	int rc;
	//path.append("/"); //done by walk already
	MC_DBG("Filtering listing: " << path);
	if (onfs == NULL) {
		onfsit = fsdummy.end();
		onfsend = fsdummy.end();
	} else {
		onfsit = onfs->begin();
		onfsend = onfs->end();
	}
	indbit = indb->begin();
	indbend = indb->end();
	if (onsrv == NULL) {
		onsrvit = srvdummy.end();
		onsrvend = srvdummy.end();
	} else {
		onsrvit = onsrv->begin();
		onsrvend = onsrv->end();
	}
	while ((onfsit != onfsend) || (indbit != indbend) || (onsrvit != onsrvend)) {
		if (indbit == indbend) {
			if (onfsit == onfsend) {
				// New file on server
				if (match_full(path, &*onsrvit, filter)) { 
					rc = purge(NULL, &*onsrvit);
					MC_CHKERR(rc);
					onsrv->erase(onsrvit++);
				} else ++onsrvit;
			} else if ((onsrvit == onsrvend) || (onsrvit->name > onfsit->name)) {
				// New file in fs
				if (match_full(path, &*onfsit, filter)) onfs->erase(onfsit++);
				else ++onfsit;
			} else if (onsrvit->name < onfsit->name) {
				// New file on server
				if (match_full(path, &*onsrvit, filter)) { 
					rc = purge(NULL, &*onsrvit);
					MC_CHKERR(rc);
					onsrv->erase(onsrvit++);
				}
				else ++onsrvit;
			} else { // onsrvit->name == onfsit->name
				// File added on srv and fs
				if (match_full(path, &*onfsit, filter)) { 					
					rc = purge(NULL, &*onsrvit);
					MC_CHKERR(rc);
					onsrv->erase(onsrvit++);
					onfs->erase(onfsit++); 
				} else { ++onsrvit; ++onfsit; }
			}
		} else if ((onfsit == onfsend) || onfsit->name > indbit->name) {
			if ((onsrvit == onsrvend) || (onsrvit->name > indbit->name)) {
				// Lonely db entry
				// Should not happen (known to db = known to srv at last sync)
				// Since deleted files are still known to srv, this should not happen
				// Assumption: File has been deleted on fs while not known to srv (client must have been killed during sync)
				// We drop this entry
				if (match_full(path, &*indbit, filter)) {
					rc = purge(&*indbit, NULL);
					MC_CHKERR(rc);
					onsrv->erase(indbit++);
				} else ++indbit;
			} else if (onsrvit->name == indbit->name) {
				// File has been deleted
				if (match_full(path, &*indbit, filter)) {
					rc = purge(&*indbit, NULL);
					MC_CHKERR(rc);
					indb->erase(indbit++); 
					onsrv->erase(onsrvit++); }
				else { ++indbit; ++onsrvit; }
			} else { // onsrvit->name < indbit->name
				// New file on server
				if (match_full(path, &*onsrvit, filter)) {
					rc = purge(NULL, &*onsrvit);
					MC_CHKERR(rc);
					onsrv->erase(onsrvit++);
				} else ++onsrvit;
			}
		} else if (onfsit->name == indbit->name) {
			if ((onsrvit == onsrvend) || (onsrvit->name > onfsit->name)) {
				// Should not happen (known to db = known to srv at last sync)
				// Since deleted files are still known to srv, this should not happen
				// Assumption: Added (client must have been killed during sync)
				if (match_full(path, &*indbit, filter)) {
					rc = purge(&*indbit, NULL);
					MC_CHKERR(rc);
					indb->erase(indbit++);
					onfs->erase(onfsit++); 
				} else { ++indbit; ++onfsit; }
			} else if (onsrvit->name == onfsit->name) {
				// Standard: File known to all, check modified
				if (match_full(path, &*indbit, filter)) {
					rc = purge(&*indbit, &*onsrvit);
					MC_CHKERR(rc);
					onfs->erase(onfsit++);
					indb->erase(indbit++);
					onsrv->erase(onsrvit++); 
				} else { ++onfsit; ++indbit; ++onsrvit; }
			} else { // onsrvit->name < onfsit->name
				// New file on server
				if (match_full(path, &*onsrvit, filter)) { 
					rc = purge(NULL, &*onsrvit);
					MC_CHKERR(rc);
					onsrv->erase(onsrvit++);
				} else ++onsrvit;
			}
		} else { // onfsit->name < indbit->name
			if ((onsrvit == onsrvend) || (onsrvit->name > onfsit->name)) {
				// New file in fs
				if (match_full(path, &*onfsit, filter)) onfs->erase(onfsit++);
				else ++onfsit;
			} else if (onsrvit->name == onfsit->name) {
				// File added on srv and fs
				if (match_full(path, &*onfsit, filter)) { 					
					rc = purge(NULL, &*onsrvit);
					MC_CHKERR(rc);
					onsrv->erase(onsrvit++);
					onfs->erase(onfsit++); 
				} else { ++onsrvit; ++onfsit; }
			} else { // onsrvit->name < onfsit->name
				// New file on server
				if (match_full(path, &*onsrvit, filter)) { 
					rc = purge(NULL, &*onsrvit);
					MC_CHKERR(rc);
					onsrv->erase(onsrvit++);
				} else ++onsrvit;
			}
		}
		
	}
	return 0;
}
