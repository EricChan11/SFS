#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>

#define FILENAME 8
#define EXPAND 3
#define BLOCKSIZE 512
struct sb
{
    long fs_size;                  // 文件系统的大小，以块为单位
    long first_blk;                // 数据区的第一块块号，根目录也放在此
    long datasize;                 // 数据区大小，以块为单位
    long first_inode;              // inode区起始块号
    long inode_area_size;          // inode区大小，以块为单位
    long fisrt_blk_of_inodebitmap; // inode位图区起始块号
    long inodebitmap_size;         // inode位图区大小，以块为单位
    long first_blk_of_databitmap;  // 数据块位图起始块号
    long databitmap_size;          // 数据块位图大小，以块为单位
};

struct inode
{                            // 共64字节
    short int st_mode;       /* 权限，2字节 */
    short int st_ino;        /* i-node号，2字节 */
    char st_nlink;           /* 连接数，1字节 */
    uid_t st_uid;            /* 拥有者的用户 ID ，4字节 */
    gid_t st_gid;            /* 拥有者的组 ID，4字节  */
    off_t st_size;           /*文件大小，4字节 */
    struct timespec st_atim; /* 16个字节time of last access */
    short int addr[7];       /*磁盘地址，14字节*/
};

struct directory
{ // 共16字节
    char name[FILENAME + 1];
    char expand[EXPAND + 1];
    short int st_ino; /* i-node号，2字节 */
    // 3字节备用
};
int main()
{
    FILE *fp = NULL;
    fp = fopen("/home/cky/Desktop/SFS/disk", "r+"); // 打开文件
    if (fp == NULL)
    {
        printf("打开文件失败，文件不存在\n");
        return 0;
    }
    // SB占1块，Inode位图1块，数据块位图4块
    // Inode区占4K*64B/512B=512块
    // 共有8MB/512B=16K=16,384块
    // 根目录的Inode在6块的前64个字节（0-5，共6块被SB，Inode位图，数据块位图占据）
    // 根目录在516块（0-515，共516块被SB，Inode位图，数据块位图，Inode占据）
    // 数据区还剩下16384-516=15,868
    // 1. 初始化SB     大小：1块
    struct sb *super_blk = malloc(sizeof(struct sb)); // 动态内存分配，申请super_blk
    super_blk->fs_size = 16384;
    super_blk->first_blk = 518; // 根目录的data_block在1281编号的块中（从0开始编号）
    super_blk->datasize = 15866;
    super_blk->first_inode = 6;
    super_blk->inode_area_size = 512;
    super_blk->fisrt_blk_of_inodebitmap = 1;
    super_blk->inodebitmap_size = 1;
    super_blk->first_blk_of_databitmap = 2;
    super_blk->databitmap_size = 4;
    fwrite(super_blk, sizeof(struct sb), 1, fp);
    printf("initial super_block success!\n");
    // 初始化Inode位图，在1块
    // 第一个被用了，把第一位初始化成1
    if (fseek(fp, BLOCKSIZE * 1, SEEK_SET) != 0) // 首先要将指针移动到文件的1块的起始位置
        fprintf(stderr, "inode bitmap fseek failed!\n");
    int ino = 0;
    int mask1 = 1;
    mask1 <<= 31;
    ino |= mask1;
    fwrite(&ino, sizeof(int), 1, fp);
    printf("initial inode bitmap success!\n");
    // 初始化数据块位图，从2块开始，有4块
    // 518块已经用了，518/32=16余8
    // 用大小为16的int（4字节32位）数组初始化前512块
    if (fseek(fp, BLOCKSIZE * 2, SEEK_SET) != 0) // 首先要将指针移动到文件的2块的起始位置
        fprintf(stderr, "data bitmap fseek failed!\n");
    int a[16];                // 刚好大小为512bit，可以用来初始化数据区位图的前512bit
    memset(a, -1, sizeof(a)); // 补码
    fwrite(a, sizeof(a), 1, fp);
    // 512-518
    int b = 0;
    int i = 0;
    int mask = 1;
    mask <<= 26;
    for (int i = 0; i < 6; i++)
    {
        b |= mask;
        mask <<= 1;
    }
    fwrite(&b, sizeof(int), 1, fp);
    // 初始化这一块剩下的部分
    // 一块512*8bit，可以用128（128*32=512*8)个整数表示，128-16-1=111
    int c[111];
    memset(c, 0, sizeof(c));
    fwrite(c, sizeof(c), 1, fp);
    // 初始化剩下3块
    int rest_of_bitmap = (3 * 512) / 4;
    int d[rest_of_bitmap];
    memset(d, 0, sizeof(d));
    fwrite(d, sizeof(d), 1, fp);
    printf("initial data bitmap success!\n");
    // 初始化inode区
    fseek(fp, BLOCKSIZE * 6, SEEK_SET);
    struct inode *root = malloc(sizeof(struct inode));
    root->addr[0] = 518;
    int i2 = 1;
    for (; i2 <= 7; i2++)
    {
        root->addr[i2] = 0;
    }
    root->st_ino = 0;
    root->st_size = 0;
    root->st_nlink = 0;
    // 杂七杂八的权限没搞
    clock_gettime(CLOCK_REALTIME, &(root->st_atim));
    fwrite(root, sizeof(struct inode), 1, fp);
    printf("initial inode area success!\n");
    // 根目录一开始没有目录项
    fclose(fp);
    printf("all init success!\n");
}
