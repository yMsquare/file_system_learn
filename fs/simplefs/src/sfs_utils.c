#include "../include/sfs.h"
#include "types.h"
#include <stdio.h>

extern struct sfs_super      sfs_super; 
extern struct custom_options sfs_options;

/**
 * @brief 获取文件名
 * 
 * @param path 
 * @return char* 
 */
char* sfs_get_fname(const char* path) {
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}
/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int sfs_calc_lvl(const char * path) {
    // char* path_cpy = (char *)malloc(strlen(path));
    // strcpy(path_cpy, path);
    char* str = path;
    int   lvl = 0;
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != NULL) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}
/**
 * @brief 驱动读
 * 
 * @param offset 
 * @param out_content 
 * @param size 
 * @return int 
 */
int sfs_driver_read(int offset, uint8_t *out_content, int size) {
    int      offset_aligned = SFS_ROUND_DOWN(offset, SFS_IO_SZ());
    int      bias           = offset - offset_aligned;
    int      size_aligned   = SFS_ROUND_UP((size + bias), SFS_IO_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    // lseek(SFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(SFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // read(SFS_DRIVER(), cur, SFS_IO_SZ());
        ddriver_read(SFS_DRIVER(), cur, SFS_IO_SZ());
        cur          += SFS_IO_SZ();
        size_aligned -= SFS_IO_SZ();   
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return SFS_ERROR_NONE;
}
/**
 * @brief 驱动写
 * 
 * @param offset 
 * @param in_content 
 * @param size 
 * @return int 
 */
int sfs_driver_write(int offset, uint8_t *in_content, int size) {
    int      offset_aligned = SFS_ROUND_DOWN(offset, SFS_IO_SZ());
    int      bias           = offset - offset_aligned;
    int      size_aligned   = SFS_ROUND_UP((size + bias), SFS_IO_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    sfs_driver_read(offset_aligned, temp_content, size_aligned);
    memcpy(temp_content + bias, in_content, size);
    
    // lseek(SFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(SFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // write(SFS_DRIVER(), cur, SFS_IO_SZ());
        ddriver_write(SFS_DRIVER(), cur, SFS_IO_SZ());
        cur          += SFS_IO_SZ();
        size_aligned -= SFS_IO_SZ();   
    }

    free(temp_content);
    return SFS_ERROR_NONE;
}
/**
 * @brief 将denry插入到inode中，采用头插法
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int sfs_alloc_dentry(struct sfs_inode* inode, struct sfs_dentry* dentry) {
    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;
    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    return inode->dir_cnt;
}
/**
 * @brief 将dentry从inode的dentrys中取出
 * 
 * @param inode 一个目录的索引结点
 * @param dentry 该目录下的一个目录项
 * @return int 
 */
int sfs_drop_dentry(struct sfs_inode * inode, struct sfs_dentry * dentry) {
    boolean is_find = FALSE;
    struct sfs_dentry* dentry_cursor;
    dentry_cursor = inode->dentrys;
    
    if (dentry_cursor == dentry) {
        inode->dentrys = dentry->brother;
        is_find = TRUE;
    }
    else {
        while (dentry_cursor)
        {
            if (dentry_cursor->brother == dentry) {
                dentry_cursor->brother = dentry->brother;
                is_find = TRUE;
                break;
            }
            dentry_cursor = dentry_cursor->brother;
        }
    }
    if (!is_find) {
        return -SFS_ERROR_NOTFOUND;
    }
    inode->dir_cnt--;
    return inode->dir_cnt;
}
/**
 * @brief 分配一个inode，占用位图
 * 
 * @param dentry 该dentry指向分配的inode
 * @return sfs_inode
 */
struct sfs_inode* sfs_alloc_inode(struct sfs_dentry * dentry) {
    struct sfs_inode* inode;
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    boolean is_find_free_entry = FALSE;
    /* 检查位图是否有空位 */
    for (byte_cursor = 0; byte_cursor < SFS_BLKS_SZ(sfs_super.map_inode_blks); 
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((sfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                sfs_super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }

    if (!is_find_free_entry || ino_cursor == sfs_super.max_ino)
        return -SFS_ERROR_NOSPACE;

    inode = (struct sfs_inode*)malloc(sizeof(struct sfs_inode));
    inode->ino  = ino_cursor; 
    inode->size = 0;
                                                      /* dentry指向inode */
    dentry->inode = inode;
    dentry->ino   = inode->ino;
                                                      /* inode指回dentry */
    inode->dentry = dentry;
    
    inode->dir_cnt = 0;
    inode->dentrys = NULL;
    
    if (SFS_IS_REG(inode)) {
        inode->data = (uint8_t *)malloc(SFS_BLKS_SZ(SFS_DATA_PER_FILE));
    }

    return inode;
}
/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 * 
 * @param inode 
 * @return int 
 */
int sfs_sync_inode(struct sfs_inode * inode) {
    struct sfs_inode_d  inode_d;
    struct sfs_dentry*  dentry_cursor;
    struct sfs_dentry_d dentry_d;
    int ino             = inode->ino;
    inode_d.ino         = ino;
    inode_d.size        = inode->size;
    memcpy(inode_d.target_path, inode->target_path, SFS_MAX_FILE_NAME);
    inode_d.ftype       = inode->dentry->ftype;
    inode_d.dir_cnt     = inode->dir_cnt;
    int offset;
    /* 先写inode本身 */
    SFS_DBG("\n----sync:ino: %d, offset:%d \n", ino,  SFS_INO_OFS(ino));
    if (sfs_driver_write(SFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                     sizeof(struct sfs_inode_d)) != SFS_ERROR_NONE) {
        SFS_DBG("[%s] io error\n", __func__);
        return -SFS_ERROR_IO;
    }

    /* 再写inode下方的数据 */
    if (SFS_IS_DIR(inode)) { /* 如果当前inode是目录，那么数据是目录项，且目录项的inode也要写回 */                          
        dentry_cursor = inode->dentrys;
        offset        = SFS_DATA_OFS(ino);
        while (dentry_cursor != NULL)
        {
            memcpy(dentry_d.fname, dentry_cursor->fname, SFS_MAX_FILE_NAME);
            dentry_d.ftype = dentry_cursor->ftype;
            dentry_d.ino = dentry_cursor->ino;
            if (sfs_driver_write(offset, (uint8_t *)&dentry_d, 
                                 sizeof(struct sfs_dentry_d)) != SFS_ERROR_NONE) {
                SFS_DBG("[%s] io error\n", __func__);
                return -SFS_ERROR_IO;                     
            }
            
            if (dentry_cursor->inode != NULL) {
                sfs_sync_inode(dentry_cursor->inode);
            }

            dentry_cursor = dentry_cursor->brother;
            offset += sizeof(struct sfs_dentry_d);
        }
    }
    else if (SFS_IS_REG(inode)) { /* 如果当前inode是文件，那么数据是文件内容，直接写即可 */
        if (sfs_driver_write(SFS_DATA_OFS(ino), inode->data, 
                             SFS_BLKS_SZ(SFS_DATA_PER_FILE)) != SFS_ERROR_NONE) {
            SFS_DBG("[%s] io error\n", __func__);
            return -SFS_ERROR_IO;
        }
    }
    return SFS_ERROR_NONE;
}
/**
 * @brief 删除内存中的一个inode
 * Case 1: Reg File
 * 
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Reg Dentry)
 *                       |
 *                      Inode  (Reg File)
 * 
 *  1) Step 1. Erase Bitmap     
 *  2) Step 2. Free Inode                      (Function of sfs_drop_inode)
 * ------------------------------------------------------------------------
 *  3) *Setp 3. Free Dentry belonging to Inode (Outsider)
 * ========================================================================
 * Case 2: Dir
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Dir Dentry)
 *                       |
 *                      Inode  (Dir)
 *                    /     \
 *                Dentry -> Dentry
 * 
 *   Recursive
 * @param inode 
 * @return int 
 */
int sfs_drop_inode(struct sfs_inode * inode) {
    struct sfs_dentry*  dentry_cursor;
    struct sfs_dentry*  dentry_to_free;
    struct sfs_inode*   inode_cursor;

    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    boolean is_find = FALSE;

    if (inode == sfs_super.root_dentry->inode) {
        return SFS_ERROR_INVAL;
    }

    if (SFS_IS_DIR(inode)) {
        dentry_cursor = inode->dentrys;
                                                      /* 递归向下drop */
        while (dentry_cursor)
        {   
            inode_cursor = dentry_cursor->inode;
            sfs_drop_inode(inode_cursor);
            sfs_drop_dentry(inode, dentry_cursor);
            dentry_to_free = dentry_cursor;
            dentry_cursor = dentry_cursor->brother;
            free(dentry_to_free);
        }

        for (byte_cursor = 0; byte_cursor < SFS_BLKS_SZ(sfs_super.map_inode_blks); 
            byte_cursor++)                            /* 调整inodemap */
        {
            for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
                if (ino_cursor == inode->ino) {
                     sfs_super.map_inode[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
                     is_find = TRUE;
                     break;
                }
                ino_cursor++;
            }
            if (is_find == TRUE) {
                break;
            }
        }
    }
    else if (SFS_IS_REG(inode) || SFS_IS_SYM_LINK(inode)) {
        for (byte_cursor = 0; byte_cursor < SFS_BLKS_SZ(sfs_super.map_inode_blks); 
            byte_cursor++)                            /* 调整inodemap */
        {
            for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
                if (ino_cursor == inode->ino) {
                     sfs_super.map_inode[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
                     is_find = TRUE;
                     break;
                }
                ino_cursor++;
            }
            if (is_find == TRUE) {
                break;
            }
        }
        if (inode->data)
            free(inode->data);
        free(inode);
    }
    return SFS_ERROR_NONE;
}
/**
 * @brief 
 * 
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct sfs_inode* 
 */
struct sfs_inode* sfs_read_inode(struct sfs_dentry * dentry, int ino) {
    struct sfs_inode* inode = (struct sfs_inode*)malloc(sizeof(struct sfs_inode));
    struct sfs_inode_d inode_d;
    struct sfs_dentry* sub_dentry;
    struct sfs_dentry_d dentry_d;
    int    dir_cnt = 0, i;
    /* 从磁盘读索引结点 */
    if (sfs_driver_read(SFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                        sizeof(struct sfs_inode_d)) != SFS_ERROR_NONE) {
        SFS_DBG("[%s] io error\n", __func__);
        return NULL;                    
    }
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    memcpy(inode->target_path, inode_d.target_path, SFS_MAX_FILE_NAME);
    inode->dentry = dentry;
    inode->dentrys = NULL;
    /* 内存中的inode的数据或子目录项部分也需要读出 */
    if (SFS_IS_DIR(inode)) {
        dir_cnt = inode_d.dir_cnt;
        for (i = 0; i < dir_cnt; i++)
        {
            // 这里要读的是当前 inode 的 dentry_d，是存放在 inode 指向的 data blk里面的
            if (sfs_driver_read(SFS_DATA_OFS(ino) + i * sizeof(struct sfs_dentry_d), 
                                (uint8_t *)&dentry_d, 
                                sizeof(struct sfs_dentry_d)) != SFS_ERROR_NONE) {
                SFS_DBG("[%s] io error\n", __func__);
                return NULL;
            }
            sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
            sub_dentry->parent = inode->dentry;
            sub_dentry->ino    = dentry_d.ino; 
            sfs_alloc_dentry(inode, sub_dentry);
        }
    }
    else if (SFS_IS_REG(inode)) {
        inode->data = (uint8_t *)malloc(SFS_BLKS_SZ(SFS_DATA_PER_FILE));
        if (sfs_driver_read(SFS_DATA_OFS(ino), (uint8_t *)inode->data, 
                            SFS_BLKS_SZ(SFS_DATA_PER_FILE)) != SFS_ERROR_NONE) {
            SFS_DBG("[%s] io error\n", __func__);
            return NULL;                    
        }
    }
    return inode;
}
/**
 * @brief 
 * 
 * @param inode 
 * @param dir [0...]
 * @return struct sfs_dentry* 
 */
struct sfs_dentry* sfs_get_dentry(struct sfs_inode * inode, int dir) {
    struct sfs_dentry* dentry_cursor = inode->dentrys;
    int    cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}
/**
 * @brief 查找文件或目录
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry 
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 *  
 * 
 * 如果能查找到，返回该目录项
 * 如果查找不到，返回的是上一个有效的路径
 * 
 * path: /a/b/c
 *      1) find /'s inode     lvl = 1
 *      2) find a's dentry 
 *      3) find a's inode     lvl = 2
 *      4) find b's dentry    如果此时找不到了，is_find=FALSE且返回的是a的inode对应的dentry
 * 
 * @param path 
 * @return struct sfs_dentry* 
 */
struct sfs_dentry* sfs_lookup(const char * path, boolean* is_find, boolean* is_root) {
    struct sfs_dentry* dentry_cursor = sfs_super.root_dentry;
    struct sfs_dentry* dentry_ret = NULL;
    struct sfs_inode*  inode; 
    int   total_lvl = sfs_calc_lvl(path);
    int   lvl = 0;
    boolean is_hit;
    char* fname = NULL;
    char* path_cpy = (char*)malloc(sizeof(path));
    *is_root = FALSE;
    strcpy(path_cpy, path);

    if (total_lvl == 0) {                           /* 根目录 */
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = sfs_super.root_dentry;
    }
    fname = strtok(path_cpy, "/");       
    while (fname)
    {   
        lvl++;
        if (dentry_cursor->inode == NULL) {           /* Cache机制 */
            sfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;

        if (SFS_IS_REG(inode) && lvl < total_lvl) {
            SFS_DBG("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }
        if (SFS_IS_DIR(inode)) {
           SFS_DBG("[%s] is a dir\n", __func__);
            dentry_ret = inode->dentry;

            dentry_cursor = inode->dentrys;
            is_hit        = FALSE;

            while (dentry_cursor)   /* 遍历子目录项 */
            {
                
                if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0) {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }
            
            if (!is_hit) {
                *is_find = FALSE;
                SFS_DBG("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }

            if (is_hit && lvl == total_lvl) {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/"); 
    }

    if (dentry_ret->inode == NULL) {
        dentry_ret->inode = sfs_read_inode(dentry_ret, dentry_ret->ino);
    }
    
    return dentry_ret;
}
/**
 * @brief 挂载sfs, Layout 如下
 * 
 * Layout
 * | Super | Inode Map | Data |
 * 
 * IO_SZ = BLK_SZ
 * 
 * 每个Inode占用一个Blk
 * @param options 
 * @return int 
 */
int sfs_mount(struct custom_options options){   // 参数是 custom_options ,选择 device 和 showhelp
    int                 ret = SFS_ERROR_NONE;
    int                 driver_fd; //驱动fd
    struct sfs_super_d  sfs_super_d;  // 磁盘超级快
    struct sfs_dentry*  root_dentry;  // 根目录dentry
    struct sfs_inode*   root_inode;   // 根目录的inode

    int                 inode_num; // inode 总数量
    int                 map_inode_blks; // inode 位图的块数
    
    int                 super_blks;  // 超级块的位置
    boolean             is_init = FALSE;

    sfs_super.is_mounted = FALSE;

    // driver_fd = open(options.device, O_RDWR);
    driver_fd = ddriver_open(options.device);

    if (driver_fd < 0) {
        return driver_fd;
    }

    sfs_super.driver_fd = driver_fd;
    ddriver_ioctl(SFS_DRIVER(), IOC_REQ_DEVICE_SIZE,  &sfs_super.sz_disk);
    ddriver_ioctl(SFS_DRIVER(), IOC_REQ_DEVICE_IO_SZ, &sfs_super.sz_io);
    
    root_dentry = new_dentry("/", SFS_DIR);     /* 根目录项每次挂载时新建 */

    if (sfs_driver_read(SFS_SUPER_OFS, (uint8_t *)(&sfs_super_d), 
                        sizeof(struct sfs_super_d)) != SFS_ERROR_NONE) {
        return -SFS_ERROR_IO;
    }   
    
    // 如果没有初始化                                   /* 读取super */
    if (sfs_super_d.magic_num != SFS_MAGIC_NUM) {     /* 幻数不正确，初始化 */
                                                      /* 估算各部分大小 */
        super_blks = SFS_ROUND_UP(sizeof(struct sfs_super_d), SFS_IO_SZ()) / SFS_IO_SZ();
        // super_blks 的位置，是一个 super_d 的大小 / IO大小
        // 实际应该是一个 super_d / logic

        inode_num  =  SFS_DISK_SZ() / ((SFS_DATA_PER_FILE + SFS_INODE_PER_FILE) * SFS_IO_SZ());
        // 磁盘大小 / 单个文件大小  * IO size

        map_inode_blks = SFS_ROUND_UP(SFS_ROUND_UP(inode_num, UINT32_BITS), SFS_IO_SZ()) 
                         / SFS_IO_SZ();
        // inode的个数 / IO size  = 需要多少个块来存放 inode 位图
        // 实际应该是 disk size
                                                      /* 布局layout */
        sfs_super.max_ino = (inode_num - super_blks - map_inode_blks); //最大 inode 数量
        sfs_super_d.map_inode_offset = SFS_SUPER_OFS + SFS_BLKS_SZ(super_blks); // inode 位图的位置
        sfs_super_d.data_offset = sfs_super_d.map_inode_offset + SFS_BLKS_SZ(map_inode_blks); // 数据位置,包括inode和实际数据的datablk
        //  清零索引节点和数据块位图
        sfs_super_d.map_inode_blks  = map_inode_blks; // inode 位图的数量
        sfs_super_d.sz_usage    = 0; // 初次挂载，磁盘使用量为0
        SFS_DBG("inode map blocks: %d\n", map_inode_blks); // debug信息
        is_init = TRUE; // 已经初始化
    }
    // 上述信息都是 to-disk 结构，以下为 in-mem 结构
    // 初始化过了，读取填充磁盘的布局信息、位图等
    sfs_super.sz_usage   = sfs_super_d.sz_usage;      /* 建立 in-memory 结构 */
    
    sfs_super.map_inode = (uint8_t *)malloc(SFS_BLKS_SZ(sfs_super_d.map_inode_blks));
    sfs_super.map_inode_blks = sfs_super_d.map_inode_blks;
    sfs_super.map_inode_offset = sfs_super_d.map_inode_offset;
    sfs_super.data_offset = sfs_super_d.data_offset;

    sfs_dump_map();

	printf("\n--------------------------------------------------------------------------------\n\n");
    if (sfs_driver_read(sfs_super_d.map_inode_offset, (uint8_t *)(sfs_super.map_inode), 
                        SFS_BLKS_SZ(sfs_super_d.map_inode_blks)) != SFS_ERROR_NONE) {
        return -SFS_ERROR_IO;
    }
    // 创建 空 的根目录 inode 和 dentry
    // 如果是初始化
    if (is_init) {                                    /* 分配根节点 */
        root_inode = sfs_alloc_inode(root_dentry);
        sfs_sync_inode(root_inode);
    }
    
    root_inode            = sfs_read_inode(root_dentry, SFS_ROOT_INO);  /* 读取根目录 */
    root_dentry->inode    = root_inode;
    // 将 super 块中的 root_dentry 设置为函数里定义的 dentry
    sfs_super.root_dentry = root_dentry;
    sfs_super.is_mounted  = TRUE;

    sfs_dump_map();
    return ret;
}
/**
 * @brief 
 * 
 * @return int 
 */
int sfs_umount() {
    
    struct sfs_super_d  sfs_super_d; 

    if (!sfs_super.is_mounted) {
        return SFS_ERROR_NONE;
    }

    sfs_sync_inode(sfs_super.root_dentry->inode);     /* 从根节点向下刷写节点 */
                                                    
    sfs_super_d.magic_num           = SFS_MAGIC_NUM;
    printf("----magic num : %d", sfs_super_d.magic_num);
    fflush(stdout);
    sfs_super_d.map_inode_blks      = sfs_super.map_inode_blks;
    sfs_super_d.map_inode_offset    = sfs_super.map_inode_offset;
    sfs_super_d.data_offset         = sfs_super.data_offset;
    sfs_super_d.sz_usage            = sfs_super.sz_usage;

    if (sfs_driver_write(SFS_SUPER_OFS, (uint8_t *)&sfs_super_d, 
                     sizeof(struct sfs_super_d)) != SFS_ERROR_NONE) {
        return -SFS_ERROR_IO;
    }

    if (sfs_driver_write(sfs_super_d.map_inode_offset, (uint8_t *)(sfs_super.map_inode), 
                         SFS_BLKS_SZ(sfs_super_d.map_inode_blks)) != SFS_ERROR_NONE) {
        return -SFS_ERROR_IO;
    }
    
    // int num = 0x1111;
    // sfs_driver_write(0,(uint8_t *)&num,sizeof(num));
   
    free(sfs_super.map_inode);
    ddriver_close(SFS_DRIVER());

    return SFS_ERROR_NONE;
}
