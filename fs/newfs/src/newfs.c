#include "ddriver.h"
#include "types.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#define _XOPEN_SOURCE 700

#include "newfs.h"

/******************************************************************************
* SECTION: 宏定义
*******************************************************************************/
#define OPTION(t, p)        { t, offsetof(struct custom_options, p), 1 }

/******************************************************************************
* SECTION: 全局变量
*******************************************************************************/
static const struct fuse_opt option_spec[] = {		/* 用于FUSE文件系统解析参数 */
	OPTION("--device=%s", device),
	FUSE_OPT_END
};

struct custom_options newfs_options;			 /* 全局选项 */
struct newfs_super super; 
/******************************************************************************
* SECTION: FUSE操作定义
*******************************************************************************/
static struct fuse_operations operations = {
	.init = newfs_init,						 /* mount文件系统 */		
	.destroy = newfs_destroy,				 /* umount文件系统 */
	.mkdir = newfs_mkdir,					 /* 建目录，mkdir */
	.getattr = newfs_getattr,				 /* 获取文件属性，类似stat，必须完成 */
	.readdir = newfs_readdir,				 /* 填充dentrys */
	.mknod = newfs_mknod,					 /* 创建文件，touch相关 */
	.write = newfs_write,								  	 /* 写入文件 */
	.read = newfs_read,								  	 /* 读文件 */
	.utimens = newfs_utimens,				 /* 修改时间，忽略，避免touch报错 */
	.truncate = newfs_truncate,						  		 /* 改变文件大小 */
	.unlink = NULL,							  		 /* 删除文件 */
	.rmdir	= NULL,							  		 /* 删除目录， rm -r */
	.rename = NULL,							  		 /* 重命名，mv */

	.open = newfs_open,							
	.opendir = NULL,
	.access = newfs_access
};
/******************************************************************************
* SECTION: 必做函数实现
*******************************************************************************/
/**
 * @brief 挂载（mount）文件系统
 * 
 * @param conn_info 可忽略，一些建立连接相关的信息 
 * @return void*
 */
