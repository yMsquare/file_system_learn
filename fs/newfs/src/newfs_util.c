#include "../include/newfs.h"
#include "ddriver.h"
#include "types.h"
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

extern struct newfs_super super;
extern struct custom_options sfs_options;

#define ROUND_UP(value, round)                                                 \
  ((value) % (round) == 0 ? value : ((value) / (round) + 1 ) * (round))
#define ROUND_DOWN(value, round)                                               \
  ((value) % (round) == 0 ? value : ((value) / (round) ) * round)
#define SFS_ROUND_DOWN(value, round)                                           \
  ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
/**
 * @brief 将denry插入到inode中，采用头插法
 *
 * @param inode
 * @param dentry
 * @return int
 */
int alloc_dentry(struct newfs_inode *inode, struct newfs_dentry *dentry) {
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
    if (!is_find_free_entry || ino_cursor == super.max_ino) {
      printf("allocate inode failed ");
    }
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
    inode->data_block_pointer =
        (uint8_t *)malloc(DATA_PER_FILE * super.sz_io * 2);
  }
  return inode;
}

// 分配 inode
// struct newfs_inode* allocate_data(struct newfs_dentry * dentry){
//   struct newfs_inode* inode ;
//   int byte_cursor = 0;
//   int bit_cursor = 0;
//   int datano_cursor = 0;
//   boolean is_find_free_entry = FALSE;
//   // 检查位图是否有空位
//   for (byte_cursor = 0; byte_cursor < super.sz_io * 2; byte_cursor++) {
//     for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
//       if ((super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {
//         super.map_data[byte_cursor] |= (0x1 << bit_cursor);
//         is_find_free_entry = TRUE;
//         break;
//       }
//       datano_cursor++;
//     }
//     if (is_find_free_entry) {
//       break;
//     }
//     if (!is_find_free_entry || datano_cursor == super.max_data) {
//       printf("allocate data blk failed ");
//     }
//   }
//   inode = (struct newfs_inode *)malloc(sizeof(struct newfs_inode));
//   inode->ino = ino_cursor;
//   inode->file_size = 0;

//   dentry->ino = inode->ino;
//   dentry->inode = inode;

//   inode->dentry = dentry;

//   inode->dir_dentry_cnt = 0;
//   inode->dentries = NULL;

//   if(inode->file_type == NFS_REG_FILE){
//     inode->data_block_pointer = (uint8_t *)
//     malloc(DATA_PER_FILE*super.sz_io*2);
//   }
//   return inode;
// }

int sync_inode(struct newfs_inode *inode) {
  struct newfs_inode_d inode_d;
  struct newfs_dentry *dentry_cursor;
  struct newfs_dentry_d dentry_d;
  int ino = inode->ino;
  inode_d.ino = ino;
  inode_d.size = inode->file_size;
  // memcpy() 用于软链接的复制
  inode_d.file_type = inode->dentry->file_type;
  inode_d.dir_dentry_cnt = inode->dir_dentry_cnt;
  int offset;
  // inode
  if (newfs_driver_write(INO_OFS(ino), (uint8_t *)&inode_d,
                         sizeof(struct newfs_inode_d)) != 0) {
    return -1; // todo 错误号
  }
  // data
  if (inode->file_type == NFS_DIR) { // 目录
    dentry_cursor = inode->dentries;
    offset = *inode->data_block_pointer; // ?
    while (dentry_cursor != NULL) {
      memcpy(dentry_d.name, dentry_cursor->name, 128);
      dentry_d.file_type = dentry_cursor->file_type;
      dentry_d.ino = dentry_cursor->ino;
      if (newfs_driver_write(offset, (uint8_t *)&dentry_d,
                             sizeof(struct newfs_dentry_d)) != 0) {
        return -1;
      }
      if (dentry_cursor->inode != NULL) {
        sync_inode(dentry_cursor->inode);
      }
      dentry_cursor = dentry_cursor->brother;
      offset += sizeof(struct newfs_dentry_d);
    }
  }
  return 0;
}

