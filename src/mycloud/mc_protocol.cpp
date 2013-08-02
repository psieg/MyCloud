#include "mc_protocol.h"

/* These functions fill buf with the respective request body */

void pack_auth(mc_buf *buf, string user, string passwd){
	int num;
	unsigned int index = 0;
	size_t bufsize = 4*sizeof(int)+user.size()+passwd.size();
	MatchBuf(buf,bufsize);
	
	num = MC_SRVQRY_AUTH;
	memcpy(&buf->mem[index],&num,sizeof(int));
	index += sizeof(int);

	num = user.size();
	memcpy(&buf->mem[index],&num,sizeof(int));
	index += sizeof(int);
	memcpy(&buf->mem[index],user.c_str(),num);
	index += user.size();

	num = passwd.size();
	memcpy(&buf->mem[index],&num,sizeof(int));
	index += sizeof(int);
	memcpy(&buf->mem[index],passwd.c_str(),num);
	index += num;

	num = MC_CLIENT_PROTOCOL_VERSION;
	memcpy(&buf->mem[index],&num,sizeof(int));
	index += sizeof(int);

	buf->used = bufsize;
}

void pack_listsyncs(mc_buf *buf, unsigned char authtoken[16]){
	int num = MC_SRVQRY_LISTSYNCS;
	MatchBuf(buf,sizeof(int)+16);
	memcpy(&buf->mem[0],&num,sizeof(int));
	memcpy(&buf->mem[sizeof(int)],authtoken,16);
	buf->used = sizeof(int)+16;
}

void pack_listfilters(mc_buf *buf, unsigned char authtoken[16], int syncid){
	int num = MC_SRVQRY_LISTFILTERS;
	MatchBuf(buf,2*sizeof(int)+16);
	memcpy(&buf->mem[0],&num,sizeof(int));
	memcpy(&buf->mem[sizeof(int)],authtoken,16);

	memcpy(&buf->mem[sizeof(int)+16],&syncid,sizeof(int));
	buf->used = 2*sizeof(int)+16;
}

void pack_listdir(mc_buf *buf, unsigned char authtoken[16], int parent){
	int num = MC_SRVQRY_LISTDIR;
	MatchBuf(buf,2*sizeof(int)+16);
	memcpy(&buf->mem[0],&num,sizeof(int));
	memcpy(&buf->mem[sizeof(int)],authtoken,16);

	memcpy(&buf->mem[sizeof(int)+16],&parent,sizeof(int));
	buf->used = 2*sizeof(int)+16;
}


void pack_getfile(mc_buf *buf, unsigned char authtoken[16], int id, int64 offset, int64 blocksize, unsigned char hash[16]){
	int num = MC_SRVQRY_GETFILE;
	MatchBuf(buf,2*sizeof(int)+2*sizeof(int64)+2*16);
	memcpy(&buf->mem[0],&num,sizeof(int));
	memcpy(&buf->mem[sizeof(int)],authtoken,16);

	memcpy(&buf->mem[sizeof(int)+16],&id,sizeof(int));
	memcpy(&buf->mem[2*sizeof(int)+16],&offset,sizeof(int64));
	memcpy(&buf->mem[2*sizeof(int)+sizeof(int64)+16],&blocksize,sizeof(int64));
	memcpy(&buf->mem[2*sizeof(int)+2*sizeof(int64)+16],hash,16);
	buf->used = 2*sizeof(int)+2*sizeof(int64)+2*16;
}

void pack_getoffset(mc_buf *buf, unsigned char authtoken[16], int id){
	int num = MC_SRVQRY_GETOFFSET;
	MatchBuf(buf,sizeof(int)+sizeof(int64)+16);
	memcpy(&buf->mem[0],&num,sizeof(int));
	memcpy(&buf->mem[sizeof(int)],authtoken,16);

	memcpy(&buf->mem[sizeof(int)+16],&id,sizeof(int));
	buf->used = sizeof(int)+sizeof(int64)+16;
}

