#include "mc_crypt.h"
#ifdef MC_QTCLIENT
#	include "qtClient.h"
#else
#	include "mc_workerthread.h"
#endif
#include "openssl/rand.h"

//Helper function for generating IVs from string (parent path or sync name)
int crypt_striv(mc_crypt_ctx *cctx, const string& path){
	unsigned char h[16];
	int rc = strmd5(h,path);
	MC_CHKERR(rc);
	memcpy(cctx->iv+MC_CRYPTNAME_IDOFFSET,h,MC_CRYPTNAME_IVSIZE);
	return 0;
}
//Randomness helper function for generating IVs
int crypt_randiv(mc_crypt_ctx *cctx){
	if(RAND_status()){
		if(!RAND_bytes(cctx->iv+MC_CRYPT_IDOFFSET,MC_CRYPT_IVSIZE))
			MC_ERR_MSG(MC_ERR_CRYPTO,"RAND_bytes failed");
	} else MC_ERR_MSG(MC_ERR_CRYPTO,"Not enough random data");

	return 0;
}
//Generate a new key
int crypt_randkey(unsigned char key[MC_CRYPT_KEYSIZE]){
	if(RAND_status()){
		if(!RAND_bytes(key,MC_CRYPT_KEYSIZE))
			MC_ERR_MSG(MC_ERR_CRYPTO,"RAND_bytes failed");
	} else MC_ERR_MSG(MC_ERR_CRYPTO,"Not enough random data");

	return 0;
}

void crypt_seed(const string& data){
	RAND_add(data.c_str(),data.length(),data.length()*0.5);
}
void crypt_seed(int64 data){
	RAND_add(&data,sizeof(int64),sizeof(int64));
}

int crypt_encryptstring(mc_sync_ctx *ctx, const string& ivstr, const string& data, string *out){
	mc_crypt_ctx cctx;
	QByteArray buf;
	int rc, written;
	buf = QByteArray(data.length()+MC_CRYPTNAME_SIZEOVERHEAD,'\0');

	init_crypt_ctx(&cctx,ctx);
	MC_CHKERR(crypt_striv(&cctx,ivstr));
	memcpy(cctx.iv,MC_CRYPTNAME_IDENTIFIER,strlen(MC_CRYPTNAME_IDENTIFIER));
	memcpy(buf.data(),cctx.iv,MC_CRYPTNAME_OFFSET);

	cctx.evp = EVP_CIPHER_CTX_new();

	rc = EVP_EncryptInit_ex(cctx.evp,EVP_aes_256_gcm(),NULL,cctx.ctx->sync->cryptkey,cctx.iv+MC_CRYPTNAME_IDOFFSET);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptInit failed");
			
	rc = EVP_EncryptUpdate(cctx.evp, (unsigned char*)buf.data()+MC_CRYPTNAME_OFFSET, &written, (unsigned char*)data.c_str(), data.length());
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptUpdate failed");

	rc = EVP_EncryptFinal_ex(cctx.evp, NULL, &written);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptFinal failed");
				
	rc = EVP_CIPHER_CTX_ctrl(cctx.evp, EVP_CTRL_GCM_GET_TAG, MC_CRYPTNAME_PADDING, cctx.tag);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"CipherCTXCtrl failed");
	memcpy(buf.data()+buf.size()-MC_CRYPTNAME_PADDING,cctx.tag,MC_CRYPTNAME_PADDING);

	EVP_CIPHER_CTX_free(cctx.evp);

	out->assign(buf.toHex());
	return 0;
}
int crypt_decryptstring(mc_sync_ctx *ctx, const string& ivstr, const string& data, string *out){
	mc_crypt_ctx cctx;
	QByteArray buf;
	int rc,written;
	buf = QByteArray::fromHex(data.c_str());

	init_crypt_ctx(&cctx,ctx);
	memcpy(cctx.iv,buf.data(),MC_CRYPTNAME_OFFSET);
	if(memcmp(cctx.iv,MC_CRYPTNAME_IDENTIFIER,MC_CRYPTNAME_IDOFFSET) != 0){
			MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"Unrecognized cryptoname format: " << data);
	};
	MC_CHKERR(crypt_striv(&cctx,ivstr)); //the IV is not part of the string as it is implicit
		
	cctx.evp = EVP_CIPHER_CTX_new();
		
	rc = EVP_DecryptInit_ex(cctx.evp,EVP_aes_256_gcm(),NULL,cctx.ctx->sync->cryptkey,cctx.iv+MC_CRYPTNAME_IDOFFSET);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptInit failed");

	rc = EVP_DecryptUpdate(cctx.evp, (unsigned char*)buf.data()+MC_CRYPTNAME_OFFSET, &written, (unsigned char*)buf.data()+MC_CRYPTNAME_OFFSET, buf.size()-MC_CRYPTNAME_SIZEOVERHEAD);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptUpdate failed");

	memcpy(cctx.tag,buf.data()+buf.size()-MC_CRYPTNAME_PADDING,MC_CRYPTNAME_PADDING);
	rc = EVP_CIPHER_CTX_ctrl(cctx.evp,EVP_CTRL_GCM_SET_TAG,MC_CRYPTNAME_PADDING,cctx.tag);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"CipherCTXCtrl failed");

	rc = EVP_DecryptFinal_ex(cctx.evp,NULL,&written);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptFinal failed! This means the string was modified or you entered the wrong key.");

	EVP_CIPHER_CTX_free(cctx.evp);

	out->assign(buf.data()+MC_CRYPTNAME_OFFSET,buf.size()-MC_CRYPTNAME_SIZEOVERHEAD);
	return 0;
}

