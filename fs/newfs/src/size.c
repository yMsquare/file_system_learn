#include <stdio.h>

typedef enum newfs_file_type {
    NFS_REG_FILE,
    NFS_DIR,
    NFS_SYM_LINK
} NFS_FILE_TYPE;

// int main() {
//     printf("Size of NFS_FILE_TYPE: %zu bytes\n", sizeof(NFS_FILE_TYPE));
//     return 0;
// }