//Note the caller has to append the filedata (size blocksize) itself to the buffer
void pack_putfile(mc_buf *buf, unsigned char authtoken[16], mc_file *file, int64 blocksize){
	int num = MC_SRVQRY_PUTFILE;
	unsigned int index = 0;
	size_t bufsize = 5*sizeof(int)+4*sizeof(int64)+2*16+file->name.length();
	MatchBuf(buf,bufsize);

	memcpy(&buf->mem[index],&num,sizeof(int));
	index += sizeof(int);
	memcpy(&buf->mem[index],authtoken,16);
	index += 16;

	memcpy(&buf->mem[index],&file->id,sizeof(int));
	index += sizeof(int);
	num = file->name.length();
	memcpy(&buf->mem[index],&num,sizeof(int));
	index += sizeof(int);
	memcpy(&buf->mem[index],file->name.c_str(),num);
	index += num;
	memcpy(&buf->mem[index],&file->ctime,sizeof(int64));
	index += sizeof(int64);
	memcpy(&buf->mem[index],&file->mtime,sizeof(int64));
	index += sizeof(int64);
	memcpy(&buf->mem[index],&file->size,sizeof(int64));
	index += sizeof(int64);
	num = file->is_dir != 0;
	memcpy(&buf->mem[index],&num,sizeof(int));
	index += sizeof(int);
	memcpy(&buf->mem[index],&file->parent,sizeof(int));
	index += sizeof(int);
	memcpy(&buf->mem[index],&file->hash,16);
	index += 16;
	
	memcpy(&buf->mem[index],&blocksize,sizeof(int64));
	index += sizeof(int64);
	buf->used = bufsize;
}

//Note the caller has to append the filedata (size blocksize) itself to the buffer
void pack_addfile(mc_buf *buf, unsigned char authtoken[16], int id, int64 offset, int64 blocksize, unsigned char hash[16]){
	int num = MC_SRVQRY_ADDFILE;
	MatchBuf(buf,2*sizeof(int)+2*sizeof(int64)+2*16);
	memcpy(&buf->mem[0],&num,sizeof(int));
	memcpy(&buf->mem[sizeof(int)],authtoken,16);

	memcpy(&buf->mem[sizeof(int)+16],&id,sizeof(int));
	memcpy(&buf->mem[2*sizeof(int)+16],&offset,sizeof(int64));
	memcpy(&buf->mem[2*sizeof(int)+sizeof(int64)+16],&blocksize,sizeof(int64));
	memcpy(&buf->mem[2*sizeof(int)+2*sizeof(int64)+16],hash,16);
	buf->used = 2*sizeof(int)+2*sizeof(int64)+2*16;

}

void pack_patchfile(mc_buf *buf, unsigned char authtoken[16], mc_file *file){
	int num = MC_SRVQRY_PATCHFILE;
	unsigned int index = 0;
	size_t bufsize = 4*sizeof(int)+2*sizeof(int64)+16+file->name.length();
	MatchBuf(buf,bufsize);

	memcpy(&buf->mem[index],&num,sizeof(int));
	index += sizeof(int);
	memcpy(&buf->mem[index],authtoken,16);
	index += 16;

	memcpy(&buf->mem[index],&file->id,sizeof(int));
	index += sizeof(int);
	num = file->name.length();
	memcpy(&buf->mem[index],&num,sizeof(int));
	index += sizeof(int);
	memcpy(&buf->mem[index],file->name.c_str(),num);
	index += num;
	memcpy(&buf->mem[index],&file->ctime,sizeof(int64));
	index += sizeof(int64);
	memcpy(&buf->mem[index],&file->mtime,sizeof(int64));
	index += sizeof(int64);
	memcpy(&buf->mem[index],&file->parent,sizeof(int));
	index += sizeof(int);
	
	buf->used = bufsize;
}

void pack_delfile(mc_buf *buf, unsigned char authtoken[16], int id, int64 mtime){
	int num = MC_SRVQRY_DELFILE;
	MatchBuf(buf,2*sizeof(int)+sizeof(int64)+16);
	memcpy(&buf->mem[0],&num,sizeof(int));
	memcpy(&buf->mem[sizeof(int)],authtoken,16);

	memcpy(&buf->mem[sizeof(int)+16],&id,sizeof(int));
	memcpy(&buf->mem[2*sizeof(int)+16],&mtime,sizeof(int64));
	buf->used = 2*sizeof(int)+sizeof(int64)+16;
}

