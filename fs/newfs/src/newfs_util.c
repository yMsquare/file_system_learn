#include "../include/newfs.h"
#include "ddriver.h"
#include "types.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

extern struct newfs_super super;
extern struct custom_options sfs_options;

#define ROUND_UP(value, round)                                                 \
  ((value) % (round) == 0 ? value : ((value) / (round) + 1) * (round))
#define ROUND_DOWN(value, round)                                               \
  ((value) % (round) == 0 ? value : ((value) / (round)) * round)
#define SFS_ROUND_DOWN(value, round)                                           \
  ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))

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
  for(int j = 0; j < DATA_PER_FILE; j++){
    inode_d.data_block_no[j] = inode->data_block_no[j];
  }
  int offset;
  // inode
  NFS_DBG("\n newfs_driver_write in sync: ino:%d, offset:%d \n", ino,
          INO_OFS(ino));
  if (newfs_driver_write(INO_OFS(ino), (uint8_t *)&inode_d,
                         sizeof(struct newfs_inode_d)) != 0) {
    NFS_DBG("[%s] io error\n", __func__);
    return -NFS_ERROR_IO;
  }
  // inode 下方的 data
  if (inode->dentry->file_type == NFS_DIR) { // 目录,将子目录的inode写回,dentry也要写回
    dentry_cursor = inode->dentries;
    offset = DENTRY_OFS(inode->data_block_no[0]); // todo ： dentry 所在的地方应该是data block 区域
    // ! 好像没关系，因为dentry_cursor会是null。
    while (dentry_cursor != NULL) {
      memcpy(dentry_d.name, dentry_cursor->name, 128);
      dentry_d.file_type = dentry_cursor->file_type;
      dentry_d.ino = dentry_cursor->ino;
      NFS_DBG("[%s] sync dentry: %s offset : %d\n", __func__, dentry_d.name,offset);
      if (newfs_driver_write(offset, (uint8_t *)&dentry_d,
                             sizeof(struct newfs_dentry_d)) != 0) {
        NFS_DBG("[%s] io error\n", __func__); // 写回 dentry
        return -NFS_ERROR_IO;
      }
      if (dentry_cursor->inode != NULL) {
        sync_inode(dentry_cursor->inode);// 写回 inode
      }
      dentry_cursor = dentry_cursor->brother;
      offset += sizeof(struct newfs_dentry_d);
    }
  } else if (inode->dentry->file_type == NFS_REG_FILE) { /* 如果当前inode是文件，那么数据是文件内容，直接写即可
                                                          */
    for (int i = 0; i < DATA_PER_FILE; i++) {
      int data_no = inode->data_block_no[i];
      if (data_no > -1) {
        NFS_DBG("\n[%s] newfs_driver_write in sync in file: ino:%d, offset:%d \n",
                __func__,ino, DATA_OFS(data_no));
        if (newfs_driver_write(DATA_OFS(data_no),
                               inode->data[i],
                               inode->file_size) != 0) { // ? filesize 是多大？
          NFS_DBG("[%s] io error\n", __func__);
          return -NFS_ERROR_IO;
        }
      }
    }
  }
  return NFS_ERROR_NONE;
}

