#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#define BLOCK_SIZE 512
#define SUPER_BLOCK 0
#define INODE_BITMAP 512
#define DATA_BIMAP 1024
#define INODE 6
#define FILENAME 8
#define EXPAND 3
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
    // 写入struct inode的数据到缓冲区
    // memcpy(buffer + offset, &data->flag, sizeof(data->flag));
    // offset += sizeof(data->flag);
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

    // memcpy(buffer + offset, &data->ac_size, sizeof(data->ac_size));
    // offset += sizeof(data->ac_size);

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
int makenode()
{
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    printf("正在找新inode\n\n");
    if (fp == NULL)
    {
        printf("错误：打开文件失败\n\n");
        return -ENOENT;
    }
    int ino_bit[128];
    int ino_num = -1;
    // int flag = 0;
    //  1.读出inode位图
    fseek(fp, 512, 0);
    fread(ino_bit, sizeof(int[128]), 1, fp);
    for (int i = 0; i < 128; i++)
    {
        int a = 0;
        for (int j = 0; j < 32; j++)
        {
            a = 1 << (31 - j);
            // 2.找寻位图中为0的点位
            if ((ino_bit[i] & a) == 0)
            {
                // flag = 1;
                ino_num = 32 * i + j;
                // 3.将0置1，返回该位置的值
                ino_bit[i] = (ino_bit[i] | a);
                fseek(fp, 512, 0);
                fwrite(ino_bit, sizeof(int[128]), 1, fp);

                fflush(fp);
                fseek(fp, 512, 0);
                fread(ino_bit, sizeof(int[128]), 1, fp);
                printf("数据已经存储！%d\n", ino_bit[i] & a);
                return ino_num;
            }
        }
    }
    printf("creat_inode函数中，未能找到空的块\n\n");
    return -1;
}
int makeblock()
{
    printf("正在找新block\n\n");
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    if (fp == NULL)
    {
        printf("错误:打开文件失败\n\n");
        return -ENOENT;
    }
    int data_bit[512];
    int ino_num = -1;
    // int flag = 0;
    //  1.读出数据块位图
    fseek(fp, 1024, 0);
    fread(data_bit, sizeof(int[512]), 1, fp);
    for (int i = 0; i < 512; i++)
    {
        int a = 0;
        // 2.找寻位图中为0的点位
        for (int j = 0; j < 32; j++)
        {
            a = 1 << (31 - j);
            if ((data_bit[i] & a) == 0)
            {
                //  flag = 1;
                ino_num = 32 * i + j;
                // 3.将0置1，返回该位置的值
                data_bit[i] = data_bit[i] | a;
                fseek(fp, 1024, 0);
                fwrite(data_bit, sizeof(int[512]), 1, fp);
                fflush(fp);
                return ino_num;
            }
        }
    }
    printf("创建数据块时失败！\n\n");
    return -1;
}
int find_ino(char *path)
{
    // return inode
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+"); // 打开文件
    printf("find:搜索路径：%s\n\n", path);
    fseek(fp, INODE * 512, SEEK_SET);
    char buf0[64];
    memset(buf0, 0, 64);
    fread(buf0, 64, 1, fp);
    fclose(fp);
    struct inode *nino = malloc(sizeof(struct inode));
    readino(buf0, nino);
    printf("find:根目录ino：%hd,根目录首地址：%hd\n\n", nino->st_ino, nino->addr[0]);
    if (!strcmp(path, "/"))
    {
        printf("find:要找的就是根目录\n\n");

        free(nino);
        return 0;
    }
    printf("find:要找的不是根目录\n\n");
    char *filename[10];
    char *token = strtok(path, "/");
    int i = 0;
    while (token && i < 10)
    {
        filename[i] = malloc(strlen(token) + 1);
        // strcpy(filename[i], token);
        for (int x = 0; x < strlen(token); x++)
        {
            filename[i][x] = token[x];
        }
        filename[i][strlen(token)] = '\0';
        // printf("赋值\n\n");
        i++;
        // printf("自增\n\n");
        printf("分解%d次的结果是%s\n\n", i, token);
        token = strtok(NULL, "/");
        // printf("更新\n\n");
    }
    int nextino = -1;

    for (int k = 0; k < i; k++)
    {
        printf("一共有%d次现在是第%d次\n\n", i, k);
        int flag = 0;
        printf("find:当前处理: %s\n", filename[k]);
        for (int i = 0; i < 7; i++)
        {
            if (flag)
            {
                break;
            }
            for (int j = 0; j < 512; j += 16)
            {
                fp = fopen(FILEADDR, "r+"); // 打开文件
                fseek(fp, nino->addr[i] * 512 + j, SEEK_SET);
                char buf0[16];
                memset(buf0, 0, 16);
                fread(buf0, 16, 1, fp);
                fclose(fp);
                struct directory *dir = malloc(sizeof(struct directory));
                readdir(buf0, dir);
                if (dir->st_ino == 0)
                {
                    printf("find:当前地址第%d块，块号%hd的第%d目录空，结束\n", i, nino->addr[i], j);
                    free(dir);
                    free(nino);
                    return -1;
                }
                else if (dir->st_ino == -1)
                {
                    free(dir);
                    printf("find:当前地址第%d块，块号%hd的第%d目录是墓碑，跳过\n", i, nino->addr[i], j);
                    continue;
                }
                else
                {
                    if (strlen(dir->expand) == 0)
                    {
                        if (!strcmp(dir->name, filename[k]))
                        {
                            printf("find:当前地址第%d块，块号%hd的第%d目录是匹配\n", i, nino->addr[i], j);
                            nextino = dir->st_ino;
                            free(dir);
                            fp = fopen(FILEADDR, "r+"); // 打开文件
                            printf("find:下一个ino：%d\n\n", nextino);
                            fseek(fp, INODE * 512 + nextino * 64, SEEK_SET);
                            char buf0[64] = {0};
                            fread(buf0, 64, 1, fp);
                            fclose(fp);
                            readino(buf0, nino);
                            flag = 1;
                            break;
                        }
                        else
                        {
                            printf("find:当前地址第%d块，块号%hd的第%d目录不匹配\n", i, nino->addr[i], j);
                            free(dir);
                            continue;
                        }
                    }
                    else
                    {
                        char temp[30];
                        strcpy(temp, filename[k]);
                        char *dot = strrchr(temp, '.');
                        if (dot == NULL)
                        {
                            printf("find:当前地址第%d块，块号%hd的第%d目录不匹配\n", i, nino->addr[i], j);
                            free(dir);
                            continue;
                        }
                        *dot = '\0';
                        dot++;
                        if (!strcmp(dir->name, temp) && !strcmp(dot, dir->expand))
                        {
                            printf("find:当前地址第%d块，块号%hd的第%d目录是匹配\n", i, nino->addr[i], j);
                            nextino = dir->st_ino;
                            free(dir);
                            fp = fopen(FILEADDR, "r+"); // 打开文件
                            printf("find:下一个ino：%d\n\n", nextino);
                            fseek(fp, INODE * 512 + nextino * 64, SEEK_SET);
                            char buf0[64] = {0};
                            fread(buf0, 64, 1, fp);
                            fclose(fp);
                            readino(buf0, nino);
                            flag = 1;
                            break;
                        }
                        else
                        {
                            printf("find:当前地址第%d块，块号%hd的第%d目录不匹配\n", i, nino->addr[i], j);
                            free(dir);
                            continue;
                        }
                    }
                }
            }
        }
        // printf("find:下一个ino：%hd\n\n", nextino);
        if (flag == 0 || nextino == -1)
        {
            printf("find:找不到\n");
            return -1;
        }
        else
        {
            printf("find:下一个ino：%hd\n\n", nextino);
            continue;
        }
        // FILE *fp = NULL;
    }

    free(nino);

    for (int j = 0; j < i; j++)
    {
        free(filename[j]);
    }
    printf("find:完成\n\n");
    return nextino;
}
int delete_inode(int ino_num) // 删除所给定的inode节点
{
    /*
    操作如下：
    1.读出inode位图
    2.找到该位置的位
    3.将1置0，返回该位置的值
    */
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    if (fp == NULL)
    {
        printf("delete inode:打开文件失败\n");
        return -ENOENT;
    }
    // 1.读出inode位图
    fseek(fp, 512, 0);
    int ino_bit[128];
    memset(ino_bit, 0, 512);
    fread(ino_bit, sizeof(int[128]), 1, fp);
    // 2.找到该位置的位
    int pos = ino_num / 32;
    int pos1 = ino_num % 32;
    int a = 1 << (31 - pos1);
    // 检测该节点是否有被使用
    if ((ino_bit[pos] & a) == 0)
    {
        printf("\n所给的inode节点并未被使用\n");
        return -2;
    }
    a = (~a);
    // 3.将1置0，返回该位置的值
    ino_bit[pos] = (ino_bit[pos] & a);
    fseek(fp, 512, 0);
    fwrite(ino_bit, sizeof(int[128]), 1, fp);
    fflush(fp);
    printf("已成功删除节点：%d\n", ino_num);
    return 1;
}
int delete_data(int data_num) // 删除所给定的数据节点
{
    /*
    操作如下：
    1.读出inode位图
    2.找到该位置的位
    3.将1置0，返回该位置的值
    */
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    if (fp == NULL)
    {
        printf("delete data:打开文件失败\n\n");
        return -ENOENT;
    }
    fseek(fp, 512 * data_num, 0);
    int data_bit[512];
    memset(data_bit, 0, 2048);
    fread(data_bit, sizeof(int[512]), 1, fp);
    int pos = data_num / 32;
    int pos1 = data_num % 32;
    int a = 1 << (31 - pos1);
    if ((data_bit[pos] & a) == 0)
    {
        printf("\n所给的data节点并未被使用\n");
        return -2;
    }
    a = ~a;
    data_bit[pos] = data_bit[pos] & a;
    fseek(fp, 512 * data_num, 0);
    fwrite(data_bit, sizeof(int[512]), 1, fp);
    fflush(fp);
    printf("\n已成功删除节点：%d", data_num);
    return 1;
}
/***************************************************************************************************************************/
/*struct stat {
        mode_t     st_mode;       //文件对应的模式，文件，目录等
        ino_t      st_ino;       //inode节点号
        dev_t      st_dev;        //设备号码
        dev_t      st_rdev;       //特殊设备号码
        nlink_t    st_nlink;      //文件的连接数
        uid_t      st_uid;        //文件所有者
        gid_t      st_gid;        //文件所有者对应的组
        off_t      st_size;       //普通文件，对应的文件字节数
        time_t     st_atime;      //文件最后被访问的时间
        time_t     st_mtime;      //文件内容最后被修改的时间
        time_t     st_ctime;      //文件状态改变时间
        blksize_t st_blksize;    //文件内容对应的块大小
        blkcnt_t   st_blocks;     //文件内容对应的块数量
      };*/