int crypt_filter_tosrv(mc_sync_ctx *ctx, const string& syncname, mc_filter *f){
	if(ctx->sync->crypted){
		return crypt_encryptstring(ctx,syncname,f->rule,&f->rule);
	}
	return 0;	
}
int crypt_filter_fromsrv(mc_sync_ctx *ctx, const string& syncname, mc_filter *f){
	if(ctx->sync->crypted){
		return crypt_decryptstring(ctx,syncname,f->rule,&f->rule);
	}
	return 0;
}
int crypt_filterlist_fromsrv(mc_sync_ctx *ctx, const string& syncname, list<mc_filter> *l){
	int rc;
	if(ctx->sync->crypted){
		for(mc_filter& f : *l){
			rc = crypt_filter_fromsrv(ctx,syncname,&f);
			MC_CHKERR(rc);
		}
	}
	return 0;
}

//Fields that cannot be mapped: id,parent,(ctime),mtime,is_dir,status
//Hash is not mapped because it must be encrypted locally too for encrypted syncs
//Fields that are to be mapped: name, size (crypt needs a few bytes more)

int crypt_file_tosrv(mc_sync_ctx *ctx, const string& path, mc_file *f){
	if(ctx->sync->crypted){
		if(f->cryptname == ""){
			return crypt_encryptstring(ctx,path,f->name,&f->cryptname);
		}
	} else f->cryptname = "";
	return 0;	
}
int crypt_file_fromsrv(mc_sync_ctx *ctx, const string& path, mc_file *f){
	int rc;
	if(ctx->sync->crypted){
		f->cryptname = f->name;
		rc = crypt_decryptstring(ctx,path,f->cryptname,&f->name);
		MC_CHKERR(rc);

		if(!f->is_dir) f->size -= MC_CRYPT_SIZEOVERHEAD;
	}
	return 0;
}
int crypt_filelist_fromsrv(mc_sync_ctx *ctx, const string& path, list<mc_file> *l){
	int rc;
	if(ctx->sync->crypted){
		for(mc_file& f : *l){
			rc = crypt_file_fromsrv(ctx,path,&f);
			MC_CHKERR(rc);
		}
	}
	return 0;
}