void* newfs_init(struct fuse_conn_info * conn_info) {
	/* TODO: 在这里进行挂载 */
	// int		ret = NFS_ERROR_NONE;
	int		driver_fd; // 磁盘驱动
	struct	newfs_super_d super_d; // 磁盘超级块
	struct	newfs_dentry* root_dentry;  // 根目录 dentry
	struct	newfs_inode*	root_inode;  // 根目录的inode

	int logic_num;	// 逻辑块 总数量

	int inode_num;  // inode总数量
	int map_inode_blks; // inode 位图总数

	int data_num;		//数据块总数量
	int map_data_blks;  // 数据位图（为1）
	int map_data_blk_offset; 	// 数据位图的偏移

	int super_blks; // 超级块的位置
	boolean is_init = FALSE; // 初始化flag

	driver_fd = ddriver_open(newfs_options.device);
    printf("\n\n successfully opened \n\n");
    fflush(stdout);
	
	if(driver_fd < 0){
		printf("error\n");
		return NULL;
	}
	super.driver_fd = driver_fd;
	ddriver_ioctl(driver_fd, IOC_REQ_DEVICE_SIZE, &super.sz_disk);
	ddriver_ioctl(driver_fd, IOC_REQ_DEVICE_IO_SZ, &super.sz_io);

    root_dentry = new_dentry("/", NFS_DIR);

    if (newfs_driver_read(0, (uint8_t *)(&super_d), 
                        sizeof(struct newfs_super_d)) != 0) {
        printf("error!") ;
    }  

	// 如果没有初始化
	// 修改的是to-disk结构
	if(super_d.magic != NEWFS_MAGIC){
		// 初始化做什么工作
		// 估算磁盘布局信息
		// super_blks = SFS_ROUND_UP(sizeof(struct sfs_super_d), SFS_IO_SZ()) / SFS_IO_SZ();
		super_blks = ROUND_UP(sizeof(struct newfs_super_d),LOGIC_SZ() ) / LOGIC_SZ(); // super block 的位置

		logic_num = DISK_SZ() / LOGIC_SZ() ;// 总共的逻辑块数

		inode_num =  DISK_SZ() / ((INODE_PER_FILE + DATA_PER_FILE) * LOGIC_SZ()); // 不考虑其他，总共可以用这么多inode来表示整个磁盘
		// inode_num = 585
		map_inode_blks = 1;//ROUND_UP((ROUND_UP(inode_num, UINT32_BITS)),LOGIC_SZ())/ LOGIC_SZ(); // 基于上述，最多需要这么多个inode bitmap
		map_data_blks = 1;// data bitmap ROUND_UP((ROUND_UP(logic_num, UINT32_BITS)),LOGIC_SZ()) / LOGIC_SZ()
		// map_inode_blks = 1, map_data_blks = 1
		super.max_ino = (inode_num - map_inode_blks - super_blks - map_data_blks);// 考虑完超级块和位图所占的块之后，最多可以有这么多个inode
		// max_ino = 585 - 1 - 1 -1 = 582
		super.max_data =  logic_num - inode_num - map_inode_blks - map_data_blks - super_blks;
		// max_data = 4096 - 585 - 1 - 1 - 1 = 3507

		// super_d
		// inode 位图的偏移 // 第一个块为超级块 // 所以 inode 位图的偏移为一个超级块的大小，也就是一个逻辑块的大小。
		super_d.map_inode_offset = LOGIC_SZ(); 
		super_d.map_data_offset = super_d.map_inode_offset + LOGIC_SZ(); // data 位图位于 inode 位图的下一个块
		super_d.inode_offset = super_d.map_data_offset + LOGIC_SZ();//inode 开始位置位于map_data的后方
		// inode 使用了 max_ino (582)个块。
		super_d.data_offset = super_d.inode_offset + super.max_ino * LOGIC_SZ(); // 所有数据块的开始，在 inode 区域的后面
		// 清零索引节点和数据块位图
		super_d.map_inode_blks = map_inode_blks;
		super_d.map_data_blks = map_data_blks;  // map_data 只需要1个块
		super_d.sz_usage = 0;//初次挂载
		
		is_init = TRUE;
        }
		super.sz_usage = super_d.sz_usage;

        super.map_inode = (uint8_t *) malloc(LOGIC_SZ()); // 分配一个 inode 位图块 sizeof((super_d.map_inode_blks)*super.sz_io)*2
		super.map_inode_blks = super_d.map_inode_blks;
		super.map_inode_offset = super_d.map_inode_offset; // inode 位图的偏移	
		super.inode_offset = super_d.inode_offset; 	// inode 的偏移
	
		super.map_data = (uint8_t *) malloc(LOGIC_SZ()); // 分配一个 data 位图块
		super.map_data_blks = super_d.map_data_blks;
		super.map_data_offset = super_d.map_data_offset;// data 位图的偏移
		super.data_offset = super_d.data_offset;
	
		printf("\n--------------------------------------------------------------------------------\n\n");
		// 尝试从磁盘中读取 inode 位图块
		NFS_DBG("reading inode map\n");
		if (newfs_driver_read(super_d.map_inode_offset, (uint8_t*)(super.map_inode), LOGIC_SZ()) != 0 ){
			NFS_DBG("---- error reading inode map");
		}

		// 尝试从磁盘中读取 data 位图块
		NFS_DBG("reading data map\n");
		if (newfs_driver_read(super_d.map_data_offset, (uint8_t*)(super.map_data), LOGIC_SZ()) != 0 ){
			NFS_DBG("---- error reading data map");
		}

		NFS_DBG("\n--is_init: %d\n", is_init);	
		// 如果这次需要初始化
		if (is_init){
			NFS_DBG("\n--- initialized\n");
			root_inode = allocate_inode(root_dentry);
			NFS_DBG("--- in initiallize : root inode : %s",root_inode->dentry->name);
			sync_inode(root_inode);// todo
		}

		root_inode = read_inode(root_dentry,0);
		NFS_DBG("---finished reading root inode : %s",root_inode->dentry->name);	
		root_dentry->inode = root_inode;
		root_dentry->ino = root_inode->ino;

		super.root_dentry = root_dentry;
		super.root_dentry_inode = root_inode;
		super.is_mounted  = TRUE;
	

		printf("\n\n successfully mounted \n\n");
		fflush(stdout);

		dump_map();

	return NFS_ERROR_NONE;
}

