#ifndef _TYPES_H_
#define _TYPES_H_

#include <sys/types.h>
#define MAX_NAME_LEN    128     
#include <stdint.h>
typedef int          boolean;
typedef uint16_t     flag16;
#include "string.h"
#include "stdlib.h"
/******************************************************************************
* SECTION: Macro
*******************************************************************************/
#define TRUE                    1
#define FALSE                   0
#define UINT32_BITS             32
#define UINT8_BITS              8

#define ERROR_NONE          0
#define ERROR_ACCESS        EACCES
#define ERROR_SEEK          ESPIPE     
#define ERROR_ISDIR         EISDIR
#define ERROR_NOSPACE       ENOSPC
#define ERROR_EXISTS        EEXIST
#define ERROR_NOTFOUND      ENOENT
#define ERROR_UNSUPPORTED   ENXIO
#define ERROR_IO            EIO     /* Error Input/Output */
#define ERROR_INVAL         EINVAL  /* Invalid Args */
/******************************************************************************
* SECTION: Macro Function
*******************************************************************************/
// #define SFS_IO_SZ()                     (sfs_super.sz_io)
// #define SFS_DISK_SZ()                   (sfs_super.sz_disk)
// #define SFS_DRIVER()                    (sfs_super.driver_fd)

// #define SFS_ROUND_DOWN(value, round)    ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
// #define SFS_ROUND_UP(value, round)      ((value) % (round) == 0 ? (value) : ((value) / (round) + 1) * (round))

#define BLKS_SZ(blks)               ((blks) * IO_SZ()*2) // logic block size 
// #define SFS_ASSIGN_FNAME(psfs_dentry, _fname)\ 
                                        // memcpy(psfs_dentry->fname, _fname, strlen(_fname))
#define INO_OFS(ino)                (super.sz_io * 2 + BLKS_SZ(super.map_inode_blks) + BLKS_SZ(super.map_data_blks)\
                                    + ino*super.sz_io*2)
 
#define DATA_OFS(ino)               (INO_OFS(ino) + BLKS_SZ(INODE_PER_FILE))

#define IS_DIR(pinode)              (pinode->dentry->file_type == NFS_DIR)
#define IS_REG(pinode)              (pinode->dentry->file_type == NFS_REG_FILE)
// #define IS_SYM_LINK(pinode)         (pinode->dentry->ftype == SFS_SYM_LINK)




typedef enum newfs_file_type {
    NFS_REG_FILE,
    NFS_DIR,
    NFS_SYM_LINK
} NFS_FILE_TYPE;  // 3B

struct custom_options {
	const char*        device;
};

struct newfs_super {
    // uint     magic;
    int      driver_fd;         // driver_fd

    struct newfs_inode*      root_dentry_inode;//根目录索引
    // 分区布局信息
    int      max_blks;          // 最大逻辑块数量
    
    int      max_ino;           
    uint8_t*     map_inode;         // inode 位图 指针
    int      map_inode_blks;    // inode 位图 估算块数
    int      map_inode_offset;  // inode 位图 位置

    int      max_data;          // 最大可用数据块数
    uint8_t*     map_data;         // 
    int     map_data_blks;    // data 位图 估算块数
    int     map_data_offset;    // data 位图 位置

    int    data_offset; 

    int     sz_disk;            // 磁盘总大小
    int     sz_io;              // IO 块大小
    int     sz_usage; // 磁盘使用量

    boolean is_mounted; // 已挂载
    struct newfs_dentry* root_dentry; // 根目录指针
};

struct newfs_inode {
    int      ino;
    // file infos
    int      file_size;
    int      link;  //
    int      dir_dentry_cnt;    //若文件为目录，下面有几个目录项
    struct newfs_dentry* dentry; // dentry , 也就是 first_child
    struct newfs_dentry* dentries;  // 所有 dentry

    // pointer to data block
    uint8_t*      data_block_pointer;
    //int       data_block_pointer[6];

    NFS_FILE_TYPE file_type;

    // other infos
    /*      in-mem      */
};

struct newfs_dentry {
    char     name[MAX_NAME_LEN];
    int      ino;
    struct newfs_inode* inode;
    NFS_FILE_TYPE   file_type;

    struct newfs_dentry* parent;
    struct newfs_dentry* brother;
    struct newfs_dentry* child;

};


// in-disk
struct newfs_dentry_d{
    char    name[MAX_NAME_LEN];
    int     ino;    // inode 索引
    NFS_FILE_TYPE   file_type; 
};

struct newfs_inode_d{
   uint      ino;
   uint      size;
    // file infos
    int      file_size;
    int      link;  //
    NFS_FILE_TYPE file_type;

    // pointer to data block
    int      block_pointer[6];

    // other infos
    int      dir_dentry_cnt;    // 
};

struct newfs_super_d{
    uint     magic;
    uint     sz_usage;

    uint      fd;
    uint      logic_blk_size;//逻辑块大小
    uint      root_dentry_inode;//根目录索引
    // 分区布局信息
    uint      max_inode;
    uint      map_inode_blks;
    uint      map_inode_offset; // inode 位图 offset
    uint      data_offset; // 数据 offset , 也就是位图之后的    
    uint      map_data_blks;
    uint      map_data_offset; // 数据位图offset
};

static inline struct newfs_dentry* new_dentry(char * fname, NFS_FILE_TYPE ftype) {
    struct newfs_dentry * dentry = (struct newfs_dentry *)malloc(sizeof(struct newfs_dentry));
    memset(dentry, 0, sizeof(struct newfs_dentry));
    memcpy(dentry->name, fname, strlen(fname));
    dentry->file_type = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL;
    dentry->child   = NULL;
}

#endif /* _TYPES_H_ */