int crypt_keyring_fromsrv(string data, const string& password, list<mc_keyringentry> *l){
	unsigned int index = 0;
	int num,rc,written;
	mc_keyringentry item;
	unsigned char key[32];
	unsigned char salt[MC_CRYPTRING_SALTSIZE];
	unsigned char iv[MC_CRYPTRING_IVSIZE];
	unsigned char tag[MC_CRYPTRING_PADDING];
	QByteArray buf;
	EVP_CIPHER_CTX *evp;

	// extract salt + iv, genrate key
	if(data.length() == 0) return 0;
	if(memcmp(data.c_str(),MC_CRYPTRING_IDENTIFIER,MC_CRYPTRING_IDOFFSET) != 0){
			MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"Unrecognized cryptoname format: " << data);
	};
	buf = QByteArray(data.c_str(),data.length());
	
	memcpy(salt,buf.data()+MC_CRYPTRING_IDOFFSET,MC_CRYPTRING_SALTSIZE);
	memcpy(iv,buf.data()+MC_CRYPTRING_IDOFFSET+MC_CRYPTRING_SALTSIZE,MC_CRYPTRING_IVSIZE);

	rc = PKCS5_PBKDF2_HMAC_SHA1(password.c_str(),password.length(),(unsigned char*)salt,MC_CRYPTRING_SALTSIZE,MC_CRYPTRING_ITERCOUNT,32,key);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"PBKDF2 failed");
		
	// decrypt
	evp = EVP_CIPHER_CTX_new();
		
	rc = EVP_DecryptInit_ex(evp,EVP_aes_256_gcm(),NULL,key,iv);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptInit failed");

	rc = EVP_DecryptUpdate(evp, (unsigned char*)buf.data()+MC_CRYPTRING_OFFSET, &written, (unsigned char*)buf.data()+MC_CRYPTRING_OFFSET, buf.size()-MC_CRYPTRING_SIZEOVERHEAD);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptUpdate failed");

	memcpy(tag,buf.data()+buf.size()-MC_CRYPTRING_PADDING,MC_CRYPTRING_PADDING);
	rc = EVP_CIPHER_CTX_ctrl(evp,EVP_CTRL_GCM_SET_TAG,MC_CRYPTRING_PADDING,tag);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"CipherCTXCtrl failed");

	rc = EVP_DecryptFinal_ex(evp,NULL,&written);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptFinal failed! This means the keyring was modified or you entered the wrong password.");

	EVP_CIPHER_CTX_free(evp);

	data.assign(buf.data()+MC_CRYPTRING_OFFSET,buf.size()-MC_CRYPTRING_SIZEOVERHEAD);


	// parse from string
	try {
		while(index < data.length()){
			item.sid = (int)data.c_str()[index];
			index += sizeof(int);
			num = (int)data.c_str()[index];
			index += sizeof(int);
			item.sname.assign((char*)&data.c_str()[index],num);
			index += num;
			memcpy(item.key,(void*)&data.c_str()[index],32);
			index += 32;
			//memcpy(&num,(void*)data.c_str()[index],sizeof(int));
			l->push_back(item);
		}
		return 0;
	} catch (...) {
		return MC_ERR_PROTOCOL;
	}
}
int crypt_keyring_tosrv(list<mc_keyringentry> *l, const string& password, string *data){
	int num,rc,written;
	unsigned char key[32];
	unsigned char salt[MC_CRYPTRING_SALTSIZE];
	unsigned char iv[MC_CRYPTRING_IVSIZE];
	unsigned char tag[MC_CRYPTRING_PADDING];
	QByteArray buf;
	EVP_CIPHER_CTX *evp;


	// parse to string
	for(mc_keyringentry& item : *l){
		data->append((char*)&item.sid,sizeof(int));
		num = item.sname.length();
		data->append((char*)&num,sizeof(int));
		data->append(item.sname);
		data->append((char*)item.key,32);
	}

	// derive key from password
	rc = RAND_bytes(salt,MC_CRYPTRING_SALTSIZE);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"RAND_bytes failed");
	rc = RAND_bytes(iv,MC_CRYPTRING_IVSIZE);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"RAND_bytes failed");

	rc = PKCS5_PBKDF2_HMAC_SHA1(password.c_str(),password.length(),(unsigned char*)salt,MC_CRYPTRING_SALTSIZE,MC_CRYPTRING_ITERCOUNT,32,key);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"PBKDF2 failed");

	// encrypt string
	buf = QByteArray(data->length()+MC_CRYPTRING_SIZEOVERHEAD,'\0');

	memcpy(buf.data(),MC_CRYPTRING_IDENTIFIER,strlen(MC_CRYPTRING_IDENTIFIER));
	memcpy(buf.data()+MC_CRYPTRING_IDOFFSET,salt,MC_CRYPTRING_SALTSIZE);
	memcpy(buf.data()+MC_CRYPTRING_IDOFFSET+MC_CRYPTRING_SALTSIZE,iv,MC_CRYPTRING_IVSIZE);

	evp = EVP_CIPHER_CTX_new();

	rc = EVP_EncryptInit_ex(evp,EVP_aes_256_gcm(),NULL,key,iv);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptInit failed");
			
	rc = EVP_EncryptUpdate(evp, (unsigned char*)buf.data()+MC_CRYPTRING_OFFSET, &written, (unsigned char*)data->c_str(), data->length());
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptUpdate failed");

	rc = EVP_EncryptFinal_ex(evp, NULL, &written);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptFinal failed");
				
	rc = EVP_CIPHER_CTX_ctrl(evp, EVP_CTRL_GCM_GET_TAG, MC_CRYPTRING_PADDING, tag);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"CipherCTXCtrl failed");
	memcpy(buf.data()+buf.size()-MC_CRYPTRING_PADDING,tag,MC_CRYPTRING_PADDING);

	EVP_CIPHER_CTX_free(evp);

	data->assign(buf.constData(),buf.length());

	return 0;
}

void crypt_filestring(mc_sync_ctx *ctx, mc_file *f, string *s){
	s->append((const char*)&f->id,sizeof(int));
	if(f->cryptname == "")
		MC_WRN("empty cryptname hash");
	if(ctx->sync->crypted) s->append(f->cryptname); 
	else s->append(f->name);
	//s->append((const char*)&f->ctime,sizeof(int64)); //not a deciding/important criteria
	s->append((const char*)&f->mtime,sizeof(int64));
	if(ctx->sync->crypted && !f->is_dir) f->size += MC_CRYPT_SIZEOVERHEAD;
	s->append((const char*)&f->size,sizeof(int64));
	if(ctx->sync->crypted && !f->is_dir) f->size -= MC_CRYPT_SIZEOVERHEAD;
	s->append((const char*)&f->is_dir,sizeof(char));
	s->append((const char*)&f->status,sizeof(int));
	s->append((const char*)&f->hash,sizeof(unsigned char)*16);
}


