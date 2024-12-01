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

#define NFS_ERROR_NONE          0
#define NFS_ERROR_ACCESS        EACCES
#define NFS_ERROR_SEEK          ESPIPE     
#define NFS_ERROR_ISDIR         EISDIR
#define NFS_ERROR_NOSPACE       ENOSPC
#define NFS_ERROR_EXISTS        EEXIST
#define NFS_ERROR_NOTFOUND      ENOENT
#define NFS_ERROR_UNSUPPORTED   ENXIO
#define NFS_ERROR_IO            EIO     /* Error Input/Output */
#define NFS_ERROR_INVAL         EINVAL  /* Invalid Args */

#define NFS_MAX_FILE_NAME       128
#define NFS_INODE_PER_FILE      1
#define NFS_DATA_PER_FILE       16
#define NFS_DEFAULT_PERM        0777

#define SFS_IOC_MAGIC           'S'
#define SFS_IOC_SEEK            _IO(SFS_IOC_MAGIC, 0)

#define SFS_FLAG_BUF_DIRTY      0x1
#define SFS_FLAG_BUF_OCCUPY     0x2
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
#define INO_OFS(ino)                (super.data_offset + ino * super.sz_io * 2)
 
#define DATA_OFS(ino)               (INO_OFS(ino) + BLKS_SZ(INODE_PER_FILE))

#define IS_DIR(pinode)              (pinode->dentry->file_type == NFS_DIR)
#define IS_REG(pinode)              (pinode->dentry->file_type == NFS_REG_FILE)
// #define IS_SYM_LINK(pinode)         (pinode->dentry->ftype == SFS_SYM_LINK)
/******************************************************************************
* SECTION: macro debug
*******************************************************************************/
#define NFS_DBG(fmt, ...) do { printf("NFS_DBG: " fmt, ##__VA_ARGS__); } while(0) 



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
    // 如果是这个inode对应的文件是一个目录，这里就是它的数据的目录项的指针？
    struct newfs_dentry* dentry; // 父 dentry， 从这个dentry可以找到当前的inode

    struct newfs_dentry* dentries;  // 子 dentry， 从当前inode可以找到这些dentries

    // pointer to data block
    // 如果这个inode对应的是一个文件，那么下面就是他的文件数据块指针。
    // uint8_t*      data_block_pointer;
    uint32_t      data_block_no[6];

    uint8_t*   data[6];      //数据内容
    uint8_t*    data_in_mem; // 


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
    // struct newfs_dentry* child;

};


// in-disk
struct newfs_dentry_d{
    char    name[MAX_NAME_LEN];
    int     ino;    // inode 索引,
    NFS_FILE_TYPE   file_type; 
};

struct newfs_inode_d{
   uint32_t      size;
   uint32_t      ino;
    // file infos
    uint32_t      file_size;
    uint32_t     link;  //
    NFS_FILE_TYPE file_type;

    // number to data block
    uint32_t      data_block_no[6];

    // other infos
    uint32_t      dir_dentry_cnt;    // 
};

struct newfs_super_d{
    uint32_t     magic;
    uint32_t     sz_usage;
    // uint32_t      fd;
    // 分区布局信息
    uint32_t      data_offset; // 数据 offset , 也就是位图之后的    
    uint32_t      max_inode;

    uint32_t      map_inode_blks;
    uint32_t      map_inode_offset; // inode 位图 offset
    uint32_t      map_data_blks;
    uint32_t      map_data_offset; // 数据位图offset

    uint32_t      root_dentry_inode;//根目录索引
    
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
    // dentry->child   = NULL;
}

#endif /* _TYPES_H_ */