/**
 * @brief 卸载（umount）文件系统
 * 
 * @param p 可忽略
 * @return void
 */
void newfs_destroy(void* p) {
	/* TODO: 在这里进行卸载 */
	struct newfs_super_d super_d;

	if(!super.is_mounted){
		NFS_DBG("\n-----not mounted");
		return;
	}
	sync_inode(super.root_dentry->inode);
	NFS_DBG("\n-----sync_inode");

	super_d.magic = NEWFS_MAGIC;
	super_d.map_inode_blks = super.map_inode_blks;
	super_d.map_inode_offset = super.map_inode_offset;
    super_d.data_offset = super.data_offset; // 数据 offset     
    super_d.map_data_offset = super.map_data_offset; // 数据位图offset
	super_d.inode_offset = super.inode_offset;// inode 开始位置
	super_d.sz_usage = super.sz_usage;
	super_d.max_inode = super.max_ino;
	// super_d.root_dentry_inode = super.root_dentry_inode;
	super_d.map_data_blks = super.map_data_blks;
	super_d.map_inode_blks = super.map_inode_blks;

    NFS_DBG("-------magic : %x",super_d.magic);

NFS_DBG("\n newfs_driver_write in destroy\n");
	if(newfs_driver_write(0, (uint8_t *)&super_d, sizeof(struct newfs_super_d))!= 0){
		NFS_DBG("-------error writing back super_d");
		return ;
	}		
	NFS_DBG("\n\n ------------ map_inode_offset : %d  ", super.map_inode_offset);
	if(newfs_driver_write(super.map_inode_offset, (uint8_t *)(super.map_inode), LOGIC_SZ())!=0){
		NFS_DBG("-------error writing back map_inode");
		return ;
	}
	NFS_DBG("\n\n ------------ map_data_offset : %d  ", super.map_data_offset);
	if(newfs_driver_write(super.map_data_offset, (uint8_t *)(super.map_data), LOGIC_SZ())!=0){
		NFS_DBG("-------error writing back map_inode");
		return ;
	}
	free(super.map_inode);
	free(super.map_data);
	ddriver_close(super.driver_fd);
	return;
}

/**
 * @brief 创建目录
 * 
 * @param path 相对于挂载点的路径
 * @param mode 创建模式（只读？只写？），可忽略
 * @return int 0成功，否则返回对应错误号
 */
int newfs_mkdir(const char* path, mode_t mode) {
	/* TODO: 解析路径，创建目录 */
	boolean is_find , is_root;
	char* fname ;
	struct newfs_dentry* last_dentry = lookup(path, &is_find, &is_root);
	struct newfs_dentry* dentry;
	struct newfs_inode* inode;

	if(is_find){
		return -NFS_ERROR_EXISTS;
	}
	if(last_dentry->file_type == NFS_REG_FILE){
		return -NFS_ERROR_UNSUPPORTED;;
	}
	fname = get_fname(path);
	dentry = new_dentry(fname, NFS_DIR);

	inode = allocate_inode(dentry);

	dentry->parent = last_dentry;
	dentry->brother = NULL;

	allocate_dentry(last_dentry->inode, dentry);
	NFS_DBG("\n [%s] allocated dentry\nfather:%s,child:%s", __func__, last_dentry->name, dentry->name);

	return 0;
}

/**
 * @brief 获取文件或目录的属性，该函数非常重要
 * 
 * @param path 相对于挂载点的路径
 * @param newfs_stat 返回状态
 * @return int 0成功，否则返回对应错误号
 */