int crypt_filemd5_actual(mc_crypt_ctx *cctx, size_t fsize, FILE *fdesc, unsigned char hash[16]){
	size_t bufsize;
	char* fbuf;
	QCryptographicHash cry(QCryptographicHash::Md5);
	int i,rc,written;

	cctx->evp = EVP_CIPHER_CTX_new();
	rc = EVP_EncryptInit_ex(cctx->evp,EVP_aes_256_gcm(),NULL,cctx->ctx->sync->cryptkey,cctx->iv+MC_CRYPT_IDOFFSET);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptInit failed");

	bufsize = choosebufsize(fsize);
	fbuf = (char*) malloc(bufsize);
	if(fbuf == 0) {
		bufsize = bufsize/10;
		fbuf = (char*) malloc(bufsize);
		MC_MEM(fbuf,bufsize);
	}

	cry.addData((char*)cctx->iv,MC_CRYPT_OFFSET);
	//Don't need to authenticate the IV
	//rc = EVP_EncryptUpdate(cctx->evp, NULL, &written, cctx->iv, MC_CRYPT_OFFSET);
	//if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptUpdate failed for AAD");
	
	for(i = 0; (i+1)*bufsize<fsize; i++){
		MC_NOTIFYIOSTART(MC_NT_FS);
		fread(fbuf,bufsize,1,fdesc);
		MC_NOTIFYIOEND(MC_NT_FS);
		
		rc = EVP_EncryptUpdate(cctx->evp, (unsigned char*)fbuf, &written, (unsigned char*)fbuf,bufsize);
		if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptUpdate failed");

		if(MC_TERMINATING()){
			free(fbuf);
			return MC_ERR_TERMINATING;
		}

		cry.addData(fbuf,bufsize);
	}

	memset(fbuf,0,bufsize);
	fread(fbuf,fsize-i*bufsize,1,fdesc);
	rc = EVP_EncryptUpdate(cctx->evp, (unsigned char*)fbuf, &written, (unsigned char*)fbuf,fsize-i*bufsize);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptUpdate failed");
	cry.addData(fbuf,fsize-i*bufsize);
	

	//Get TAG after encryption
	rc = EVP_EncryptFinal_ex(cctx->evp, NULL, &written);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptFinal failed");
	
	rc = EVP_CIPHER_CTX_ctrl(cctx->evp, EVP_CTRL_GCM_GET_TAG, MC_CRYPT_PADDING, cctx->tag);
	if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"CipherCTXCtrl failed");

	EVP_CIPHER_CTX_free(cctx->evp);

	//cctx->hastag = true; //tag might be based on old IV
	cry.addData((char*)cctx->tag,MC_CRYPT_PADDING);

	free(fbuf);

	QByteArray h = cry.result();
	memcpy(hash,h.constData(),sizeof(unsigned char)*16);

	return 0;
}

int crypt_filemd5_new(mc_crypt_ctx *cctx, unsigned char hash[16], const string& fpath, size_t fsize){
	if(cctx->ctx->sync->crypted){
		FILE *fdesc;
		int rc;
		MC_DBGL("Opening " << fpath);
		fdesc = fs_fopen(fpath, "rb");
		if(!fdesc) MC_ERR_MSG(MC_ERR_IO,"Can't open file");
	
		rc = crypt_filemd5_new(cctx,hash,fpath,fsize,fdesc);

		fs_fclose(fdesc);
		return rc;
	} else return fs_filemd5(hash,fpath,fsize);
}
int crypt_filemd5_new(mc_crypt_ctx *cctx, unsigned char hash[16], const string& fpath, size_t fsize, FILE *fdesc){
	if(cctx->ctx->sync->crypted){
		MC_DBGL("Calculating CryptMD5 of file " << fpath);
				
		memcpy(cctx->iv,MC_CRYPT_IDENTIFIER,strlen(MC_CRYPT_IDENTIFIER));
		crypt_seed(fpath);
		MC_CHKERR(crypt_randiv(cctx));
		cctx->hasiv = true;
		
		return crypt_filemd5_actual(cctx,fsize,fdesc,hash);
	} else return fs_filemd5(hash,fsize,fdesc);
}
int crypt_filemd5_known(mc_crypt_ctx *cctx, mc_file *file, unsigned char hash[16], const string& fpath){
	if(cctx->ctx->sync->crypted){
		FILE *fdesc;
		int rc;
		MC_DBGL("Opening " << fpath);
		fdesc = fs_fopen(fpath, "rb");
		if(!fdesc) MC_ERR_MSG(MC_ERR_IO,"Can't open file");
	
		rc = crypt_filemd5_known(cctx,file,hash,fpath,fdesc);
		MC_CHKERR_FD(rc,fdesc);

		fs_fclose(fdesc);
		return rc;
	} else return fs_filemd5(hash,fpath,file->size);
}
int crypt_filemd5_known(mc_crypt_ctx *cctx, mc_file *file, unsigned char hash[16], const string& fpath, FILE *fdesc){
	if(cctx->ctx->sync->crypted){
		int rc;
		MC_DBGL("Calculating CryptMD5 of file " << fpath);

		//For known we need the IV on the server
		rc = srv_getfile(file->id,0,MC_CRYPT_PADDING,(char*)cctx->iv,NULL,file->hash,false);
		MC_CHKERR(rc);
		//cctx->hasiv = true; //has old iv but that is not good for a new upload...
				
		rc = crypt_filemd5_actual(cctx,file->size,fdesc,hash);
		MC_CHKERR(rc);

		//if(memcmp(hash,file->hash,16) != 0){
		//	rc = crypt_filemd5_new(cctx,hash,fpath,file->size,fdesc);
		//}

		return rc;
	} else return fs_filemd5(hash,file->size,fdesc);
}


int crypt_init_download(mc_crypt_ctx *cctx, mc_file *file){
	int rc;
	cctx->f = file;
	if(cctx->ctx->sync->crypted){
		if(!file->is_dir){
			SetBuf(&cctx->pbuf,MC_RECVBLOCKSIZE);
			cctx->evp = EVP_CIPHER_CTX_new();

			if(cctx->hasiv){
				rc = EVP_DecryptInit_ex(cctx->evp,EVP_aes_256_gcm(),NULL,cctx->ctx->sync->cryptkey,cctx->iv+MC_CRYPT_IDOFFSET);
				if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptInit failed");
				//Don't need to authenticate the IV
				//rc = EVP_DecryptUpdate(cctx->evp,NULL,&written,cctx->iv,MC_CRYPT_PADDING);
				//if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptUpdate failed for AAD");
			}
		}
	}
	return 0;
}