// 该函数用于读取文件属性（通过对象的路径获取文件的属性，并赋值给stbuf）
static int SFS_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{

    printf("SFS_getattr：running\n\n");
    char *pathuse = strdup(path);
    printf("SFS_getattr：路径是：%s\n\n", pathuse);
    // printf("SFS_getattr：aa\n\n");
    char *dot = strrchr(pathuse, '.');
    // printf("SFS_getattr：bb\n\n");
    if (dot)
    {
        if (strlen(dot) > 4)
        {
            printf("SFS_getattr：错误，不是有效目录\n\n");
            return -ENOENT;
        }
    }

    int flag = find_ino(pathuse);
    if (flag == -1)
    {
        printf("SFS_getattr：错误，找不到\n\n");

        return -ENOENT;
    }
    memset(stbuf, 0, sizeof(struct stat)); // 将stat结构中成员的值全部置0
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    if (fp == NULL)
    {
        printf("SFS_getattr：打开文件失败\n\n");
        return -ENOENT;
    }
    fseek(fp, 6 * 512 + 64 * flag, SEEK_SET);
    struct inode *target;
    target = malloc(sizeof(struct inode));
    char buf0[64];
    memset(buf0, 0, 64);
    fread(buf0, 64, 1, fp);
    readino(buf0, target);
    stbuf->st_mode = target->st_mode;
    stbuf->st_size = target->st_size;
    free(target);
    fclose(fp);
    printf("SFS_getattr：完成\n\n");
    return 0;
}
// 读取文件时的操作
// 根据路径path找到文件起始位置，再偏移offset长度开始读取size大小的数据到buf中，返回文件大小
// 其中，buf用来存储从path读出来的文件信息，size为文件大小，offset为读取时候的偏移量，fi为fuse的文件信息
// 步骤：① 先读取该path所指文件的file_directory；② 然后根据nStartBlock读出文件内容
static int SFS_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("--------------------------\n");
    printf("SFS_read：running\n");
    printf("--------------------------\n");

    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    if (fp == NULL)
    {
        printf("SFS_read：打开文件失败\n\n");
        return -ENOENT;
    }
    char *pathuse = strdup(path);
    char *lastslash = strrchr(pathuse, '/');
    char *dot = strrchr(lastslash, '/');
    int ino = find_ino(pathuse);
    if (dot == NULL)
    {
        printf("SFS_read:该路径不是文件\n");
        return -EISDIR;
    }
    struct inode *node = malloc(sizeof(struct inode));
    char buf0[64];
    memset(buf0, 0, 64);
    fseek(fp, 512 * 6 + ino * 64, SEEK_SET);
    fread(buf0, 64, 1, fp);
    readino(buf0, node);

    int blocknum = offset / 512;
    int last = offset % 512;
    int remain = size;
    int off = 0;
    while (remain > 0 && blocknum < 7)
    {
        if (node->addr[blocknum] == 0)
        {
            break;
        }
        fseek(fp, 512 * node->addr[blocknum] + last, SEEK_SET);
        if (size < 512 - last)
        {
            fread(buf + off, size, 1, fp);
            break;
        }
        fread(buf + off, 512 - last, 1, fp);
        fflush(fp);
        off += (512 - last);
        remain -= (512 - last);
        last = 0;
        blocknum++;
    }
    // node->st_size = (offset + size) > node->st_size ? (offset + size) : node->st_size;
    // memset(buf0, 0, 64);
    // writeino(buf0, node);
    // fseek(fp, 512 * 6 + 64 * node->st_ino, SEEK_SET);
    // fwrite(buf0, 64, 1, fp);
    free(node);
    fflush(fp);
    fclose(fp);
    printf("SFS_read:文件已写完毕\n");
    return size;
}
// 修改文件,将buf里大小为size的内容，写入path指定的起始块后的第offset
// 步骤：① 找到path所指对象的file_directory；② 根据nStartBlock和offset将内容写入相应位置；
static int SFS_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("--------------------------\n");
    printf("SFS_write：running\n");
    printf("--------------------------\n");
    char *pathuse = strdup(path);
    int ino = find_ino(pathuse);
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    if (fp == NULL)
    {
        printf("SFS_write：打开文件失败\n\n");
        return -ENOENT;
    }
    struct inode *node = malloc(sizeof(struct inode));
    char buf0[64];
    memset(buf0, 0, 64);
    fseek(fp, 512 * 6 + ino * 64, SEEK_SET);
    fread(buf0, 64, 1, fp);
    readino(buf0, node);
    if (offset > node->st_size || (offset + size) > 4 * 512)
    {
        printf("SFS_write：写入的数据越界，超过文件大小\n");
        return -EFBIG;
    }
    int blocknum = offset / 512;
    int last = offset % 512;
    int remain = size;
    int off = 0;
    while (remain > 0 && blocknum < 7)
    {
        if (node->addr[blocknum] == 0)
        {
            node->addr[blocknum] = makeblock();
        }
        fseek(fp, 512 * node->addr[blocknum] + last, SEEK_SET);
        if (size < 512 - last)
        {
            fwrite(buf + off, size, 1, fp);
            break;
        }
        fwrite(buf + off, 512 - last, 1, fp);
        fflush(fp);
        off += (512 - last);
        remain -= (512 - last);
        last = 0;
        blocknum++;
    }
    node->st_size = (offset + size) > node->st_size ? (offset + size) : node->st_size;
    memset(buf0, 0, 64);
    writeino(buf0, node);
    fseek(fp, 512 * 6 + 64 * node->st_ino, SEEK_SET);
    fwrite(buf0, 64, 1, fp);
    free(node);
    fflush(fp);
    fclose(fp);
    printf("SFS_write:文件已写完毕\n");
    return size;
}
// 创建目录
static int SFS_mkdir(const char *path, mode_t mode)
{
    printf("--------------------------\n");
    printf("SFS_mkdir：running\n");
    printf("--------------------------\n");
    char *pathuse = strdup(path);            // const变普通的
    char *lastslash = strrchr(pathuse, '/'); // 最后一个/

    if (strlen(lastslash) > 8) // mark
    {
        printf("SFS_mkdir：错误，文件名太长\n\n");
        return -ENAMETOOLONG;
    }
    if (strchr(lastslash, '.') != NULL)
    {
        printf("SFS_mkdir：错误：路径是文件\n\n");
        return -2;
    }
    if (find_ino(pathuse) != -1)
    {
        printf("SFS_mkdir：错误：已经存在\n\n");
        return -EEXIST;
    }
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    if (fp == NULL)
    {
        printf("SFS_mkdir：打开文件失败\n\n");
        return -ENOENT;
    }

    int newnode = makenode();
    printf("新inode号为%d\n\n", newnode);
    if (newnode < 0)
    {
        return -2;
    }
    struct inode *new_ino = malloc(sizeof(struct inode));
    inoinit(new_ino);
    new_ino->st_mode = (0x4000 | 0666); // 代表创建的是文件夹
    new_ino->st_ino = newnode;
    // new_ino->st_nlink = 1;
    new_ino->st_uid = getuid();
    new_ino->st_gid = getgid();

    int newblock = makeblock(fp);
    printf("新块号为%d\n\n", newblock);
    new_ino->addr[0] = newblock;

    fseek(fp, 512 * 6 + 64 * newnode, 0);
    char buf0[64];
    memset(buf0, 0, 64);
    writeino(buf0, new_ino);
    fwrite(buf0, 64, 1, fp);
    free(new_ino);
    fflush(fp);
    char lastslash1[15];
    strcpy(lastslash1, lastslash + 1);
    if (strlen(pathuse) == strlen(lastslash))
    {
        printf("SFS_mkdir：父级就是根目录\n\n");
        pathuse = "/";
    }
    else
    {
        *lastslash = '\0';
        printf("SFS_mkdir：父级是%s\n\n", pathuse);
    }

    printf("SFS_mkdir：子级是%s\n\n", lastslash1);
    int father = find_ino(pathuse);
    fseek(fp, 512 * 6 + 64 * father, 0);
    struct inode *fino = malloc(sizeof(struct inode));
    char buf1[64];
    memset(buf1, 0, 64);
    fread(buf1, 64, 1, fp);
    readino(buf1, fino);
    printf("SFS_mkdir：上一级ino：%d\n\n", fino->st_ino);
    int pos = 0;
    while (pos < 7 && fino->addr[pos] != 0)
    {
        for (int j = 0; j < 512; j += 16)
        {
            printf("SFS_mkdir:第%d块块号%d的第%d目录：\n\n", pos, fino->addr[pos], j);
            fseek(fp, 512 * fino->addr[pos] + j, 0);
            struct directory *dir = malloc(sizeof(struct directory));
            char buf1[16];
            memset(buf1, 0, 16);
            fread(buf1, 16, 1, fp);
            readdir(buf1, dir);
            if (dir->st_ino == 0)
            {
                printf("SFS_mkdir:找到位置\n\n");

                dir->st_ino = newnode;
                strcpy(dir->name, lastslash1);
                dir->expand[0] = '\0';

                memset(buf1, 0, 16);
                writedir(buf1, dir);
                fseek(fp, 512 * fino->addr[pos] + j, 0);
                fwrite(buf1, 16, 1, fp);
                fflush(fp);

                printf("SFS_mkdir:成功\n\n");
                fino->st_nlink++;
                fseek(fp, fino->st_ino * 64 + 512 * 6, SEEK_SET);
                char buf2[64];
                memset(buf2, 0, 64);
                writeino(buf2, fino);
                fwrite(buf2, 64, 1, fp);
                free(fino);
                free(dir);
                fclose(fp);
                return 0;
            }
            else
            {
                printf("SFS_mkdir:不是位置\n\n");
                free(dir);
                continue;
            }
        }
        pos++;
    }
    if (pos >= 7)
    {
        printf("mkdir:该目录的目录项已经满了，创建失败\n\n");
        free(fino);
        fclose(fp);
        return -2;
    }
    else
    {

        int newblock = makeblock(fp);
        printf("新块号为%d\n\n", newblock);
        fino->addr[pos] = newblock;
        fino->st_nlink++;
        fseek(fp, 512 * 6 + 64 * fino->st_ino, 0);
        char buf0[64];
        memset(buf0, 0, 64);
        writeino(buf0, fino);
        fwrite(buf0, 64, 1, fp);
        free(fino);
        fflush(fp);

        fseek(fp, 512 * newblock, 0);
        struct directory *dir = malloc(sizeof(struct directory));
        char buf1[16];
        memset(buf1, 0, 16);
        dir->st_ino = newnode;
        strcpy(dir->name, lastslash1);
        dir->expand[0] = '\0';
        memset(buf1, 0, 16);
        writedir(buf1, dir);
        fwrite(buf1, 16, 1, fp);
        printf("SFS_mkdir:成功\n\n");
        free(dir);
        fclose(fp);
        return 0;
    }
    printf("mkdir:失败\n\n");
    return -2;
}
// 删除目录
static int SFS_rmdir(const char *path)
{
    printf("-------------------\n");
    printf("SFS_rmdir:running\n");
    printf("-------------------\n");
    // 创建fp
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    if (fp == NULL)
    {
        printf("SFS_rmdir：打开文件失败\n\n");
        return -ENOENT;
    }
    // 找到文件所在的inode

    char pathuse[strlen(path)];
    strcpy(pathuse, path);

    int dir_ino_num = find_ino(pathuse);
    strcpy(pathuse, path);
    if (dir_ino_num == 0 && strcmp(pathuse, "/"))
    {
        printf("SFS_rmdir：该文件夹不存在");
        return -ENOENT;
    }
    fseek(fp, 512 * 6 + 64 * dir_ino_num, 0);
    struct inode *dir_ino = malloc(sizeof(struct inode));
    char buf0[64];
    memset(buf0, 0, 64);
    fread(buf0, 64, 1, fp);
    readino(buf0, dir_ino);
    // 判断是否是文件夹
    if (dir_ino->st_mode != (0x4000 | 0666))
    {
        printf("SFS_rmdir：该文件不是文件夹,ino是%d\n", dir_ino_num);
        return -ENOTDIR;
    }
    // 遍历文件目录项
    if (dir_ino->st_nlink != 0)
    {

        printf("SFS_rmdir：该文件夹目录中仍有文件，不可删除\n");
        return -ENOTEMPTY;
    }
    // 该文件夹中无文件，可以进行删除，首先删除数据区。
    for (int pos = 0; pos < 7; pos++)
    {
        if (dir_ino->addr[pos] != 0)
        {
            delete_data(dir_ino->addr[pos]);
        }
    }
    // 其次删除该文件所在目录
    // 分割路径，找打文件夹所在目录
    int ii = strlen(pathuse) - 1;
    for (; ii > 0; ii--)
    {
        if (pathuse[ii] == '/')
        {
            break;
        }
    }
    char lastslash[strlen(pathuse) - ii]; // 1 for \0
    strcpy(lastslash, pathuse + ii + 1);
    if (ii == 0)
    {
        printf("SFS_rmdir：父级就是根目录\n");
        pathuse[1] = '\0';
    }
    else
    {
        pathuse[ii] = '\0';
        printf("SFS_mkdir：父级是%s\n", pathuse);
    }
    printf("------------------\n");
    printf("路径参数是：%s\n", pathuse);
    printf("要删除的参数是：%s\n", lastslash);
    printf("------------------\n");

    int father_ino_num = find_ino(pathuse);
    fseek(fp, 512 * 6 + 64 * father_ino_num, 0);
    struct inode *father_ino = malloc(sizeof(struct inode));
    memset(buf0, 0, 64);
    fread(buf0, 64, 1, fp);
    readino(buf0, father_ino);
    for (int pos = 0; pos < 4; pos++)
    {
        if (father_ino->addr[pos] == 0)
        {
            continue;
        }
        int flag = 0;
        for (int j = 0; j < 512; j += 16)
        {
            printf("SFS_rmdir:第%d块块号%d的第%d目录:\n", pos, father_ino->addr[pos], j);
            fseek(fp, 512 * father_ino->addr[pos] + j, 0);
            struct directory *dir = malloc(sizeof(struct directory));
            char buf1[16];
            memset(buf1, 0, 16);
            fread(buf1, 16, 1, fp);
            readdir(buf1, dir);
            if (dir->st_ino == dir_ino_num)
            {
                printf("SFS_rmdir:匹配\n");
                dir->st_ino = -1;
                fseek(fp, 512 * father_ino->addr[pos] + j, 0);
                memset(buf1, 0, 16);
                writedir(buf1, dir);
                fwrite(buf1, 16, 1, fp);
                free(dir);
                fflush(fp);
                flag = 1;
                break;
            }
            else
            {
                printf("SFS_rmdir:不匹配\n");
                free(dir);
            }
        }
        if (flag)
        {
            break;
        }
    }
    // 最后删除该文件夹所在的inode。
    delete_inode(dir_ino_num);
    free(dir_ino);
    father_ino->st_nlink--;
    fseek(fp, father_ino->st_ino * 64 + 512 * 6, SEEK_SET);
    char buf2[64];
    memset(buf2, 0, 64);
    writeino(buf2, father_ino);
    fwrite(buf2, 64, 1, fp);

    free(father_ino);
    printf("SFS_rmdir:成功！\n");
    fclose(fp);
    return 0;
}
// 获取文件属性
// gcc -Wall SFS.c `pkg-config fuse3 --cflags --libs` -o SFS
// cd libfuse-master/example
// ./SFS -f fuse
static int SFS_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    printf("SFS_readdir：running\n\n");

    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    if (fp == NULL)
    {
        printf("SFS_readdir：打开文件失败\n\n");
        return -ENOENT;
    }
    // 检查文件路径是否正确
    char *str = strdup(path);
    char *result = strrchr(str, '/'); // 寻找最后一个斜杠
    if (result != NULL)
    {
        result++; // 指向斜杠后的字符
        if (strchr(result, '.') != NULL)
        {
            printf("SFS_readdir:错误：路径是文件\n\n");
            return -ENOENT;
        }
    }
    int ino = find_ino(str);
    printf("SFS_readdir:inode节点为%d\n", ino);
    if (ino == -1)
    {
        printf("SFS_readdir:错误：该文件不存在\n\n");
        return -ENOENT;
    }
    fseek(fp, 512 * 6 + 64 * ino, 0);
    struct inode *dir_ino = malloc(sizeof(struct inode));
    inoinit(dir_ino);
    char buf0[64];
    memset(buf0, 0, 64);
    fread(buf0, 64, 1, fp);
    readino(buf0, dir_ino);
    // 不管如何先添加.与..
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    // 遍历所有目录项
    for (int i = 0; i < 7 && dir_ino->addr[i] != 0; i++)
    {
        int flag = 0;

        for (int j = 0; j < 512; j += 16)
        {
            printf("SFS_readdir:第%d块块号%d的第%d目录：\n\n", i, dir_ino->addr[i], j);
            fseek(fp, 512 * dir_ino->addr[i] + j, 0);
            struct directory *dir = malloc(sizeof(struct directory));
            char buf0[16];
            memset(buf0, 0, 16);
            fread(buf0, 16, 1, fp);
            readdir(buf0, dir);
            if (dir->st_ino != -1 && dir->st_ino != 0)
            {
                char name[15];
                strcpy(name, dir->name);
                if (strlen(dir->expand) != 0)
                {
                    strcat(name, ".");
                    strcat(name, dir->expand);
                }

                printf("SFS_readdir:添加%s\n\n", name);
                filler(buf, name, NULL, 0, 0);
                free(dir);
            }
            else if (dir->st_ino == -1)
            {
                printf("SFS_readdir:是墓碑\n\n");
                free(dir);
            }
            else
            {
                printf("SFS_readdir:到头了\n\n");
                flag = 1;
                free(dir);
                break;
            }
        }
        if (flag)
        {
            break;
        }
    }
    free(dir_ino);
    fclose(fp);
    printf("在MFS_readdir中，遍历成功\n\n");
    return 0;
}
// 创建文件
static int SFS_mknod(const char *path, mode_t mode, dev_t dev)
{
    printf("--------------------------\n");
    printf("SFS_mknod：running\n");
    printf("--------------------------\n");
    char *pathuse = strdup(path);            // const变普通的
    char *lastslash = strrchr(pathuse, '/'); // 最后一个/

    char *dot1 = strchr(lastslash, '.');
    if (dot1 == NULL)
    {
        printf("SFS_mknod：错误：路径是目录\n\n");
        return -2;
    }
    if (strlen(dot1) > 4 || strlen(lastslash) > 13 || (strlen(lastslash) - strlen(dot1)) > 9) // mark
    {
        printf("SFS_mknod：错误，文件名太长\n\n");
        return -ENAMETOOLONG;
    }
    if (find_ino(pathuse) != -1)
    {
        printf("SFS_mknod：错误：已经存在\n\n");
        return -EEXIST;
    }
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    if (fp == NULL)
    {
        printf("SFS_mknod：打开文件失败\n\n");
        return -ENOENT;
    }

    int newnode = makenode();
    printf("新inode号为%d\n\n", newnode);
    if (newnode < 0)
    {
        return -2;
    }
    struct inode *new_ino = malloc(sizeof(struct inode));
    inoinit(new_ino);
    new_ino->st_mode = (0x8000 | 0666); // 代表创建的是文件夹
    new_ino->st_ino = newnode;
    // new_ino->st_nlink = 1;
    new_ino->st_uid = getuid();
    new_ino->st_gid = getgid();

    int newblock = makeblock(fp);
    printf("新块号为%d\n\n", newblock);
    new_ino->addr[0] = newblock;

    fseek(fp, 512 * 6 + 64 * newnode, 0);
    char buf0[64];
    memset(buf0, 0, 64);
    writeino(buf0, new_ino);
    fwrite(buf0, 64, 1, fp);
    free(new_ino);
    fflush(fp);
    char dot2[4];
    strcpy(dot2, dot1 + 1);
    *dot1 = '\0';
    char lastslash1[15];
    strcpy(lastslash1, lastslash + 1);
    if (strlen(pathuse) == strlen(lastslash))
    {
        printf("SFS_mknod：父级就是根目录\n\n");
        pathuse = "/";
    }
    else
    {
        lastslash = '\0';
        printf("SFS_mknod：父级是%s\n\n", pathuse);
    }

    printf("SFS_mknod：子级是%s\n\n", lastslash1);
    printf("SFS_mknod：子级扩展是%s\n\n", dot2);
    int father = find_ino(pathuse);
    fseek(fp, 512 * 6 + 64 * father, 0);
    struct inode *fino = malloc(sizeof(struct inode));
    char buf1[64];
    memset(buf1, 0, 64);
    fread(buf1, 64, 1, fp);
    readino(buf1, fino);
    printf("SFS_mknod：上一级ino：%d\n\n", fino->st_ino);
    int pos = 0;
    while (pos < 7 && fino->addr[pos] != 0)
    {
        for (int j = 0; j < 512; j += 16)
        {
            printf("SFS_mknod:第%d块块号%d的第%d目录：\n\n", pos, fino->addr[pos], j);
            fseek(fp, 512 * fino->addr[pos] + j, 0);
            struct directory *dir = malloc(sizeof(struct directory));
            char buf1[16];
            memset(buf1, 0, 16);
            fread(buf1, 16, 1, fp);
            readdir(buf1, dir);
            if (dir->st_ino == 0)
            {
                printf("SFS_mknod:找到位置\n\n");
                dir->st_ino = newnode;
                strcpy(dir->name, lastslash1);
                strcpy(dir->expand, dot2);
                memset(buf1, 0, 16);
                writedir(buf1, dir);
                fseek(fp, 512 * fino->addr[pos] + j, 0);
                fwrite(buf1, 16, 1, fp);
                fflush(fp);
                printf("SFS_mknod:成功\n\n");

                fino->st_nlink++;
                fseek(fp, fino->st_ino * 64 + 512 * 6, SEEK_SET);
                char buf2[64];
                memset(buf2, 0, 64);
                writeino(buf2, fino);
                fwrite(buf2, 64, 1, fp);

                free(fino);
                free(dir);
                fclose(fp);
                return 0;
            }
            else
            {
                printf("SFS_mknod:不是位置\n\n");
                free(dir);
                continue;
            }
        }
        pos++;
    }
    if (pos >= 7)
    {
        printf("mknod:该目录的目录项已经满了，创建失败\n\n");
        free(fino);
        fclose(fp);
        return -2;
    }
    else
    {

        int newblock = makeblock(fp);
        printf("新块号为%d\n\n", newblock);
        fino->addr[pos] = newblock;
        fino->st_nlink++;

        fseek(fp, 512 * 6 + 64 * fino->st_ino, 0);
        char buf0[64];
        memset(buf0, 0, 64);
        writeino(buf0, fino);
        fwrite(buf0, 64, 1, fp);
        free(fino);
        fflush(fp);

        fseek(fp, 512 * newblock, 0);
        struct directory *dir = malloc(sizeof(struct directory));
        char buf1[16];
        memset(buf1, 0, 16);
        dir->st_ino = newnode;
        strcpy(dir->name, lastslash1);
        strcpy(dir->expand, dot2);
        memset(buf1, 0, 16);
        writedir(buf1, dir);
        fwrite(buf1, 16, 1, fp);
        printf("SFS_mknod成功\n\n");
        free(dir);
        fclose(fp);
        return 0;
    }
    printf("mknod:失败\n\n");
    return -2;
}
// 删除文件
static int SFS_unlink(const char *path, mode_t mode, dev_t dev)
{
    printf("--------------\n");
    printf("SFS_unlink:running\n\n");
    printf("--------------\n");
    // 创建fp
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    if (fp == NULL)
    {
        printf("SFS_unlink:打开文件失败\n\n");
        return -ENOENT;
    }
    /*步骤如下
    1.找到文件所属的ino节点
    2.删除该节点以及其所有连接的数据区的块
    3.找到该文件的目录inode
    4.删除所在目录inode的属于该文件的目录项
    */
    // 1.找到文件所属的ino节点
    char *pathuse = strdup(path);
    char *slash = strrchr(pathuse, '/');
    char *dot = strrchr(slash, '.');
    if (dot == NULL)
    {
        printf("SFS_unlink:是目录不是文件\n");
        return -EISDIR;
    }
    int ino = find_ino(pathuse);
    if (ino < 0)
    {
        printf("SFS_unlink:该文件不存在\n");
        return -ENOENT;
    }
    fseek(fp, 512 * 6 + 64 * ino, 0);
    struct inode *nino = malloc(sizeof(struct inode));
    char buf0[64];
    memset(buf0, 0, 64);
    fread(buf0, 64, 1, fp);
    readino(buf0, nino);
    for (int i = 0; i < 4; i++)
    {
        if (nino->addr[i] != 0)
        {
            delete_data(nino->addr[i]);
        }
    }
    char pathuse1[strlen(path)];
    strcpy(pathuse1, path);

    int ii = strlen(pathuse1) - 1;
    for (; ii > 0; ii--)
    {
        if (pathuse1[ii] == '/')
        {
            break;
        }
    }
    char lastslash[strlen(pathuse1) - ii]; // 1 for \0
    strcpy(lastslash, pathuse1 + ii + 1);
    if (ii == 0)
    {
        printf("SFS_unlink：父级就是根目录\n");
        pathuse1[1] = '\0';
    }
    else
    {
        pathuse1[ii] = '\0';
        printf("SFS_unlink：父级是%s\n", pathuse1);
    }
    printf("------------------\n");
    printf("路径参数是：%s\n", pathuse1);
    printf("要删除的参数是：%s\n", lastslash);
    printf("------------------\n");
    int father_ino_num = find_ino(pathuse1);

    fseek(fp, 512 * 6 + 64 * father_ino_num, SEEK_SET);
    struct inode *father_ino = malloc(sizeof(struct inode));
    memset(buf0, 0, 64);
    fread(buf0, 64, 1, fp);
    readino(buf0, father_ino);
    for (int pos = 0; pos < 7; pos++)
    {
        if (father_ino->addr[pos] == 0)
        {
            continue;
        }
        int flag = 0;
        for (int j = 0; j < 512; j += 16)
        {
            printf("SFS_unlink:第%d块块号%d的第%d目录:\n", pos, father_ino->addr[pos], j);
            fseek(fp, 512 * father_ino->addr[pos] + j, 0);
            struct directory *dir = malloc(sizeof(struct directory));
            char buf1[16];
            memset(buf1, 0, 16);
            fread(buf1, 16, 1, fp);
            readdir(buf1, dir);
            if (dir->st_ino == ino)
            {
                printf("SFS_unlink:匹配\n");
                dir->st_ino = -1;
                fseek(fp, 512 * father_ino->addr[pos] + j, 0);
                memset(buf1, 0, 16);
                writedir(buf1, dir);
                fwrite(buf1, 16, 1, fp);
                free(dir);
                fflush(fp);
                flag = 1;
                break;
            }
            else
            {
                printf("SFS_unlink:不匹配\n");
                free(dir);
            }
        }
        if (flag)
        {
            break;
        }
    }
    delete_inode(ino);
    free(nino);
    father_ino->st_nlink--;
    fseek(fp, father_ino->st_ino * 64 + 512 * 6, SEEK_SET);
    char buf2[64];
    memset(buf2, 0, 64);
    writeino(buf2, father_ino);
    fwrite(buf2, 64, 1, fp);

    free(father_ino);
    printf("SFS_unlink:成功！\n");
    fclose(fp);
    return 0;
}

static struct fuse_operations SFS_oper = {

    .getattr = SFS_getattr, // 获取文件属性（包括目录的）
    .mknod = SFS_mknod,     // 创建文件
    .unlink = SFS_unlink,   // 删除文件
    .read = SFS_read,       // 读取文件内容
    .write = SFS_write,     // 修改文件内容
    .mkdir = SFS_mkdir,     // 创建目录
    .rmdir = SFS_rmdir,     // 删除目录
    .readdir = SFS_readdir, // 读取目录
};
int main(int argc, char *argv[])
{
    umask(0);
    return fuse_main(argc, argv, &SFS_oper, NULL);
}