void pack_getmeta(mc_buf *buf, unsigned char authtoken[16], int id){
	int num = MC_SRVQRY_GETMETA;
	MatchBuf(buf,sizeof(int)+sizeof(int64)+16);
	memcpy(&buf->mem[0],&num,sizeof(int));
	memcpy(&buf->mem[sizeof(int)],authtoken,16);

	memcpy(&buf->mem[sizeof(int)+16],&id,sizeof(int));
	buf->used = sizeof(int)+sizeof(int64)+16;
}


void pack_purgefile(mc_buf *buf, unsigned char authtoken[16], int id){
	int num = MC_SRVQRY_PURGEFILE;
	MatchBuf(buf,sizeof(int)+sizeof(int64)+16);
	memcpy(&buf->mem[0],&num,sizeof(int));
	memcpy(&buf->mem[sizeof(int)],authtoken,16);

	memcpy(&buf->mem[sizeof(int)+16],&id,sizeof(int));
	buf->used = sizeof(int)+sizeof(int64)+16;
}

void pack_notifychange(mc_buf *buf, unsigned char authtoken[16], list<mc_sync_db> *l){
	list<mc_sync_db>::iterator lit,lend;
	int index = 0;
	int num = MC_SRVQRY_NOTIYCHANGE;
	MatchBuf(buf,sizeof(int)+16+l->size()*(sizeof(int)+16));
	memcpy(&buf->mem[index],&num,sizeof(int));
	index += sizeof(int);
	memcpy(&buf->mem[index],authtoken,16);
	index += 16;

	lit = l->begin();
	lend = l->end();
	while(lit!=lend){
		memcpy(&buf->mem[index],&lit->id,sizeof(int));
		index += sizeof(int);
		memcpy(&buf->mem[index],&lit->hash,16);
		index += 16;
		++lit;
	}
	buf->used = sizeof(int)+16+l->size()*(sizeof(int)+16);
}


/* These functions fill the params with the response buffer's contents 
*	Offset is always sizeof(int) as the server status code does not matter to us */
void unpack_authed(mc_buf *buf, unsigned char authtoken[16], int64 *time, int64 *basedate, int *version){
	unsigned int index = sizeof(int);
	try {
		memcpy(authtoken,&buf->mem[index],16);
		index += sizeof(char)*16;
		memcpy(time,&buf->mem[index],sizeof(int64));
		index += sizeof(int64);
		memcpy(basedate,&buf->mem[index],sizeof(int64));
		index += sizeof(int64);
		memcpy(version,&buf->mem[index],sizeof(int));
	} catch (...) {
		throw MC_ERR_PROTOCOL;
	}
}

void unpack_synclist(mc_buf *buf, list<mc_sync> *l){
	unsigned int index = sizeof(int);
	int namelen = 0;
	mc_sync item;
	try {
		while(index < buf->used){
			memcpy(&item.id,&buf->mem[index],sizeof(int));
			index += sizeof(int);
			memcpy(&namelen,&buf->mem[index],sizeof(int));
			index += sizeof(int);
			item.name.assign(&buf->mem[index],namelen);
			index += namelen;
			memcpy(&item.filterversion,&buf->mem[index],sizeof(int));
			index += sizeof(int);
			item.crypted = buf->mem[index] != 0;
			index += sizeof(char);
			memcpy(&item.hash,&buf->mem[index],16);
			index += 16;
			l->push_back(item);
		}
	} catch (...) {
		throw MC_ERR_PROTOCOL;
	}
}