int crypt_initresume_down(mc_crypt_ctx *cctx, mc_file *file){
	int rc;
	cctx->f = file;
	if(cctx->ctx->sync->crypted){
		//if(!file->is_dir){ //resume only on files
			SetBuf(&cctx->pbuf,MC_RECVBLOCKSIZE);
			cctx->evp = EVP_CIPHER_CTX_new();
			
			if(!cctx->hasiv){
				rc = srv_getfile(file->id,0,MC_CRYPT_OFFSET,(char*)cctx->iv,NULL,file->hash,false);
				MC_CHKERR(rc);
				cctx->hasiv = true;
			}
			
			rc = EVP_DecryptInit_ex(cctx->evp,EVP_aes_256_gcm(),NULL,cctx->ctx->sync->cryptkey,cctx->iv+MC_CRYPT_IDOFFSET);
			if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptInit failed");
		//}
	}
	return 0;
}

int crypt_resumetopos_down(mc_crypt_ctx *cctx, mc_file *file, int64 offset, FILE *fdesc){
	int rc,blocksize,written;
	int64 pos,read;
	EVP_CIPHER_CTX *eevp = EVP_CIPHER_CTX_new();
	if(cctx->ctx->sync->crypted){
		cctx->f = file;
		MatchBuf(&cctx->pbuf,MC_RECVBLOCKSIZE);
		fs_fseek(fdesc,0,SEEK_SET);		

		rc = EVP_EncryptInit_ex(eevp,EVP_aes_256_gcm(),NULL,cctx->ctx->sync->cryptkey,cctx->iv+MC_CRYPT_IDOFFSET);
		if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptInit failed");

		pos = 0;
		while(pos < offset){
			if(offset - pos >= MC_RECVBLOCKSIZE) blocksize = MC_RECVBLOCKSIZE;
			else blocksize = offset - pos;

			MC_NOTIFYIOSTART(MC_NT_FS);
			read = fread(cctx->pbuf.mem,blocksize,1,fdesc);
			MC_NOTIFYIOEND(MC_NT_FS);
			if(read != 1) MC_ERR_MSG(MC_ERR_IO,"Reading from file failed: " << ferror(fdesc));

			//Encrypt for "fake"
			rc = EVP_EncryptUpdate(eevp, (unsigned char*)cctx->pbuf.mem, &written, (unsigned char*)cctx->pbuf.mem, blocksize);
			if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptUpdate failed");
			cctx->pbuf.used = written;
			//Decrypt
			rc = EVP_DecryptUpdate(cctx->evp, (unsigned char*) cctx->pbuf.mem, &written, (unsigned char*) cctx->pbuf.mem, blocksize);
			if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptUpdate failed");
			pos += cctx->pbuf.used;

			MC_CHECKTERMINATING();
		}

		EVP_CIPHER_CTX_cleanup(eevp);
		EVP_CIPHER_CTX_free(eevp);

		fs_fseek(fdesc,0,SEEK_END);
	} else {
	}
	return 0;
}

