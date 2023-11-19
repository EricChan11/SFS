#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
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
{                      // 共64字节
    short int st_mode; /* 权限，2字节 */
    short int st_ino;  /* i-node号，2字节 */
    char st_nlink;     /* 连接数，1字节 */
    uid_t st_uid;      /* 拥有者的用户 ID ，4字节 */
    gid_t st_gid;      /* 拥有者的组 ID，4字节  */
    off_t st_size;     /*文件大小，4字节 */
    // off_t ac_size;
    struct timespec st_atim; /* 16个字节time of last access */
    short int addr[7];       /*磁盘地址，14字节*/
    // short int flag;          // 1 dir,2 file
};

struct directory
{ // 共16字节
    char name[9];
    char expand[4];
    short int st_ino; /* i-node号，2字节 */
    // st_ino=-1, is deleted, a tomb in here
    // 3字节备用
};

// 我的8M磁盘文件为"/home/cky/libfuse-master/example/testmount";
char *FILEADDR = "/home/cky/libfuse-master/example/testmount";
// 辅助函数声明

/***************************************************************************************************************************/
void inoinit(struct inode *root)
{
    root->st_ino = 0;
    root->st_size = 0;
    root->st_nlink = 0;
    root->st_gid = 0;
    root->st_uid = 0;
    // root->ac_size = 0;
    root->st_mode = 0;
    // root->flag = 0;
    for (int i = 0; i < 7; i++)
    {
        root->addr[i] = 0;
    }
}
void copyInode(struct inode *root, const struct inode *src)
{
    // 浅复制 inode 结构体的非嵌套部分
    // root->flag = src->flag;
    root->st_ino = src->st_ino;
    root->st_size = src->st_size;
    root->st_nlink = src->st_nlink;
    root->st_gid = src->st_gid;
    root->st_uid = src->st_uid;
    //  root->ac_size = src->ac_size;
    root->st_mode = src->st_mode;
    // 复制 st_atim 成员
    root->st_atim.tv_sec = src->st_atim.tv_sec;
    root->st_atim.tv_nsec = src->st_atim.tv_nsec;

    // 复制 addr 数组
    memcpy(root->addr, src->addr, sizeof(src->addr));
}
void writeino(char *buffer, const struct inode *data)
{
    int offset = 0;
    memcpy(buffer + offset, &data->st_mode, sizeof(data->st_mode));
    offset += sizeof(data->st_mode);
    memcpy(buffer + offset, &data->st_ino, sizeof(data->st_ino));
    offset += sizeof(data->st_ino);
    memcpy(buffer + offset, &data->st_nlink, sizeof(data->st_nlink));
    offset += sizeof(data->st_nlink);
    memcpy(buffer + offset, &data->st_uid, sizeof(data->st_uid));
    offset += sizeof(data->st_uid);
    memcpy(buffer + offset, &data->st_gid, sizeof(data->st_gid));
    offset += sizeof(data->st_gid);
    memcpy(buffer + offset, &data->st_size, sizeof(data->st_size));
    offset += sizeof(data->st_size);
    memcpy(buffer + offset, &data->st_atim.tv_sec, sizeof(data->st_atim.tv_sec));
    offset += sizeof(data->st_atim.tv_sec);
    memcpy(buffer + offset, &data->st_atim.tv_nsec, sizeof(data->st_atim.tv_nsec));
    offset += sizeof(data->st_atim.tv_nsec);
    // 写入 addr 数组
    memcpy(buffer + offset, data->addr, sizeof(data->addr));
}