void unpack_filterlist(mc_buf *buf, list<mc_filter> *l){
	unsigned int index = sizeof(int);
	int rulelen = 0;
	mc_filter item;
	try {
		while(index < buf->used){
			memcpy(&item.id,&buf->mem[index],sizeof(int));
			index += sizeof(int);
			memcpy(&item.sid,&buf->mem[index],sizeof(int));
			index += sizeof(int);
			item.files = buf->mem[index] != 0;
			index += sizeof(char);
			item.directories = buf->mem[index] != 0;
			index += sizeof(char);
			memcpy(&item.type,&buf->mem[index],sizeof(int));
			index += sizeof(int);
			memcpy(&rulelen,&buf->mem[index],sizeof(int));
			index += sizeof(int);
			item.rule.assign(&buf->mem[index],rulelen);
			index += rulelen;
			l->push_back(item);
		}
	} catch (...) {
		throw MC_ERR_PROTOCOL;
	}

}

void unpack_dirlist(mc_buf *buf, list<mc_file> *l){
	unsigned int index = sizeof(int);
	int namelen = 0;
	mc_file item;
	try {
		while(index < buf->used){
			memcpy(&item.id,&buf->mem[index],sizeof(int));
			index += sizeof(int);
			memcpy(&namelen,&buf->mem[index],sizeof(int));
			index += sizeof(int);
			item.name.assign(&buf->mem[index],namelen);
			index += namelen;
			item.cryptname = "";
			memcpy(&item.ctime,&buf->mem[index],sizeof(int64));
			index += sizeof(int64);
			memcpy(&item.mtime,&buf->mem[index],sizeof(int64));
			index += sizeof(int64);
			memcpy(&item.size,&buf->mem[index],sizeof(int64));
			index += sizeof(int64);
			item.is_dir = buf->mem[index] != 0;
			index += sizeof(char);
			memcpy(&item.parent,&buf->mem[index],sizeof(int));
			index += sizeof(int);
			memcpy(&item.status,&buf->mem[index],sizeof(int));
			index += sizeof(int);
			memcpy(&item.hash,&buf->mem[index],16);
			index += 16;
			l->push_back(item);
		}
	} catch (...) {
		throw MC_ERR_PROTOCOL;
	}
}

void unpack_file(mc_buf *buf, int64 *offset){
	try {
		memcpy(offset,&buf->mem[sizeof(int)],sizeof(int64));
	} catch (...) {
		throw MC_ERR_PROTOCOL;
	}
}

void unpack_offset(mc_buf *buf, int64 *offset){
	try {
		memcpy(offset,&buf->mem[sizeof(int)],sizeof(int64));
	} catch (...){
		throw MC_ERR_PROTOCOL;
	}
}

void unpack_fileid(mc_buf *buf, int *id){
	try {
		memcpy(id,&buf->mem[sizeof(int)],sizeof(int));
	} catch (...) {
		throw MC_ERR_PROTOCOL;
	}
}

void unpack_filemeta(mc_buf *buf, mc_file *file){
	unsigned int index = sizeof(int);
	int namelen;
	try {			
		memcpy(&file->id,&buf->mem[index],sizeof(int));
		index += sizeof(int);
		memcpy(&namelen,&buf->mem[index],sizeof(int));
		index += sizeof(int);
		file->name.assign(&buf->mem[index],namelen);
		index += namelen;
		file->cryptname = "";
		memcpy(&file->ctime,&buf->mem[index],sizeof(int64));
		index += sizeof(int64);
		memcpy(&file->mtime,&buf->mem[index],sizeof(int64));
		index += sizeof(int64);
		memcpy(&file->size,&buf->mem[index],sizeof(int64));
		index += sizeof(int64);
		file->is_dir = buf->mem[index] != 0;
		index += sizeof(char);
		memcpy(&file->parent,&buf->mem[index],sizeof(int));
		index += sizeof(int);
		memcpy(&file->status,&buf->mem[index],sizeof(int));
		index += sizeof(int);
		memcpy(&file->hash,&buf->mem[index],16);
	} catch (...){
		throw MC_ERR_PROTOCOL;
	}
}

void unpack_change(mc_buf *buf, int *id){
	try {
		memcpy(id,&buf->mem[sizeof(int)],sizeof(int));
	} catch (...) {
		throw MC_ERR_PROTOCOL;
	}
}