int crypt_getfile(mc_crypt_ctx *cctx, int id, int64 offset, int64 blocksize, FILE *fdesc, int64 *byteswritten, unsigned char hash[16]){
	int rc,cryptwritten;
	int64 tagoffset = 0;
	int64 memwritten,write,writeoffset,written;
	if(cctx->ctx->sync->crypted){
		//if(!cctx->f->is_dir){ // getfile only on files
			if(!cctx->hasiv){
				if(offset == 0){ // get IV
					//MatchBuf(&cctx->pbuf,MC_RECVBLOCKSIZE);
					if(cctx->f->size - offset + MC_CRYPT_PADDING + MC_CRYPT_OFFSET <= blocksize){ //Get IV + TAG
						rc = srv_getfile(id,offset,blocksize,cctx->pbuf.mem,&memwritten,hash);
						MC_CHKERR(rc);

						memcpy(cctx->tag,cctx->pbuf.mem+memwritten-MC_CRYPT_PADDING,MC_CRYPT_PADDING);
						cctx->hastag = true;
						write = memwritten-MC_CRYPT_OFFSET-MC_CRYPT_PADDING;
					} else if(cctx->f->size - offset + MC_CRYPT_OFFSET <= blocksize){ //IV + Partial TAG
						tagoffset = blocksize - (cctx->f->size - offset + MC_CRYPT_OFFSET);
						rc = srv_getfile(id,offset,blocksize,cctx->pbuf.mem,&memwritten,hash);
						MC_CHKERR(rc);
						memcpy(cctx->tag,cctx->pbuf.mem+memwritten-tagoffset,tagoffset);
						offset += memwritten - MC_CRYPT_OFFSET + tagoffset;
						
						write = memwritten - MC_CRYPT_OFFSET - tagoffset;
						//Rest is only rest of TAG
						rc = srv_getfile(id,offset+MC_CRYPT_OFFSET,MC_CRYPT_PADDING-tagoffset,(char*)cctx->tag+tagoffset,&memwritten,hash);
						MC_CHKERR(rc);
						cctx->hastag = true;
					} else {//Only IV
						rc = srv_getfile(id,offset,blocksize,cctx->pbuf.mem,&memwritten,hash);
						MC_CHKERR(rc);
						write = memwritten-MC_CRYPT_OFFSET;
					}

					memcpy(cctx->iv,cctx->pbuf.mem,MC_CRYPT_OFFSET);								
					if(memcmp(cctx->iv,MC_CRYPT_IDENTIFIER,MC_CRYPT_IDOFFSET) != 0){
						MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"Unrecognized crypto format");
					};
					cctx->hasiv = true;		

					rc = EVP_DecryptInit_ex(cctx->evp,EVP_aes_256_gcm(),NULL,cctx->ctx->sync->cryptkey,cctx->iv+MC_CRYPT_IDOFFSET);
					if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptInit failed");
					
					//Don't need to authenticate the IV
					//rc = EVP_DecryptUpdate(cctx->evp,NULL,&cryptwritten,cctx->iv,MC_CRYPT_PADDING);
					//if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptUpdate failed for AAD");

					writeoffset = MC_CRYPT_PADDING;
				} else {
					MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"Should not happen");
				}
			} else {
				if(cctx->f->size - offset + MC_CRYPT_PADDING <= blocksize){ //Get TAG					
					rc = srv_getfile(id,offset+MC_CRYPT_OFFSET,blocksize,cctx->pbuf.mem,&memwritten,hash);
					MC_CHKERR(rc);

					memcpy(cctx->tag,cctx->pbuf.mem+memwritten-MC_CRYPT_PADDING,MC_CRYPT_PADDING);
					cctx->hastag = true;
					write = memwritten - MC_CRYPT_PADDING;					
				} else if(cctx->f->size - offset <= blocksize){ //Partial TAG
					tagoffset = blocksize - (cctx->f->size - offset);
					rc = srv_getfile(id,offset+MC_CRYPT_OFFSET,blocksize,cctx->pbuf.mem,&memwritten,hash);
					MC_CHKERR(rc);
					memcpy(cctx->tag,cctx->pbuf.mem+memwritten-tagoffset,tagoffset);
					offset += memwritten - MC_CRYPT_OFFSET + tagoffset;
					
					write = memwritten-tagoffset;
					//Rest is only rest of TAG
					rc = srv_getfile(id,offset+MC_CRYPT_OFFSET,MC_CRYPT_PADDING-tagoffset,(char*)cctx->tag+tagoffset,&memwritten,hash);
					MC_CHKERR(rc);
					cctx->hastag = true;
				} else {//No TAG
					rc = srv_getfile(id,offset+MC_CRYPT_OFFSET,blocksize,cctx->pbuf.mem,&memwritten,hash);
					MC_CHKERR(rc);
					write = memwritten;
				}
				
				writeoffset = 0;
			}
			
			//Decrypt
			rc = EVP_DecryptUpdate(cctx->evp, (unsigned char*) cctx->pbuf.mem+writeoffset, &cryptwritten, (unsigned char*) cctx->pbuf.mem+writeoffset, write);
			if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"DecryptUpdate failed");
			//write = cryptwritten; //is the same anyway

			//Write to file
			MC_NOTIFYIOSTART(MC_NT_FS);
			written = fwrite(cctx->pbuf.mem+writeoffset,write,1,fdesc);
			MC_NOTIFYIOEND(MC_NT_FS);
			if (written != 1) MC_ERR_MSG(MC_ERR_IO,"Writing to file failed: " << ferror(fdesc));
			*byteswritten = write;

			return 0;
		//} else return srv_getfile(id,offset+MC_CRYPT_OFFSET,fdesc,byteswritten,hash);
	} else return srv_getfile(id,offset,blocksize,fdesc,byteswritten,hash);
}

int crypt_finish_download(mc_crypt_ctx *cctx){
	int rc,written;
	if(cctx->ctx->sync->crypted){
		if(!cctx->f->is_dir && cctx->f->size > 0){

			if(!cctx->hastag) MC_ERR_MSG(MC_ERR_NOT_IMPLEMENTED,"This should not happen, must have called getfile");
			rc = EVP_CIPHER_CTX_ctrl(cctx->evp,EVP_CTRL_GCM_SET_TAG,MC_CRYPT_PADDING,cctx->tag);
			if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"CipherCTXCtrl failed");

			rc = EVP_DecryptFinal_ex(cctx->evp,NULL,&written);
			if(!rc) 
				MC_ERR_MSG(MC_ERR_CRYPTOALERT,"DecryptFinal failed! This means the file was modified by someone without the key!");
			//TODO: rename file and tell user etc.

			EVP_CIPHER_CTX_cleanup(cctx->evp);
			EVP_CIPHER_CTX_free(cctx->evp);
			FreeBuf(&cctx->pbuf);
		}
	}
	return 0;
}

void crypt_abort_download(mc_crypt_ctx *cctx){
	if(cctx->ctx->sync->crypted){
		if(!cctx->f->is_dir && cctx->f->size > 0){
			EVP_CIPHER_CTX_cleanup(cctx->evp);
			EVP_CIPHER_CTX_free(cctx->evp);
			FreeBuf(&cctx->pbuf);
		}
	}
}