// 创建新的 dentry
// 需要为这个新的 dentry 预先申请一个新的父目录，供对应的 dentry_d 写回磁盘用。
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

void dump_map() {
  int byte_cursor = 0;
  int bit_cursor = 0;

  for (byte_cursor = 0; byte_cursor < super.sz_io * 2; byte_cursor += 4) {
    for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
      printf("%d ", (super.map_inode[byte_cursor] & (0x1 << bit_cursor)) >>
                        bit_cursor);
    }
    for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
      printf("%d ", (super.map_inode[byte_cursor + 1] & (0x1 << bit_cursor)) >>
                        bit_cursor);
    }
    for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
      printf("%d ", (super.map_inode[byte_cursor + 2] & (0x1 << bit_cursor)) >>
                        bit_cursor);
    }
    for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
      printf("%d ", (super.map_inode[byte_cursor + 3] & (0x1 << bit_cursor)) >>
                        bit_cursor);
    }
    printf("\n");
  }
}

/**
 * @brief 驱动读
 *
 * @param offset
 * @param out_content
 * @param size
 * @return int
 */
int newfs_driver_read(int offset, uint8_t *out_content, int size) {
  int offset_aligned = ROUND_DOWN(offset, IO_SZ()); // io
  int bias = offset - offset_aligned;
  int size_aligned = ROUND_UP((size + bias), IO_SZ());
  uint8_t *temp_content = (uint8_t *)malloc(size_aligned);
  uint8_t *cur = temp_content;
  if( !temp_content){
    free(temp_content);
    return -1;  // 文件指针设置失败
  }
  // lseek(SFS_DRIVER(), offset_aligned, SEEK_SET);
  ddriver_seek(super.driver_fd, offset_aligned, SEEK_SET);
  while (size_aligned != 0) {
    ddriver_read(super.driver_fd, (char *)cur, IO_SZ());
    cur += IO_SZ();
    size_aligned -= IO_SZ();
  }
  memcpy(out_content, temp_content + bias, size);
  free(temp_content);
  return 0;
}
/**
 * @brief 驱动写
 *
 * @param offset
 * @param in_content
 * @param size
 * @return int
 */
int newfs_driver_write(int offset, uint8_t *in_content, int size) {
  int offset_aligned = ROUND_DOWN(offset, IO_SZ());
  int bias = offset - offset_aligned;
  int size_aligned = ROUND_UP((size + bias), IO_SZ());
  uint8_t *temp_content = (uint8_t *)malloc(size_aligned);
  uint8_t *cur = temp_content;
  newfs_driver_read(
      offset_aligned, temp_content,
      size_aligned); // ddriver_read
                     // 从磁盘里偏移的对应位置独处数据然后直接放置在缓冲区中
  memcpy(temp_content + bias, in_content, size);
  // 要往一个位置里写东西，先把那一个位置对应的块全部读出来放到内存里（按照地址对齐），然后接着bias在那个位置之后进行写
  //  在内存中写完之后再把这块写完的东西写回到磁盘里。
  //  lseek(SFS_DRIVER(), offset_aligned, SEEK_SET);
  ddriver_seek(super.driver_fd, offset_aligned, SEEK_SET);
  while (size_aligned != 0) {
    // write(SFS_DRIVER(), cur, SFS_IO_SZ());
    ddriver_write(super.driver_fd, cur, IO_SZ());
    cur += IO_SZ();
    size_aligned -= IO_SZ();
  }

  free(temp_content);
  return 0;
}