int newfs_getattr(const char* path, struct stat * newfs_stat) {
	/* TODO: 解析路径，获取Inode，填充newfs_stat，可参考/fs/simplefs/sfs.c的sfs_getattr()函数实现 */
	boolean is_find, is_root;
	// debug
	NFS_DBG("\n---getattr %s\n",path);
	
	struct newfs_dentry * dentry = lookup(path, &is_find, &is_root);
	if(is_find == FALSE){
		return -NFS_ERROR_NOTFOUND;
	}
	if(dentry->file_type == NFS_DIR){
		newfs_stat->st_mode = S_IFDIR | NEWFS_DEFAULT_PERM;
		newfs_stat->st_size = dentry->inode->dir_dentry_cnt * sizeof(struct newfs_dentry_d);
	}
	else if(dentry->file_type == NFS_REG_FILE){
		newfs_stat->st_mode = S_IFREG | NEWFS_DEFAULT_PERM;
		newfs_stat->st_size = dentry->inode->file_size;
	}
	newfs_stat->st_uid = getuid();
	newfs_stat->st_gid = getgid();
	newfs_stat->st_atime = time(NULL);
	newfs_stat->st_mtime = time(NULL);
	newfs_stat->st_blksize = IO_SZ() * 2;

	if(is_root){
		NFS_DBG("\n---get attr: is root\n");
		newfs_stat->st_size = super.sz_usage;
		newfs_stat->st_blocks = super.max_blks;
	}
	return NFS_ERROR_NONE;
}

/**
 * @brief 遍历目录项，填充至buf，并交给FUSE输出
 * 
 * @param path 相对于挂载点的路径
 * @param buf 输出buffer
 * @param filler 参数讲解:
 * 
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *				const struct stat *stbuf, off_t off)
 * buf: name会被复制到buf中
 * name: dentry名字
 * stbuf: 文件状态，可忽略
 * off: 下一次offset从哪里开始，这里可以理解为第几个dentry
 * 
 * @param offset 第几个目录项？
 * @param fi 可忽略
 * @return int 0成功，否则返回对应错误号
 */
int newfs_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
			    		 struct fuse_file_info * fi) {
    /* TODO: 解析路径，获取目录的Inode，并读取目录项，利用filler填充到buf，可参考/fs/simplefs/sfs.c的sfs_readdir()函数实现 */
	boolean is_find, is_root;
	int cur_dir = offset;
	struct newfs_dentry * dentry = lookup(path, &is_find, &is_root);
	struct newfs_dentry * sub_dentry;
	struct newfs_inode  * inode;
	if(is_find){
		inode = dentry->inode;
		sub_dentry = get_dentry(inode, cur_dir);
		if(sub_dentry){
			filler(buf, sub_dentry->name, NULL, ++offset);
		}
		return 0;
	}
	printf("read dir not found");
    return -NFS_ERROR_NOTFOUND;
}

/**
 * @brief 创建文件
 * 
 * @param path 相对于挂载点的路径
 * @param mode 创建文件的模式，可忽略
 * @param dev 设备类型，可忽略
 * @return int 0成功，否则返回对应错误号
 */
int newfs_mknod(const char* path, mode_t mode, dev_t dev) {
	/* TODO: 解析路径，并创建相应的文件 */
	boolean is_find, is_root;

	struct newfs_dentry * last_dentry = lookup(path, &is_find, &is_root);
	struct newfs_dentry * dentry;
	struct newfs_inode * inode;
	char * fname;

	if(is_find == TRUE){
		return -NFS_ERROR_EXISTS;
	}

	fname = get_fname(path);

	if(S_ISREG(mode)){
		dentry = new_dentry(fname, NFS_REG_FILE);
	}
	else if( S_ISDIR(mode)){
		dentry = new_dentry(fname, NFS_DIR);
	}
	else{
		dentry = new_dentry(fname, NFS_REG_FILE);
	}
	dentry->parent = last_dentry;
	inode = allocate_inode(dentry);
	allocate_dentry(last_dentry->inode, dentry);
	
	return NFS_ERROR_NONE; 
}