int crypt_init_upload(mc_crypt_ctx *cctx, mc_file *file){
	int rc;
	if(cctx->ctx->sync->crypted){
		if(!file->is_dir){
			SetBuf(&cctx->pbuf,MC_SENDBLOCKSIZE);
			if(!cctx->hasiv){ 
				memcpy(cctx->iv,MC_CRYPT_IDENTIFIER,strlen(MC_CRYPT_IDENTIFIER));
				MC_CHKERR(crypt_randiv(cctx));
				cctx->hasiv = true;
			}
			cctx->evp = EVP_CIPHER_CTX_new();
			rc = EVP_EncryptInit_ex(cctx->evp,EVP_aes_256_gcm(),NULL,cctx->ctx->sync->cryptkey,cctx->iv+MC_CRYPT_IDOFFSET);
			if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptInit failed");
		}
	}
	return 0;
}

int crypt_initresume_up(mc_crypt_ctx *cctx, mc_file *file, int64 *offset){
	int rc;
	if(cctx->ctx->sync->crypted){
		//if(!file->is_dir){//resume only on files
			SetBuf(&cctx->pbuf,MC_SENDBLOCKSIZE);
			if(!cctx->hasiv){
				rc = srv_getfile(file->id,0,MC_CRYPT_OFFSET,(char*)cctx->iv,NULL,file->hash,false);
				MC_CHKERR(rc);
				cctx->hasiv = true;
			}
			cctx->evp = EVP_CIPHER_CTX_new();
			rc = EVP_EncryptInit_ex(cctx->evp,EVP_aes_256_gcm(),NULL,cctx->ctx->sync->cryptkey,cctx->iv+MC_CRYPT_IDOFFSET);
			if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptInit failed");

			rc = srv_getoffset(file->id,offset);
			if(*offset - MC_CRYPT_OFFSET > file->size) //if offset behind file (=Padding of nothing missing)
				*offset = file->size; // tell caller the server has everything
			else *offset -= MC_CRYPT_OFFSET;

			return rc;
		//}
	} else return srv_getoffset(file->id,offset);
	return 0;
}

int crypt_resumetopos_up(mc_crypt_ctx *cctx, mc_file *file, int64 offset, FILE *fdesc){
	int rc,written;
	int64 bs,read,pos = 0;
	if(cctx->ctx->sync->crypted){
		cctx->f = file;
		MatchBuf(&cctx->pbuf,MC_SENDBLOCKSIZE);
		fs_fseek(fdesc,0,SEEK_SET);


		while(pos < offset){
			if(offset-pos >= MC_SENDBLOCKSIZE) bs = MC_SENDBLOCKSIZE;
			else bs = offset-pos;
			MC_NOTIFYIOSTART(MC_NT_FS);
			read = fread(cctx->pbuf.mem,bs,1,fdesc);
			MC_NOTIFYIOEND(MC_NT_FS);
			if(read != 1) 
				MC_ERR_MSG(MC_ERR_IO,"Reading from file failed: " << ferror(fdesc));
			//cctx->pbuf.used += blocksize;

			//Crypted data to cryptbuf
			rc = EVP_EncryptUpdate(cctx->evp, (unsigned char*)cctx->pbuf.mem, &written, (unsigned char*)cctx->pbuf.mem, bs);
			cctx->pbuf.used = written;
			if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptUpdate failed");
			pos += cctx->pbuf.used;

			MC_CHECKTERMINATING();
		}
	} else {
		fs_fseek(fdesc,offset,SEEK_SET);
	}
	return 0;
}

int crypt_putfile(mc_crypt_ctx *cctx, const string& path, mc_file *file, int64 blocksize, FILE *fdesc){
	int rc,written;
	size_t read;
	string clearname;
	if(cctx->ctx->sync->crypted){
		cctx->f = file;
		crypt_file_tosrv(cctx->ctx,path,file);
		clearname = file->name;
		file->name = file->cryptname;
		if(!file->is_dir){
			
			MatchBuf(&cctx->pbuf,MC_CRYPT_OFFSET+blocksize);
			cctx->pbuf.used = 0;
			
			file->size += MC_CRYPT_SIZEOVERHEAD;
			memcpy(cctx->pbuf.mem,cctx->iv,MC_CRYPT_OFFSET);
			cctx->pbuf.used += MC_CRYPT_OFFSET;

			//Don't need to authenticate the IV
			//rc = EVP_EncryptUpdate(cctx->evp, NULL, &written, cctx->iv, MC_CRYPT_OFFSET);
			//if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptUpdate failed for AAD");

			//Encrypt
			if(blocksize > 0){
				MC_NOTIFYIOSTART(MC_NT_FS);
				read = fread(cctx->pbuf.mem+cctx->pbuf.used,blocksize,1,fdesc);
				MC_NOTIFYIOEND(MC_NT_FS);
				if(read != 1) MC_ERR_MSG(MC_ERR_IO,"Reading from file failed: " << ferror(fdesc));
				//cctx->pbuf.used += blocksize;

				//Crypted data to cryptbuf
				rc = EVP_EncryptUpdate(cctx->evp, (unsigned char*)cctx->pbuf.mem+cctx->pbuf.used, &written, (unsigned char*)cctx->pbuf.mem+cctx->pbuf.used, blocksize);
				cctx->pbuf.used += written;
				if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptUpdate failed");
			}


			// Append TAG
			if(file->size == blocksize+MC_CRYPT_SIZEOVERHEAD){ // this is the last block				
				rc = EVP_EncryptFinal_ex(cctx->evp, NULL, &written);
				if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptFinal failed");
				
				rc = EVP_CIPHER_CTX_ctrl(cctx->evp, EVP_CTRL_GCM_GET_TAG, MC_CRYPT_PADDING, cctx->tag);
				if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"CipherCTXCtrl failed");
				//if(cctx->hastag) //TODO: compare hashing-tag with Encrpytion tag - should match

				MatchBuf(&cctx->pbuf,MC_CRYPT_OFFSET+blocksize+MC_CRYPT_PADDING);
				memcpy(cctx->pbuf.mem+cctx->pbuf.used,cctx->tag,MC_CRYPT_PADDING);
				cctx->pbuf.used += MC_CRYPT_PADDING;
				cctx->hastag = true;

				rc = srv_putfile(file,cctx->pbuf.used,cctx->pbuf.mem);
			} else rc = srv_putfile(file,cctx->pbuf.used,cctx->pbuf.mem);
			file->size -= MC_CRYPT_SIZEOVERHEAD;
		} else rc = srv_putfile(file,blocksize,fdesc);
		file->name = clearname;
		return rc;
	} else return srv_putfile(file,blocksize,fdesc);

}

