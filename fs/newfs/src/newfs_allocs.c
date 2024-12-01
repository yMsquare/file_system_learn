#include "../include/newfs.h"
#include "types.h"

#include <stdint.h>
extern struct newfs_super super;
extern struct custom_options sfs_options;
/**
 * @brief 将denry插入到inode中，采用头插法
 *
 * @param inode
 * @param dentry
 * @return int
 */
int allocate_dentry(struct newfs_inode *inode, struct newfs_dentry *dentry) {
  if (inode->dentries == NULL) {
    inode->dentries = dentry;
  } else {
    dentry->brother = inode->dentries;
    inode->dentries = dentry;
  }
  inode->dir_dentry_cnt++;
  return inode->dir_dentry_cnt;
}

// 分配 inode
struct newfs_inode *allocate_inode(struct newfs_dentry *dentry) {
  struct newfs_inode *inode;
  int byte_cursor = 0;
  int bit_cursor = 0;
  int ino_cursor = 0;
  boolean is_find_free_entry = FALSE;
  // 检查位图是否有空位
  for (byte_cursor = 0; byte_cursor < super.sz_io * 2; byte_cursor++) {
    for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
      if ((super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {
        super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
        is_find_free_entry = TRUE;
        break;
      }
      ino_cursor++;
    }
    if (is_find_free_entry) {
      break;
    }
  }
  if (!is_find_free_entry || ino_cursor == super.max_ino) {
    printf("allocate inode failed ");
    return -NFS_ERROR_NOSPACE;
  }
  inode = (struct newfs_inode *)malloc(sizeof(struct newfs_inode));
  inode->ino = ino_cursor;
  inode->file_size = 0;

  dentry->ino = inode->ino;
  dentry->inode = inode;

  inode->dentry = dentry;

  inode->dir_dentry_cnt = 0;
  inode->dentries = NULL;

  if (inode->dentry->file_type == NFS_REG_FILE) { // ???
    // inode->data_block_pointer =
    //     (uint8_t *)malloc(DATA_PER_FILE * super.sz_io * 2);
    allocate_data(inode);
  }
  return inode;
}

// 为一个 inode 分配数据块 data_block_pointer
// 对于目录，data_block_pointer 为null
// 对于文件，data block pointer 为指向数据库的指针
void allocate_data(struct newfs_inode *inode) {
  int index = 0;
  boolean is_found = FALSE;
  boolean is_allocated = FALSE;
  struct newfs_inode *inode_cursor = inode;
  int byte_cursor = 0;
  int bit_cursor = 0;
  int datano_cursor = 0;
  boolean is_find_free_entry = FALSE;
  // 检查data位图是否有空位
  for (byte_cursor = 0; byte_cursor < super.sz_io * 2; byte_cursor++) {
    for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
      if ((super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {
        super.map_data[byte_cursor] |= (0x1 << bit_cursor);
        is_find_free_entry = TRUE;
        break;
      }
      datano_cursor++;
    }
    if (is_find_free_entry) {
      break;
    }
  }
  if (!is_find_free_entry || datano_cursor == super.max_data) {
    NFS_DBG("allocate data failed ");
    return -NFS_ERROR_NOSPACE;
  }

  for (index = 0; index < DATA_PER_FILE; index++) {
    if ((inode_cursor->data_block_no[index]) == -1) {
      // 空闲的数据块指针
      inode_cursor->data_block_no[index] = datano_cursor;
      inode->data[index] = (uint8_t*)malloc(LOGIC_SZ());
      // 分配一块内存空间，在inode中记录下这块空间的指针。
      // 地址是内存中的不是磁盘中的！会超出磁盘大小。需要写回磁盘，我还是需要一个no来计算offset。
      // 写回的时候根据 data_block_no 计算写回位置
      // ? data block pointer 如何记录下来？记录 datano
      break;
    }
  }
}