void readino(const char *buffer, struct inode *data)
{
    int offset = 0;
    // 从缓冲区读取数据到 struct inode
    // memcpy(&data->flag, buffer + offset, sizeof(data->flag));
    // offset += sizeof(data->flag);
    memcpy(&data->st_mode, buffer + offset, sizeof(data->st_mode));
    offset += sizeof(data->st_mode);

    memcpy(&data->st_ino, buffer + offset, sizeof(data->st_ino));
    offset += sizeof(data->st_ino);

    memcpy(&data->st_nlink, buffer + offset, sizeof(data->st_nlink));
    offset += sizeof(data->st_nlink);

    memcpy(&data->st_uid, buffer + offset, sizeof(data->st_uid));
    offset += sizeof(data->st_uid);

    memcpy(&data->st_gid, buffer + offset, sizeof(data->st_gid));
    offset += sizeof(data->st_gid);

    memcpy(&data->st_size, buffer + offset, sizeof(data->st_size));
    offset += sizeof(data->st_size);

    // memcpy(&data->ac_size, buffer + offset, sizeof(data->ac_size));
    // offset += sizeof(data->ac_size);

    memcpy(&data->st_atim.tv_sec, buffer + offset, sizeof(data->st_atim.tv_sec));
    offset += sizeof(data->st_atim.tv_sec);

    memcpy(&data->st_atim.tv_nsec, buffer + offset, sizeof(data->st_atim.tv_nsec));
    offset += sizeof(data->st_atim.tv_nsec);

    // 读取 addr 数组
    memcpy(data->addr, buffer + offset, sizeof(data->addr));
}

void writedir(char *buffer, const struct directory *data)
{
    // 写入 directory 结构体数据到缓冲区
    memcpy(buffer, data->name, FILENAME + 1);
    memcpy(buffer + FILENAME + 1, data->expand, EXPAND + 1);
    memcpy(buffer + FILENAME + 1 + EXPAND + 1, &data->st_ino, sizeof(data->st_ino));
}