void dump_map() {
  int byte_cursor = 0;
  int bit_cursor = 0;
  NFS_DBG(
      "---dump map: from super.map_inode's offset:%d\n super.inode_offset:%d\n",
      super.map_inode_offset, super.inode_offset);
  int cnt = 0;

  for (byte_cursor = 0; byte_cursor < super.sz_io * 2; byte_cursor += 4) {
    // if(byte_cursor == ROUND_UP(super.data_offset, 1024)){
    //   printf("\n\n data now \n \n ");
    // }

    // 打印 256 行 每行 4 组， 每组 8 个, 共 8192 个bit。实际上最多只支持582个。
    for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
      printf("%d ", (super.map_inode[byte_cursor] & (0x1 << bit_cursor)) >>
                        bit_cursor);
      cnt++;
      if (byte_cursor * 8 + bit_cursor == super.inode_offset) {
        printf("\n\n inode now \n \n ");
      }
    }
    printf("\t");
    for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
      printf("%d ", (super.map_inode[byte_cursor + 1] & (0x1 << bit_cursor)) >>
                        bit_cursor);

      cnt++;
      if (byte_cursor * 8 + bit_cursor + 8 == super.inode_offset) {
        printf("\n\n inode now \n \n ");
      }
    }
    printf("\t");
    for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
      printf("%d ", (super.map_inode[byte_cursor + 2] & (0x1 << bit_cursor)) >>
                        bit_cursor);
      cnt++;
      if (byte_cursor * 8 + bit_cursor + 16 == super.inode_offset) {
        printf("\n\n inode now \n \n ");
      }
    }
    printf("\t");
    for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
      printf("%d ", (super.map_inode[byte_cursor + 3] & (0x1 << bit_cursor)) >>
                        bit_cursor);
      cnt++;
      if (byte_cursor * 8 + bit_cursor + 24 == super.inode_offset) {
        printf("\n\n inode now \n \n ");
      }
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
  NFS_DBG("\n -- driver reading offset: %d\n", offset);
  int offset_aligned = ROUND_DOWN(offset, IO_SZ()); // io
  int bias = offset - offset_aligned;
  int size_aligned = ROUND_UP((size + bias), IO_SZ());
  uint8_t *temp_content = (uint8_t *)malloc(size_aligned);
  uint8_t *cur = temp_content;
  if (!temp_content) {
    free(temp_content);
    return -1; // 文件指针设置失败
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
  // NFS_DBG("-- driver write: write to offset_aligned: %d, \ncontent: %s, bias:
  // %d \n",offset_aligned,in_content,bias);
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
  if (total_lvl == 0) {
    *is_find = TRUE;
    *is_root = TRUE;
    dentry_ret = super.root_dentry;
  }

  fname = strtok(path_cpy, "/");

  while (fname) {
    lvl++;
    inode = dentry_cursor->inode;

    if (inode->dentry->file_type == NFS_REG_FILE && lvl < total_lvl) {
      NFS_DBG("\n[%s] not a dir\n", __func__);
      dentry_ret = inode->dentry;
      break;
    }
    if (inode->dentry->file_type == NFS_DIR) {
      NFS_DBG("\n[%s] %s is a dir\n", __func__, inode->dentry->name);

      dentry_ret = inode->dentry;
      NFS_DBG("\n[%s] dentry_ret %s \n", __func__, dentry_ret->name);

      dentry_cursor = inode->dentries; // 所有目录项
      is_hit = FALSE;

      while (dentry_cursor) {

        NFS_DBG("\n[%s] -- dentry_cursor : %s, fname:%s\n", __func__,dentry_cursor->name, fname);

        if (memcmp(dentry_cursor->name, fname, strlen(fname)) == 0) {

          is_hit = TRUE;
          break;
        }
        dentry_cursor = dentry_cursor->brother;
        NFS_DBG("\n[%s] -- changed to brother %s", __func__,dentry_cursor->name);
      }
      if (!is_hit) {
        *is_find = FALSE;
        NFS_DBG("[%s] not found %s\n", __func__, fname);
        dentry_ret = inode->dentry;
       NFS_DBG("\n[%s] dentry_ret %s \n", __func__, dentry_ret->name);
        break;
      }
      if (is_hit && lvl == total_lvl) {
        NFS_DBG("\n!![%s] found %s\n", __func__, fname);

        *is_find = TRUE;
        dentry_ret = dentry_cursor;
        NFS_DBG("\n[%s] dentry_ret %s \n", __func__, dentry_ret->name);
        break;
      }
    }
    fname = strtok(NULL, "/");
  }
  if (dentry_ret->inode == NULL) {
    NFS_DBG("\n[%s] dentry_ret:%s->inode == NULL, reading inode by inos \n", __func__);
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
struct newfs_inode * read_inode(struct newfs_dentry *dentry, int ino) {
  struct newfs_inode *inode =
      (struct newfs_inode *)malloc(sizeof(struct newfs_inode));
  struct newfs_inode_d inode_d;
  struct newfs_dentry *sub_dentry;
  struct newfs_dentry_d dentry_d;
  int dir_cnt = 0, i;
  /* 从磁盘读索引结点 */

  NFS_DBG("[%s] reading ino : %d, offset: %d \n", __func__, ino, INO_OFS(ino));

  if (newfs_driver_read(INO_OFS(ino), (uint8_t *)&inode_d,
                        sizeof(struct newfs_inode_d)) != 0) {
    NFS_DBG("[%s] io error\n", __func__);
    return NULL;
  }

  NFS_DBG("[%s] just read inode_d ino : %d from offset: %d\n", __func__, ino, INO_OFS(ino));
  // 此时 inode_d 已经在内存里了，包括 data_block_no[6]
  inode->ino = inode_d.ino;
  inode->dir_dentry_cnt = 0;
  inode->file_size = inode_d.size;
  inode->file_type = inode_d.file_type;// todo 
  // memcpy(inode->target_path, inode_d.target_path, SFS_MAX_FILE_NAME);
  inode->dentry = dentry;
  NFS_DBG("[%s] just set inode's dentry : %s\n", __func__, dentry->name);
  inode->dentries = NULL;
  // inode->file_type = inode_d.file_type;
  NFS_DBG("[%s] just set inode's filetype : %c\n", __func__, inode->file_type);

  for (int i = 0; i < DATA_PER_FILE; i++) {
    inode->data_block_no[i] = inode_d.data_block_no[i];
  }
  /* 内存中的inode的数据或子目录项部分也需要读出 */
  if (inode->dentry->file_type ==NFS_DIR) { // inode 指向一个包含了若干dentry的数据块
    NFS_DBG("\n---read_inode: reading DIR\n");
    dir_cnt = inode_d.dir_dentry_cnt;
    int k = 0;
    for (int j = 0; j < DATA_PER_FILE; j++) { // 读每一个数据块
      if (inode->data_block_no[j] == -1) {
        break;
      }
      //
      if (k == dir_cnt) {
        break;
      }
      //
      for (i = 0; i < 128;i++) { // todo 
      if(k >= dir_cnt){
        break;
      }
        NFS_DBG("\n--- dir: i : %d\n", i);
        if (newfs_driver_read(DATA_OFS(inode->data_block_no[j]) +
                                  i * sizeof(struct newfs_dentry_d),
                              (uint8_t *)&dentry_d,
                              sizeof(struct newfs_dentry_d)) != 0) {
          return NULL;
        }
        sub_dentry = new_dentry(dentry_d.name, dentry_d.file_type);
        sub_dentry->parent = inode->dentry;
        sub_dentry->ino = dentry_d.ino;
        allocate_dentry(inode, sub_dentry);
        k++;
      }
    }
  } else if (inode->dentry->file_type == NFS_REG_FILE) {
    NFS_DBG("\n---read_inode: reading FILE\n");
    for (int i = 0; i < DATA_PER_FILE; i++) {
      if (inode->data_block_no[i] > -1) {
        inode->data[i] = (uint8_t *)malloc(LOGIC_SZ());
        if (newfs_driver_read(super.data_offset +
                                  inode->data_block_no[i] * LOGIC_SZ(),
                              inode->data[i], LOGIC_SZ()) != NFS_ERROR_NONE) {
          NFS_DBG("[%s] io error\n", __func__);
        }
      }
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