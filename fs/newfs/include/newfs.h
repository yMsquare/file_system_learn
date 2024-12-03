#ifndef _NEWFS_H_
#define _NEWFS_H_

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include <unistd.h>
#include "fcntl.h"
#include "fuse.h"
#include <stddef.h>
#include "ddriver.h"
#include "errno.h"
#include "types.h"
#include "stdint.h"
#include "string.h"
#include "stdlib.h"


/******************************************************************************
* SECTION: newfs.c
*******************************************************************************/
void* 			   newfs_init(struct fuse_conn_info *);
void  			   newfs_destroy(void *);
int   			   newfs_mkdir(const char *, mode_t);
int   			   newfs_getattr(const char *, struct stat *);
int   			   newfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
						                struct fuse_file_info *);
int   			   newfs_mknod(const char *, mode_t, dev_t);
int   			   newfs_write(const char *, const char *, size_t, off_t,
					                  struct fuse_file_info *);
int   			   newfs_read(const char *, char *, size_t, off_t,
					                 struct fuse_file_info *);
int   			   newfs_access(const char *, int);
int   			   newfs_unlink(const char *);
int   			   newfs_rmdir(const char *);
int   			   newfs_rename(const char *, const char *);
int   			   newfs_utimens(const char *, const struct timespec tv[2]);
int   			   newfs_truncate(const char *, off_t);
			
int   			   newfs_open(const char *, struct fuse_file_info *);
int   			   newfs_opendir(const char *, struct fuse_file_info *);
/******************************************************************************
* SECTION: newfs_util.c
*******************************************************************************/
struct newfs_inode*  allocate_inode(struct newfs_dentry * dentry);
int allocate_dentry(struct newfs_inode* inode, struct newfs_dentry* dentry);
int newfs_driver_read(int offset, uint8_t *out_content, int size);
int newfs_driver_write(int offset, uint8_t *out_content, int size);
int sync_inode(struct newfs_inode * inode);
struct newfs_dentry* lookup(const char * path, boolean* is_find, boolean * is_root);
struct newfs_inode* read_inode(struct newfs_dentry * dentry, int ino);
char* get_fname(const char * path);
struct newfs_dentry* get_dentry(struct newfs_inode * inode, int dir);
void dump_map();
void allocate_data(struct newfs_inode *inode);
#endif  /* _newfs_H_ */