void readdir(const char *buffer, struct directory *data)
{
    // 从缓冲区读取 directory 结构体数据
    memcpy(data->name, buffer, FILENAME + 1);
    memcpy(data->expand, buffer + FILENAME + 1, EXPAND + 1);
    memcpy(&data->st_ino, buffer + FILENAME + 1 + EXPAND + 1, sizeof(data->st_ino));
}
int findAndSetFirstZeroBit(int *bitmap, int size)
{
    int soi = size / 4;
    for (int i = 0; i < soi; i++)
    {
        for (int j = 0; j < 32; j++)
        {
            int mask = 1 << (31 - j);
            if ((bitmap[i] & mask) == 0)
            {
                // 找到第一个0位，将其设置为1
                // bitmap[i] |= mask;
                return i * 32 + j;
            }
        }
    }
    return -1; // 如果没有找到0位，可以返回一个特殊值或采取其他操作
}
int main()
{
    FILE *fp = NULL;
    fp = fopen("/home/cky/libfuse-master/example/testmount", "r+"); // 打开文件
    if (fp == NULL)
    {
        printf("打开文件失败，文件不存在\n");
        return 0;
    }
    // SB占1块，Inode位图1块，数据块位图4块
    // Inode区占4K*64B/512B=512块
    // 共有8MB/512B=16K=16,384块
    // 根目录的Inode在6块的前64个字节（0-5，共6块被SB，Inode位图，数据块位图占据）
    // 根目录在518块（0-517，共518块被SB，Inode位图，数据块位图，Inode占据）
    // 数据区还剩下16384-518=15,866
    // 1. 初始化SB     大小：1块
    struct sb *super_blk = malloc(sizeof(struct sb)); // 动态内存分配，申请super_blk
    super_blk->fs_size = 16384;
    super_blk->first_blk = 518; // 根目录的data_block在518编号的块中（从0开始编号）
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
    // 519块已经用了，518/32=16余8，还有根目录
    // 用大小为16的int（4字节32位）数组初始化前512块
    if (fseek(fp, BLOCKSIZE * 2, SEEK_SET) != 0) // 首先要将指针移动到文件的2块的起始位置
        fprintf(stderr, "data bitmap fseek failed!\n");
    int a[16];                // 刚好大小为512bit，可以用来初始化数据区位图的前512bit
    memset(a, -1, sizeof(a)); // 补码
    fwrite(a, sizeof(a), 1, fp);
    fseek(fp, BLOCKSIZE * 2 + 16 * 4, SEEK_SET);
    // 512-518
    int b = 0;
    int i = 0;
    int mask = 1;
    mask <<= 25;
    for (int i = 0; i < 7; i++)
    {
        b |= mask;
        mask <<= 1;
    }
    fwrite(&b, sizeof(int), 1, fp);
    fseek(fp, BLOCKSIZE * 2 + 17 * 4, SEEK_SET);
    // 初始化这一块剩下的部分
    // 一块512*8bit，可以用128（128*32=512*8)个整数表示，128-16-1=111
    int c[111];
    memset(c, 0, sizeof(c));
    fwrite(c, sizeof(c), 1, fp);
    // 初始化剩下3块
    fseek(fp, BLOCKSIZE * 3, SEEK_SET);
    // int rest_of_bitmap = (3 * 512) / 4;
    int d[384];
    memset(d, 0, sizeof(d));
    fwrite(d, sizeof(d), 1, fp);
    printf("initial data bitmap success!\n");

    // 初始化inode区
    fseek(fp, BLOCKSIZE * 6, SEEK_SET);
    struct inode *root = malloc(sizeof(struct inode));
    inoinit(root);
    // root->flag = 1;
    root->st_mode = (0x4000 | 0666);
    root->st_nlink = 1;
    // new_ino->st_nlink = 1;
    root->st_uid = getuid();
    root->st_gid = getgid();
    root->addr[0] = 518;
    root->st_ino = 0;
    int i2 = 1;
    for (; i2 < 7; i2++)
    {
        root->addr[i2] = 0;
    }
    clock_gettime(CLOCK_REALTIME, &(root->st_atim));
    char buffer[64];
    memset(buffer, 0, 64);
    writeino(buffer, root);
    fwrite(buffer, 64, 1, fp);
    printf("initial inode area success!\n");
    // 根目录一开始没有目录项
    free(root);
    // 数据区初始化
    for (int i = 518; i < 16384; i++)
    {
        int d[128];
        memset(d, 0, sizeof(d));
        fseek(fp, BLOCKSIZE * i, SEEK_SET);
        fwrite(d, 512, 1, fp);
    }
    fflush(fp);
    // test

    //
    // struct directory *tdir = malloc(sizeof(struct directory));
    // tdir->st_ino = 1;
    // tdir->name[0] = 'a';
    // char tbf1[16] = {0};
    // fseek(fp, 512 * 518, SEEK_SET);
    // writedir(tbf1, tdir);
    // fwrite(tbf1, 16, 1, fp);
    // fflush(fp);

    // struct directory *tdir1 = malloc(sizeof(struct directory));
    // fseek(fp, 512 * 518, SEEK_SET);
    // char tbf11[16] = {0};
    // fread(tbf11, 16, 1, fp);
    // readdir(tbf11, tdir1);
    // printf("is:%s\n", tdir1->name);
    // printf("is:%hd\n", tdir1->st_ino);
    // fflush(fp);
    // //

    fclose(fp);
    printf("all init success!\n");
}
struct sb *super_blk = malloc(sizeof(struct sb));
super_blk->fs_size = 16384; // 8M/512B=16384
super_blk->first_blk = 518;
super_blk->datasize = 15866;             // 数据区还剩下16384-518=15,866
super_blk->first_inode = 6;              // 前面有超级块和两个位图
super_blk->inode_area_size = 512;        // 4K*64B/512B=512
super_blk->fisrt_blk_of_inodebitmap = 1; // 前面有超级块
super_blk->inodebitmap_size = 1;         // 4k bit/512B =512
super_blk->first_blk_of_databitmap = 2;  // 前面有超级块和inode位图
super_blk->databitmap_size = 4;          // 16384/(512B)=4