int calc_lvl(const char *path) {
  // char* path_cpy = (char *)malloc(strlen(path));
  // strcpy(path_cpy, path);
  char *str = path;
  int lvl = 0;
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
 * @brief 获取文件名
 *
 * @param path
 * @return char*
 */
char *get_fname(const char *path) {
  char ch = '/';
  char *q = strrchr(path, ch) + 1;
  return q;
}

struct newfs_dentry *lookup(const char *path, boolean *is_find,
                            boolean *is_root) {
  struct newfs_dentry *dentry_cursor = super.root_dentry;
  struct newfs_dentry *dentry_ret = NULL;
  struct newfs_inode *inode;
  int total_lvl = calc_lvl(path);
  int lvl = 0;
  boolean is_hit;
  char *fname = NULL;
  char *path_cpy = (char *)malloc(sizeof(path));
  *is_root = FALSE;
  strcpy(path_cpy, path);
  // debug
  printf("looking up path: %s\n", path);
  fflush(stdout);

  if (total_lvl == 0) {
    *is_find = TRUE;
    *is_root = TRUE;
    dentry_ret = super.root_dentry;
  }
  fname = strtok(path_cpy, "/");
  printf("fname : %s\n", fname);
  fflush(stdout);
  while (fname) {
    lvl++;
    if (dentry_cursor->inode == NULL) {
      read_inode(dentry_cursor, dentry_cursor->ino);
    }
    inode = dentry_cursor->inode;

    if (inode->dentry->file_type == NFS_REG_FILE && lvl < total_lvl) {
      printf("not a dir");
      dentry_ret = inode->dentry;
      break;
    }
    if (inode->dentry->file_type == NFS_DIR) {
      dentry_cursor = inode->dentries; // 所有目录项
      is_hit = FALSE;

       while (dentry_cursor) {
        if (memcmp(dentry_cursor->name, fname, strlen(fname)) == 0) {
          is_hit = TRUE;
          break;
        }
        dentry_cursor = dentry_cursor->brother;
      }
      if (!is_hit) {
        *is_find = FALSE;
        printf("not found ");
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
    dentry_ret->inode = read_inode(dentry_ret, dentry_ret->ino);
  }
  return dentry_ret;
}

/**
 * @brief
 *
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct sfs_inode*
 */
struct newfs_inode *read_inode(struct newfs_dentry *dentry, int ino) {
  struct newfs_inode *inode =
      (struct sfs_inode *)malloc(sizeof(struct newfs_inode));
  struct newfs_inode_d inode_d;
  struct newfs_dentry *sub_dentry;
  struct newfs_dentry_d dentry_d;
  int dir_cnt = 0, i;
  /* 从磁盘读索引结点 */
  if (newfs_driver_read(INO_OFS(ino), (uint8_t *)&inode_d,
                        sizeof(struct newfs_inode_d)) != 0) {
    return NULL;
  }
  inode->dir_dentry_cnt = 0;
  inode->ino = inode_d.ino;
  inode->file_size = inode_d.size;
  // memcpy(inode->target_path, inode_d.target_path, SFS_MAX_FILE_NAME);
  inode->dentry = dentry;
  inode->dentries = NULL;
  /* 内存中的inode的数据或子目录项部分也需要读出 */
  if (inode->file_type == NFS_DIR) {
    dir_cnt = inode_d.dir_dentry_cnt;
    for (i = 0; i < dir_cnt; i++) {
      if (newfs_driver_read(DATA_OFS(ino) + i * sizeof(struct newfs_dentry_d),
                            (uint8_t *)&dentry_d,
                            sizeof(struct newfs_dentry_d)) != 0) {
        return NULL;
      }
      sub_dentry = new_dentry(dentry_d.name, dentry_d.file_type);
      sub_dentry->parent = inode->dentry;
      sub_dentry->ino = dentry_d.ino;
      alloc_dentry(inode, sub_dentry);
    }
  } else if (inode->file_type == NFS_REG_FILE) {
    inode->data_block_pointer = (uint8_t *)malloc(BLKS_SZ(DATA_PER_FILE));
    if (newfs_driver_read(DATA_OFS(ino), (uint8_t *)inode->data_block_pointer,
                          BLKS_SZ(DATA_PER_FILE)) != 0) {
      // SFS_DBG("[%s] io error\n", __func__);
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
struct newfs_dentry *get_dentry(struct newfs_inode *inode, int dir) {
  struct newfs_dentry *dentry_cursor = inode->dentries;
  int cnt = 0;
  while (dentry_cursor) {
    if (dir == cnt) {
      return dentry_cursor;
    }
    cnt++;
    dentry_cursor = dentry_cursor->brother;
  }
  return NULL;
}