/**
 * @brief 修改时间，为了不让touch报错 
 * 
 * @param path 相对于挂载点的路径
 * @param tv 实践
 * @return int 0成功，否则返回对应错误号
 */
int newfs_utimens(const char* path, const struct timespec tv[2]) {
	(void)path;
	return 0;
}
/******************************************************************************
* SECTION: 选做函数实现
*******************************************************************************/
/**
 * @brief 写入文件
 * 
 * @param path 相对于挂载点的路径
 * @param buf 写入的内容
 * @param size 写入的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 写入大小
 */
int newfs_write(const char* path, const char* buf, size_t size, off_t offset,
		        struct fuse_file_info* fi) {
	 boolean	is_find, is_root;
	struct newfs_dentry* dentry = lookup(path, &is_find, &is_root);
	struct newfs_inode*  inode;
	
	if (is_find == FALSE) {
		return -NFS_ERROR_NOTFOUND;
	}

	inode = dentry->inode;
	
	if (inode->dentry->file_type == NFS_DIR) {
		return -NFS_ERROR_ISDIR;	
	}

	if (inode->file_size < offset) {
		return -NFS_ERROR_SEEK;
	}

	size_t remaining_size = size;
    size_t written_size = 0;
    size_t cur_offset = offset;

    while (remaining_size > 0) {
        // 计算当前偏移所在块和块内偏移
        int block_idx = cur_offset / LOGIC_SZ();
        int block_offset = cur_offset % LOGIC_SZ();

        if (block_idx >= DATA_PER_FILE) {
            return -NFS_ERROR_NOSPACE;
        }

        // 检查是否需要分配新块
        if (!inode->data[block_idx]) {
            allocate_data(inode);
        }

        // 计算本次写入的数据量
        size_t write_size = (remaining_size < LOGIC_SZ() - block_offset ? (remaining_size) : LOGIC_SZ()-block_offset);

        // 写入数据
        memcpy(inode->data[block_idx] + block_offset, buf + written_size, write_size);

        // 更新写入状态
        remaining_size -= write_size;
        written_size += write_size;
        cur_offset += write_size;
    }

    // 更新文件大小
    inode->file_size = inode->file_size >  cur_offset ? inode->file_size : cur_offset;

    return written_size;
}

/**
 * @brief 读取文件
 * 
 * @param path 相对于挂载点的路径
 * @param buf 读取的内容
 * @param size 读取的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 读取大小
 */
int newfs_read(const char* path, char* buf, size_t size, off_t offset,
		       struct fuse_file_info* fi) {
	 boolean	is_find, is_root;
	struct newfs_dentry* dentry = lookup(path, &is_find, &is_root);
	struct newfs_inode*  inode;
	
	if (is_find == FALSE) {
		return -NFS_ERROR_NOTFOUND;
	}

	inode = dentry->inode;
	
	if (inode->dentry->file_type == NFS_DIR) {
		return -NFS_ERROR_ISDIR;	
	}

	if (inode->file_size < offset) {
		return -NFS_ERROR_SEEK;
	}

	size_t remaining_size = size;
    size_t read_size = 0;
    size_t cur_offset = offset;

    while (remaining_size > 0) {
        // 计算当前偏移所在块和块内偏移
        int block_idx = cur_offset / LOGIC_SZ();
        int block_offset = cur_offset % LOGIC_SZ();

        if (block_idx >= DATA_PER_FILE) {
            return -NFS_ERROR_NOSPACE;
        }

        // 检查是否需要分配新块
        if (!inode->data[block_idx]) {
            allocate_data(inode);
        }

        // 计算本次写入的数据量
        size_t write_size = (remaining_size < LOGIC_SZ() - block_offset ? (remaining_size) : LOGIC_SZ()-block_offset);

        // 写入数据
        memcpy(buf + read_size, inode->data[block_idx] + block_offset, write_size);

        // 更新写入状态
        remaining_size -= write_size;
        read_size += write_size;
        cur_offset += write_size;
    }

    // 更新文件大小
    inode->file_size = inode->file_size >  cur_offset ? inode->file_size : cur_offset;

    return read_size;
}
/**
 * @brief 删除文件
 * 
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则返回对应错误号
 */