int crypt_addfile(mc_crypt_ctx *cctx, int id, int64 offset, int64 blocksize, FILE *fdesc, unsigned char hash[16]){
	int rc,written;
	size_t read;
	if(cctx->ctx->sync->crypted){
		//if(!cctx->f->is_dir){ //addfile only on files
			MatchBuf(&cctx->pbuf,blocksize);
			cctx->pbuf.used = 0;
			//Encrypt
			if(blocksize > 0){
				MC_NOTIFYIOSTART(MC_NT_FS);
				read = fread(cctx->pbuf.mem+cctx->pbuf.used,blocksize,1,fdesc);
				MC_NOTIFYIOEND(MC_NT_FS);
				if(read != 1) MC_ERR_MSG(MC_ERR_IO,"Reading from file failed: " << ferror(fdesc));
				//cctx->pbuf.used += blocksize;

				//Crypted data to cryptbuf
				rc = EVP_EncryptUpdate(cctx->evp, (unsigned char*)cctx->pbuf.mem+cctx->pbuf.used, &written, (unsigned char*)cctx->pbuf.mem+cctx->pbuf.used, blocksize);
				cctx->pbuf.used += written;
				if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptUpdate failed");
			}
			// Append TAG
			if(blocksize == cctx->f->size - offset){ //Last Part
				rc = EVP_EncryptFinal_ex(cctx->evp, NULL, &written);
				if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"EncryptFinal failed");
				
				rc = EVP_CIPHER_CTX_ctrl(cctx->evp, EVP_CTRL_GCM_GET_TAG, MC_CRYPT_PADDING, cctx->tag);
				if(!rc) MC_ERR_MSG(MC_ERR_CRYPTO,"CipherCTXCtrl failed");
				//if(cctx->hastag) //TODO: compare hashing-tag with Encrpytion tag - should match

				MatchBuf(&cctx->pbuf,MC_CRYPT_OFFSET+blocksize+MC_CRYPT_PADDING);
				memcpy(cctx->pbuf.mem+cctx->pbuf.used,cctx->tag,MC_CRYPT_PADDING);
				cctx->pbuf.used += MC_CRYPT_PADDING;
				cctx->hastag = true;

				return srv_addfile(id,offset+MC_CRYPT_OFFSET,cctx->pbuf.used,cctx->pbuf.mem,hash);
			} else return srv_addfile(id,offset+MC_CRYPT_OFFSET,cctx->pbuf.used,cctx->pbuf.mem,hash);
		//} else return srv_addfile(id,offset+MC_CRYPT_OFFSET,blocksize,fdesc,hash);
	} else return srv_addfile(id,offset,blocksize,fdesc,hash);
}

int crypt_patchfile(mc_crypt_ctx *cctx, const string& path, mc_file *file){
	int rc;
	string clearname;
	if(cctx->ctx->sync->crypted){
		crypt_file_tosrv(cctx->ctx,path,file);
		if(file->is_dir) file->size += MC_CRYPT_SIZEOVERHEAD;
		clearname = file->name;
		file->name = file->cryptname;
		rc = srv_patchfile(file);
		file->name = clearname;
		if(file->is_dir) file->size -= MC_CRYPT_SIZEOVERHEAD;
		return rc;
	} else return srv_patchfile(file);
}

int crypt_finish_upload(mc_crypt_ctx *cctx){
	if(cctx->ctx->sync->crypted){
		if(!cctx->f->is_dir){
			EVP_CIPHER_CTX_cleanup(cctx->evp);
			EVP_CIPHER_CTX_free(cctx->evp);
			FreeBuf(&cctx->pbuf);
		}
	}
	return 0;
}

void crypt_abort_upload(mc_crypt_ctx *cctx){
	if(cctx->ctx->sync->crypted){
		if(!cctx->f->is_dir){
			EVP_CIPHER_CTX_cleanup(cctx->evp);
			EVP_CIPHER_CTX_free(cctx->evp);
			FreeBuf(&cctx->pbuf);
		}
	}
}