int newfs_unlink(const char* path) {
	/* 选做 */
	return 0;
}

/**
 * @brief 删除目录
 * 
 * 一个可能的删除目录操作如下：
 * rm ./tests/mnt/j/ -r
 *  1) Step 1. rm ./tests/mnt/j/j
 *  2) Step 2. rm ./tests/mnt/j
 * 即，先删除最深层的文件，再删除目录文件本身
 * 
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则返回对应错误号
 */
int newfs_rmdir(const char* path) {
	/* 选做 */
	return 0;
}

/**
 * @brief 重命名文件 
 * 
 * @param from 源文件路径
 * @param to 目标文件路径
 * @return int 0成功，否则返回对应错误号
 */
int newfs_rename(const char* from, const char* to) {
	/* 选做 */
	return 0;
}

/**
 * @brief 打开文件，可以在这里维护fi的信息，例如，fi->fh可以理解为一个64位指针，可以把自己想保存的数据结构
 * 保存在fh中
 * 
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则返回对应错误号
 */
int newfs_open(const char* path, struct fuse_file_info* fi) {
	return NFS_ERROR_NONE;
}

/**
 * @brief 打开目录文件
 * 
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则返回对应错误号
 */
int newfs_opendir(const char* path, struct fuse_file_info* fi) {
	return NFS_ERROR_NONE;
}

/**
 * @brief 改变文件大小
 * 
 * @param path 相对于挂载点的路径
 * @param offset 改变后文件大小
 * @return int 0成功，否则返回对应错误号
 */
int newfs_truncate(const char* path, off_t offset) {
	boolean	is_find, is_root;
	struct newfs_dentry* dentry = lookup(path, &is_find, &is_root);
	struct newfs_inode*  inode;
	
	if (is_find == FALSE) {
		return -NFS_ERROR_NOTFOUND;
	}
	
	inode = dentry->inode;

	if (inode->dentry->file_type == NFS_DIR) {
		return -NFS_ERROR_ISDIR;
	}

	inode->file_size = offset;

	return NFS_ERROR_NONE;
}


/**
 * @brief 访问文件，因为读写文件时需要查看权限
 * 
 * @param path 相对于挂载点的路径
 * @param type 访问类别
 * R_OK: Test for read permission. 
 * W_OK: Test for write permission.
 * X_OK: Test for execute permission.
 * F_OK: Test for existence. 
 * 
 * @return int 0成功，否则返回对应错误号
 */
int newfs_access(const char* path, int type) {
		boolean	is_find, is_root;
	boolean is_access_ok = FALSE;
	struct newfs_dentry* dentry = lookup(path, &is_find, &is_root);
	struct newfs_inode*  inode;

	switch (type)
	{
	case R_OK:
		is_access_ok = TRUE;
		break;
	case F_OK:
		if (is_find) {
			is_access_ok = TRUE;
		}
		break;
	case W_OK:
		is_access_ok = TRUE;
		break;
	case X_OK:
		is_access_ok = TRUE;
		break;
	default:
		break;
	}
	return is_access_ok ? NFS_ERROR_NONE : -NFS_ERROR_ACCESS;
	return 0;
}	
/******************************************************************************
* SECTION: FUSE入口
*******************************************************************************/
int main(int argc, char **argv)
{
    int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	newfs_options.device = strdup("/dev/ddriver");

	if (fuse_opt_parse(&args, &newfs_options, option_spec, NULL) == -1)
		return -1;
	
	ret = fuse_main(args.argc, args.argv, &operations, NULL);
	fuse_opt_free_args(&args);
	return ret;
}