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
long TOTAL_BLOCK_NUM;
#define FILENAME 8
#define EXPAND 3
#define FILEADDR ""
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
    off_t ac_size;
    struct timespec st_atim; /* 16个字节time of last access */
    short int addr[7];       /*磁盘地址，14字节*/
};

struct stat
{
    int is_file;
    short int st_mode; /* 权限，2字节 */
    off_t st_size;     /*文件大小，4字节 */
};

struct directory
{ // 共16字节
    char name[9];
    char expand[4];
    short int st_ino; /* i-node号，2字节 */
    // st_ino=-1, is deleted, a tomb in here
    // 3字节备用
};
void inoinit(struct inode *root)
{
    root->st_ino = 0;
    root->st_size = 0;
    root->st_nlink = 0;
    root->st_gid = 0;
    root->st_uid = 0;
    root->ac_size = 0;
    root->st_mode = 0;
}
void copyInode(struct inode *root, const struct inode *src)
{
    // 浅复制 inode 结构体的非嵌套部分

    root->st_ino = src->st_ino;
    root->st_size = src->st_size;
    root->st_nlink = src->st_nlink;
    root->st_gid = src->st_gid;
    root->st_uid = src->st_uid;
    root->ac_size = src->ac_size;
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

    memcpy(buffer + offset, &data->ac_size, sizeof(data->ac_size));
    offset += sizeof(data->ac_size);

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

    memcpy(&data->ac_size, buffer + offset, sizeof(data->ac_size));
    offset += sizeof(data->ac_size);

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

short int find_in_block(char *file_name, char *extensions, int off, off_t size)
{
    struct directory *dir = malloc(sizeof(struct directory));
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    short int i = 0;
    short int res = -1;
    while (i < size)
    {
        fseek(fp, off, SEEK_SET);
        char buffer[16] = {0};
        fread(buffer, 16, 1, fp);
        readdir(buffer, dir);
        if (strcmp(file_name, dir->name) && strcmp(extensions, dir->expand))
        {
            res = dir->st_ino;
            free(dir);
            fclose(fp);
            return res;
        }
        off += 16;
        i += 16;
    }
    free(dir);
    fclose(fp);
    return res;
}
short int find_once_indirect(char *file_name, char *extensions, short int addr, off_t size)
{
    short int res = -1;
    short int offset = 0;
    // 在一级间接里找
    int block_bit = size - 2048;
    int block_num = block_bit / 512;
    int last_block = block_bit % 512;
    short int *ndir = NULL;
    int noff = 0;
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    int i = 0;
    for (; i < block_num; i++)
    {
        offset = addr * 512 + i * 2;
        fseek(fp, offset, SEEK_SET);
        fread(ndir, sizeof(short int), 1, fp);
        noff = (*ndir) * 512;
        res = find_in_block(file_name, extensions, noff, 512);
        if (res != -1)
        {
            free(ndir);
            fclose(fp);
            return res;
        }
    }
    if (last_block != 0)
    {
        offset = addr * 512 + i * 2;
        fseek(fp, offset, SEEK_SET);
        fread(ndir, sizeof(short int), 1, fp);
        noff = (*ndir) * 512;
        res = find_in_block(file_name, extensions, noff, last_block);
    }
    free(ndir);
    fclose(fp);
    return res;
}
short int find_twice_indirect(char *file_name, char *extensions, short int addr, off_t size)
{
    short int res = -1;
    short int offset = 0;
    offset = addr * 512;
    int block_bit = size - 133120;
    int block_num_2 = block_bit / (512 * 256);
    int last_block_2 = block_bit % (512 * 256);
    short int *ndir = NULL;
    int naddr = 0;
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    int i = 0;
    for (; i < block_num_2; i++)
    {
        offset = addr * 512 + i * 2;
        fseek(fp, offset, SEEK_SET);
        fread(ndir, sizeof(short int), 1, fp);
        naddr = (*ndir);
        res = find_once_indirect(file_name, extensions, naddr, 133120);
        if (res != -1)
        {
            free(ndir);
            fclose(fp);
            return res;
        }
    }
    if (last_block_2 != 0)
    {
        offset = addr * 512 + i * 2;
        fseek(fp, offset, SEEK_SET);
        fread(naddr, sizeof(short int), 1, fp);
        naddr = (*ndir);
        res = find_once_indirect(file_name, extensions, naddr, last_block_2);
    }
    free(ndir);
    fclose(fp);
    return res;
}
short int find(char *file_name, char *extensions, char addr[], off_t size)
{
    // 0，1，2，3直接，4一级间接，5二级间接，6三级间接
    // 目录项16字节，一个块512字节，可以放32个
    // 地址项2字节，一个块可以放256个
    short int res = -1;
    short int offset = 0;
    if (size <= 2048)
    { // 不大于4*512=2048的，直接访问
        int time = size / 512;
        int remind = time % 512;
        int index = 0;
        while (time)
        {
            offset = addr[index] * 512;
            res = find_in_block(file_name, extensions, offset, 512);
            if (res != -1)
            {
                return res;
            }
            time--;
            index++;
        }
        if (remind != 0)
        {
            offset = addr[index] * 512;
            res = find_in_block(file_name, extensions, offset, remind);
        }
        return res;
    }
    else if (size <= 133120)
    { // 不大于2048+256*512=133120的，直接加一级
        int time = 4;
        int index = 0;
        while (time)
        {
            offset = addr[index] * 512;
            res = find_in_block(file_name, extensions, offset, 512);
            if (res != -1)
            {
                return res;
            }
            time--;
            index++;
        }
        res = find_once_indirect(file_name, extensions, addr[4], size);
        return res;
    }
    else if (size <= 33687552)
    { // 不大于133120+256*256*512=33687552的，再加二级
        int time = 4;
        int index = 0;
        while (time)
        {
            offset = addr[index] * 512;
            res = find_in_block(file_name, extensions, offset, 512);
            if (res != -1)
            {
                return res;
            }
            time--;
            index++;
        }
        res = find_once_indirect(file_name, extensions, addr[4], 133120);
        if (res != -1)
        {
            return res;
        }
        // 启动第二级
        res = find_twice_indirect(file_name, extensions, addr[5], size);
        return res;
    }
    else
    { // 剩下的都是上了三级的
        int time = 4;
        int index = 0;
        while (time)
        {
            offset = addr[index] * 512;
            res = find_in_block(file_name, extensions, offset, 512);
            if (res != -1)
            {
                return res;
            }
            time--;
            index++;
        }
        res = find_once_indirect(file_name, extensions, addr[4], 133120);
        if (res != -1)
        {
            return res;
        }
        // 启动第二级
        res = find_twice_indirect(file_name, extensions, addr[5], 33687552);
        if (res != -1)
        {
            return res;
        }
        // 第三级
        offset = addr[6] * 512;
        int block_bit = size - 33687552;
        int block_num_3 = block_bit / (512 * 256 * 256);
        int last_block_3 = block_bit % (512 * 256 * 256);
        short int *ndir = NULL;
        int naddr = 0;
        FILE *fp = NULL;
        fp = fopen(FILEADDR, "r+");
        int i = 0;
        for (; i < block_num_3; i++)
        {
            offset = addr[6] * 512 + i * 2;
            fseek(fp, offset, SEEK_SET);
            fread(ndir, sizeof(short int), 1, fp);
            naddr = (*ndir);
            res = find_twice_indirect(file_name, extensions, naddr, 33687552);
            if (res != -1)
            {
                free(ndir);
                fclose(fp);
                return res;
            }
        }
        if (last_block_3 != 0)
        {
            offset = addr[6] * 512 + i * 2;
            fseek(fp, offset, SEEK_SET);
            fread(naddr, sizeof(short int), 1, fp);
            naddr = (*ndir);
            res = find_twice_indirect(file_name, extensions, naddr, last_block_3);
        }
        free(ndir);
        fclose(fp);
        return res;
    }
}
int get_fd_to_attr(char *path, struct inode *io)
{
    // 0 not exist
    // 1 exist and is dir
    // 2 exsit and is file
    char *token = strtok((char *)path, "/");
    char *file_names[100]; // 假设最多有10层
    char *extensions[100];
    int i = 0;
    while (token != NULL)
    {
        file_names[i] = token;
        char *dot = strrchr(file_names[i], '.');
        if (dot != NULL)
        {
            extensions[i] = dot + 1;
            *dot = '\0'; // 在点之前截断文件名
        }
        else
        {
            extensions[i] = ""; // 如果没有后缀，设置为空字符串
        }
        token = strtok(NULL, "/");
        i++;
    }
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+"); // 打开文件
    int offset = 6 * 512;       // inode of root dir
    short int curnode = 0;
    struct inode *now = malloc(sizeof(struct inode));
    for (int j = 0; j < i; j++)
    {
        fseek(fp, offset, SEEK_SET);
        char *buffer[64] = {0};
        fread(buffer, 64, 1, fp);
        readino(buffer, now);
        if (strcmp(file_names[i], "") && strcmp(extensions[i], ""))
        {
            // 就是当前这个目录
            // mark
            memcpy(io, now, sizeof(struct inode));
            io = malloc(sizeof(struct inode));
            copyInode(io, now);
            free(now);
            fclose(fp);
            return 1;
        }

        if (now->st_size == 0)
        {
            // 这一级目录空的，肯定没有你要找的东西
            free(now);
            fclose(fp);
            return 0;
        }
        curnode = find(file_names[i], extensions[i], now->addr, now->st_size);
        if (curnode != -1)
        {
            // 找到下一级的inode号了
            offset = 6 * 512 + curnode * 64;
            continue;
        }
        else
        {
            // 找不到
            free(now);
            fclose(fp);
            return 0;
        }
    }
    fseek(fp, offset, SEEK_SET);
    char *buffer[64] = {0};
    fread(buffer, 64, 1, fp);
    readino(buffer, now);
    // mark
    io = malloc(sizeof(struct inode));
    copyInode(io, now);
    free(now);
    fclose(fp);
    return 2;
}
static int SFS_getattr(const char *path1, struct stat *stbuf, struct fuse_file_info *fi)
{
    char *path;
    memcpy(path, path1, strlen(path1) + 1);
    //(void) fi;
    int res = 0;
    struct inode *io = malloc(sizeof(struct inode));
    int type = get_fd_to_attr(path, io);
    // 非根目录
    if (type == 0)
    {
        free(io);
        printf("SFS_getattr：get_fd_to_attr时发生错误，函数结束返回\n\n");
        memset(stbuf, 0, sizeof(struct stat));
        // 将stat结构中成员的值全部置0
        return -ENOENT;
    }
    else if (type == 1)
    { // 从path判断这个文件是		一个目录	还是	一般文件
        printf("SFS_getattr：这个file_directory是一个目录\n\n");
        stbuf->st_mode = S_IFDIR | 0666; // 设置成目录,S_IFDIR和0666（8进制的文件权限掩码），这里进行或运算
        stbuf->st_size = io->st_size;
        stbuf->is_file = 0;
    }
    else if (type == 2)
    {
        printf("SFS_getattr：这个file_directory是一个文件\n\n");
        stbuf->st_mode = S_IFREG | 0666; // 该文件是	一般文件
        stbuf->st_size = io->st_size;
        stbuf->is_file = 1;
        // stbuf->st_nlink = 1;
    }
    else
    {
        printf("SFS_getattr：这个文件（目录）不存在，函数结束返回\n\n");
        res = -ENOENT;
    } // 文件不存在

    printf("SFS_getattr：getattr成功，函数结束返回\n\n");
    free(io);
    return res;
}
void list_block(void *buf, int off, off_t size, fuse_fill_dir_t &filler)
{
    struct directory *dir = malloc(sizeof(struct directory));
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    short int i = 0;
    char name[8 + 3 + 2];
    while (i < size)
    {
        fseek(fp, off, SEEK_SET);
        char *buffer[16] = {0};
        fread(buffer, 16, 1, fp);
        readdir(buffer, dir);
        // 复制过去
        if (dir->st_ino != -1)
        {
            strcpy(name, dir->name);
            if (strlen(dir->expand) != 0)
            {
                strcat(name, ".");
                strcat(name, dir->expand);
            }
            filler(buf, name, NULL, 0, 0);
        }
        off += 16;
        i += 16;
    }
    free(dir);
    fclose(fp);
};
void list_once(void *buf, short int addr, int st_size, fuse_fill_dir_t &filler)
{
    // if (st_size <= 2048)
    // {
    //     list_block(buf, addr * 512, st_size, filler);
    // }
    int time0 = st_size >= 133120 ? 256 : (st_size - 2048) / (512);
    int last0 = st_size >= 133120 ? 0 : (st_size - 2048) % (512);
    short int offset = 0;
    // 在一级间接里找
    short int *ndir = NULL;
    int noff = 0;
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    int i = 0;
    for (; i < time0; i++)
    {
        offset = addr * 512 + i * 2;
        fseek(fp, offset, SEEK_SET);
        fread(ndir, sizeof(short int), 1, fp);
        noff = (*ndir) * 512;
        list_block(buf, noff, 512, filler);
    }
    if (last0 != 0)
    {
        offset = addr * 512 + i * 2;
        fseek(fp, offset, SEEK_SET);
        fread(ndir, sizeof(short int), 1, fp);
        noff = (*ndir) * 512;
        list_block(buf, noff, last0, filler);
    }
    free(ndir);
    fclose(fp);
}
void list_twice(void *buf, short int addr, int st_size, fuse_fill_dir_t &filler)
{
    // if (st_size <= 133120)
    // {
    //     list_once(buf, addr, st_size, filler);
    //     return;
    // }
    int time0 = st_size >= 33687552 ? 256 : (st_size - 133120) / (512 * 256);
    int last0 = st_size >= 33687552 ? 0 : (st_size - 133120) % (512 * 256);
    short int offset = 0;
    // 在一级间接里找
    short int *ndir = NULL;
    int noff = 0;
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    int i = 0;
    for (; i < time0; i++)
    {
        offset = addr * 512 + i * 2;
        fseek(fp, offset, SEEK_SET);
        fread(ndir, sizeof(short int), 1, fp);
        // noff = (*ndir) * 512;
        list_once(buf, ndir, 133120, filler);
    }
    if (last0 != 0)
    {
        offset = addr * 512 + i * 2;
        fseek(fp, offset, SEEK_SET);
        fread(ndir, sizeof(short int), 1, fp);
        // noff = (*ndir) * 512;
        list_once(buf, ndir, last0, filler);
    }
    free(ndir);
    fclose(fp);
}
static int SFS_readdir(const char *path1, void *buf, fuse_fill_dir_t filleroff_t offset, struct fuse_file_info *fi) //,enum use_readdir_flags flags)
{
    char *path;
    memcpy(path, path1, strlen(path1) + 1);
    struct inode *io = malloc(sizeof(struct inode));
    // 打开path指定的文件，将文件属性读到io中
    if (get_fd_to_attr(path, io) != 1)
    { // 不是目录，退出
        free(io);
        return -ENOENT;
    }
    // 无论是什么目录，先用filler函数添加 . 和 ..
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    // 按顺序查找,并向buf添加目录内的文件和目录名
    int index = 0;
    int time0 = io->st_size >= 2048 ? 4 : io->st_size / 512;
    int last0 = io->st_size >= 2048 ? 0 : io->st_size % 512;
    int offset = 0;
    // 直接
    while (time0)
    {
        offset = io->addr[index] * 512;
        // 读一个块
        list_block(buf, offset, 512, filler);
        time0--;
        index++;
    }
    if (last0 != 0)
    {
        offset = io->addr[index] * 512;
        list_block(buf, offset, last0, filler);
        // 读剩下的
    }
    // 一级
    if (io->st_size > 2048)
    {

        list_once(buf, io->addr[4], io->st_size, filler);
    }
    // 二级
    if (io->st_size > 133120)
    {
        // time0 = io->st_size >= 33687552 ? 256 : (io->st_size - 133120) / (512 * 256);
        // last0 = io->st_size >= 33687552 ? 0 : (io->st_size - 133120) % (512 * 256);
        list_twice(buf, io->addr[5], io->st_size, filler);
    }
    // 三级
    if (io->st_size > 33687552)
    {
        time0 = (io->st_size - 33687552) / (512 * 256 * 256);
        last0 = (io->st_size - 33687552) % (512 * 256 * 256);

        FILE *fp = NULL;
        fp = fopen(FILEADDR, "r+");

        int i = 0;
        short int *ndir = NULL;
        int naddr = 0;
        for (; i < time0; i++)
        {
            offset = io->addr[6] * 512 + i * 2;
            fseek(fp, offset, SEEK_SET);
            fread(ndir, sizeof(short int), 1, fp);
            naddr = (*ndir);
            list_twice(buf, naddr, 33687552, filler);
        }
        if (last0 != 0)
        {
            offset = io->addr[6] * 512 + i * 2;
            fseek(fp, offset, SEEK_SET);
            fread(ndir, sizeof(short int), 1, fp);
            naddr = (*ndir);
            list_twice(buf, naddr, last0, filler);
            // res = find_in_block(file_name, extensions, naddr, last_block_3);
        }
        free(ndir);
        fclose(fp);
    }
    free(io);
    return 0;
    // fill的定义：
    //	typedef int (*fuse_fill_dir_t) (void *buf, const char *name, const struct stat *stbuf, off_t off);
    //	其作用是在readdir函数中增加一个目录项或者文件
}
int findAndSetFirstZeroBit(unsigned char *bitmap, int size)
{
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            unsigned char mask = 1 << j;
            if ((bitmap[i] & mask) == 0)
            {
                // 找到第一个0位，将其设置为1
                bitmap[i] |= mask;
                return i * 8 + j;
            }
        }
    }
    return -1; // 如果没有找到0位，可以返回一个特殊值或采取其他操作
}
void mkd(char *name, struct inode *root, char *ex)
{
    if (root->st_size < 2048)
    {
        int block = root->st_size / 512;
        int inblock = root->st_size % 512;
        FILE *fp = NULL;
        fp = fopen(FILEADDR, "r+");

        struct directory *ndir = malloc(sizeof(struct directory));
        memcpy(ndir->name, name, strlen(name) + 1);
        if (ex == NULL)
        {
            ndir->expand[0] = '\0';
        }
        else
        {
            memcpy(ndir->expand, ex, strlen(ex) + 1);
        }
        // 搞个inode号
        // 同时把inode位图的那一位置1
        unsigned char bitmap[512];
        fseek(fp, 512, SEEK_SET);
        fread(bitmap, 500, 1, fp);
        int node = findFirstSetBit(bitmap, 512);
        fwrite(bitmap, 500, 1, fp);
        // 如果正好新开一块，找个新块号，改位图和addr
        if (inblock == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            fread(datamap, 2048, 1, fp);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            fwrite(datamap, 2048, 1, fp);
            root->addr[block] = nblock;
        }
        // 目录项写回去
        ndir->st_ino = node;
        fseek(fp, root->addr[block] * 512 + inblock, SEEK_SET);
        char buffer[16] = {0};
        writedir(buffer, ndir);
        fwrite(buffer, 16, 1, fp);
        // 改inode区
        struct inode *nnode = malloc(sizeof(struct inode));
        nnode->st_ino = node;
        nnode->st_nlink = 0;
        nnode->ac_size = 0;
        nnode->st_size = 0;
        fseek(fp, 512 + 64 * node, SEEK_SET);
        char buffer[64] = {0};
        writeino(buffer, nnode);
        fwrite(buffer, 64, 1, fp);
        // 改root
        root->st_nlink++;
        fseek(fp, 512 * 6, SEEK_SET);
        char buffer[64] = {0};
        writeino(buffer, root);
        fwrite(buffer, 64, 1, fp);
        free(nnode);
        free(ndir);
        fclose(fp);
        return;
    }
    else if (root->st_size < 133120)
    {
        int block = (root->st_size - 2048) / 512;
        int inblock = (root->st_size - 2048) % 512;
        FILE *fp = NULL;
        fp = fopen(FILEADDR, "r+");
        short int t1 = 0;
        struct directory *ndir = malloc(sizeof(struct directory));
        memcpy(ndir->name, name, strlen(name) + 1);
        if (ex == NULL)
        {
            ndir->expand[0] = '\0';
        }
        else
        {
            memcpy(ndir->expand, ex, strlen(ex) + 1);
        }
        // 搞个inode号
        // 同时把inode位图的那一位置1
        unsigned char bitmap[512];
        fseek(fp, 512, SEEK_SET);
        fread(bitmap, 500, 1, fp);
        int node = findFirstSetBit(bitmap, 512);
        fwrite(bitmap, 500, 1, fp);
        // 如果正好新开一块，找个新块号，改位图和addr
        if (root->st_size == 2048)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            fread(datamap, 2048, 1, fp);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            fwrite(datamap, 2048, 1, fp);
            root->addr[4] = nblock;
        }
        if (inblock == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            fread(datamap, 2048, 1, fp);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            fwrite(datamap, 2048, 1, fp);

            short int *tmp = malloc(2);
            *tmp = nblock;
            t1 = nblock;
            fseek(fp, root->addr[4] * 512 + block * 2, SEEK_SET);
            fwrite(tmp, 2, 1, fp);
            free(tmp);
        }

        short int *tmp = malloc(2);
        fseek(fp, root->addr[4] * 512 + 2 * block, SEEK_SET);
        fread(tmp, 2, 1, fp);
        t1 = *tmp;
        free(tmp);
        fseek(fp, t1 * 512 + inblock, SEEK_SET);
        // 目录项写回去
        ndir->st_ino = node;
        char buffer[16] = {0};
        writedir(buffer, ndir);
        fwrite(buffer, 16, 1, fp);
        // 改inode区
        struct inode *nnode = malloc(sizeof(struct inode));
        nnode->st_ino = node;
        nnode->st_nlink = 0;
        nnode->ac_size = 0;
        nnode->st_size = 0;
        fseek(fp, 512 + 64 * node, SEEK_SET);
        char buffer[64] = {0};
        writeino(buffer, nnode);
        fwrite(buffer, 64, 1, fp);
        // 改root
        root->st_nlink++;
        fseek(fp, 512 * 6, SEEK_SET);
        char buffer[64] = {0};
        writeino(buffer, root);
        fwrite(buffer, 64, 1, fp);
        free(nnode);
        free(ndir);
        fclose(fp);
        return;
    }
    else if (root->st_size < 33687552)
    {
        int block = (root->st_size - 133120) / (512 * 256);
        int inblock = (root->st_size - 133120) % (512 * 256);
        FILE *fp = NULL;
        fp = fopen(FILEADDR, "r+");
        short int t1 = 0;
        short int t2 = 0;
        struct directory *ndir = malloc(sizeof(struct directory));
        memcpy(ndir->name, name, strlen(name) + 1);
        if (ex == NULL)
        {
            ndir->expand[0] = '\0';
        }
        else
        {
            memcpy(ndir->expand, ex, strlen(ex) + 1);
        }
        // 搞个inode号
        // 同时把inode位图的那一位置1
        unsigned char bitmap[512];
        fseek(fp, 512, SEEK_SET);
        fread(bitmap, 500, 1, fp);
        int node = findFirstSetBit(bitmap, 512);
        fwrite(bitmap, 500, 1, fp);
        // 如果正好新开一块，找个新块号，改位图和addr
        if (root->st_size == 133120)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            fread(datamap, 2048, 1, fp);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            fwrite(datamap, 2048, 1, fp);
            root->addr[5] = nblock;
        }
        if (inblock == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            fread(datamap, 2048, 1, fp);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            fwrite(datamap, 2048, 1, fp);

            short int *tmp = malloc(2);
            *tmp = nblock;
            t1 = nblock;
            fseek(fp, root->addr[5] * 512 + block * 2, SEEK_SET);
            fwrite(tmp, 2, 1, fp);
            free(tmp);
        }
        short int *tmp = malloc(2);
        fseek(fp, root->addr[4] * 512 + 2 * block, SEEK_SET);
        fread(tmp, 2, 1, fp);
        t1 = *tmp;
        free(tmp);
        if (inblock % 512 == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            fread(datamap, 2048, 1, fp);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            fwrite(datamap, 2048, 1, fp);

            short int *tmp = malloc(2);
            *tmp = nblock;
            t2 = nblock;
            fseek(fp, t1 * 512 + (inblock / 512) * 2, SEEK_SET);
            fwrite(tmp, 2, 1, fp);
            free(tmp);
        }
        short int *tmp = malloc(2);
        fseek(fp, t1 * 512 + (inblock / 512) * 2, SEEK_SET);
        fwrite(tmp, 2, 1, fp);
        t2 = *tmp;
        free(tmp);
        fseek(fp, t2 * 512 + (inblock % 512), SEEK_SET);
        // 目录项写回去
        ndir->st_ino = node;
        char buffer[16] = {0};
        writedir(buffer, ndir);
        fwrite(buffer, 16, 1, fp);
        // 改inode区
        struct inode *nnode = malloc(sizeof(struct inode));
        nnode->st_ino = node;
        nnode->st_nlink = 0;
        nnode->ac_size = 0;
        nnode->st_size = 0;
        fseek(fp, 512 + 64 * node, SEEK_SET);
        char buffer[64] = {0};
        writeino(buffer, nnode);
        fwrite(buffer, 64, 1, fp);
        // 改root
        root->st_nlink++;
        fseek(fp, 512 * 6, SEEK_SET);
        char buffer[64] = {0};
        writeino(buffer, root);
        fwrite(buffer, 64, 1, fp);
        free(nnode);
        free(ndir);
        fclose(fp);
        return;
    }
    else
    {
        int block = (root->st_size - 33687552) / (512 * 256 * 256);
        int inblock = (root->st_size - 33687552) % (512 * 256 * 256);
        FILE *fp = NULL;
        fp = fopen(FILEADDR, "r+");
        short int t1 = 0;
        short int t2 = 0;
        short int t3 = 0;
        struct directory *ndir = malloc(sizeof(struct directory));
        memcpy(ndir->name, name, strlen(name) + 1);
        if (ex == NULL)
        {
            ndir->expand[0] = '\0';
        }
        else
        {
            memcpy(ndir->expand, ex, strlen(ex) + 1);
        }
        // 搞个inode号
        // 同时把inode位图的那一位置1
        unsigned char bitmap[512];
        fseek(fp, 512, SEEK_SET);
        fread(bitmap, 500, 1, fp);
        int node = findFirstSetBit(bitmap, 512);
        fwrite(bitmap, 500, 1, fp);
        // 如果正好新开一块，找个新块号，改位图和addr
        if (root->st_size == 33687552)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            fread(datamap, 2048, 1, fp);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            fwrite(datamap, 2048, 1, fp);
            root->addr[6] = nblock;
        }
        if (inblock == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            fread(datamap, 2048, 1, fp);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            fwrite(datamap, 2048, 1, fp);

            short int *tmp = malloc(2);
            *tmp = nblock;
            t1 = nblock;
            fseek(fp, root->addr[6] * 512 + block * 2, SEEK_SET);
            fwrite(tmp, 2, 1, fp);
            free(tmp);
        }
        short int *tmp = malloc(2);
        fseek(fp, root->addr[6] * 512 + 2 * block, SEEK_SET);
        fread(tmp, 2, 1, fp);
        t1 = *tmp;
        free(tmp);
        if (inblock % (256 * 512) == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            fread(datamap, 2048, 1, fp);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            fwrite(datamap, 2048, 1, fp);

            short int *tmp = malloc(2);
            *tmp = nblock;
            t2 = nblock;
            fseek(fp, t1 * 512 + (inblock / (256 * 512)) * 2, SEEK_SET);
            fwrite(tmp, 2, 1, fp);
            free(tmp);
        }
        short int *tmp = malloc(2);
        fseek(fp, t1 * 512 + (inblock / (256 * 512)) * 2, SEEK_SET);
        fwrite(tmp, 2, 1, fp);
        t2 = *tmp;
        free(tmp);
        // fseek(fp, t2 * 512 + (inblock % (256 * 512)) * 2, SEEK_SET);
        int ninblock = inblock % (256 * 512);
        if (ninblock % 512 == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            fread(datamap, 20481, 1, fp);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            fwrite(datamap, 2048, 1, fp);

            short int *tmp = malloc(2);
            *tmp = nblock;
            t3 = nblock;
            fseek(fp, t2 * 512 + (ninblock / (512)) * 2, SEEK_SET);
            fwrite(tmp, 2, 1, fp);
            free(tmp);
        }
        short int *tmp = malloc(2);
        fseek(fp, t2 * 512 + (ninblock / (512)) * 2, SEEK_SET);
        t3 = *tmp;
        fwrite(tmp, 2, 1, fp);
        free(tmp);
        fseek(fp, t3 * 512 + (ninblock % 512), SEEK_SET);
        // 目录项写回去
        ndir->st_ino = node;
        char buffer[16] = {0};
        writedir(buffer, ndir);
        fwrite(buffer, 16, 1, fp);
        // 改inode区
        struct inode *nnode = malloc(sizeof(struct inode));
        inoinit(nnode);
        nnode->st_ino = node;
        nnode->st_nlink = 0;
        nnode->ac_size = 0;
        nnode->st_size = 0;
        fseek(fp, 512 + 64 * node, SEEK_SET);
        char buffer[64] = {0};
        writeino(buffer, nnode);
        fwrite(buffer, 64, 1, fp);
        // 改root
        root->st_nlink++;
        fseek(fp, 512 * 6, SEEK_SET);
        char buffer[64] = {0};
        writeino(buffer, root);
        fwrite(buffer, 64, 1, fp);
        free(nnode);
        free(ndir);
        fclose(fp);
        return;
    }
}
static int SFS_mkdir(const char *name1)
{
    char *name;
    memcpy(name, name1, strlen(name1) + 1);
    if (strlen(name) > 8)
    {
        printf("SFS_mkdir：文件名过长\n\n");
        return -ENAMETOOLONG;
    }
    for (int i = 0; i < strlen(name); i++)
    {
        if (name[i] == '/')
        {
            printf("SFS_mkdir：要创建的目录不在根目录下\n\n");
            return -EPERM;
        }
    }
    char *ex = "\0";
    struct inode *root = malloc(sizeof(struct inode));
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    fseek(fp, 6 * 512, SEEK_SET);
    char *buffer[64] = {0};
    fread(buffer, 64, 1, fp);
    readino(buffer, root);
    if (find(name, ex, root->addr, root->st_size) != -1)
    {
        free(root);
        fclose(fp);
        return -EEXIST;
    }
    mkd(name, root, NULL);
    free(root);
    fclose(fp);
    printf("SFS_mkdir：成功\n\n");
    return 0;
}
static int SFS_mknod(const char *name1, const char *path1, dev_t dev)
{
    char *path;
    char *name;
    memcpy(name, name1, strlen(name1) + 1);
    memcpy(path, path1, strlen(path1) + 1);
    if (strcmp(path, "/"))
    {
        printf("错误：SFS_mknod：根目录\n\n");
        return -EPERM;
    }
    char *tmp = NULL;
    memcpy(tmp, name, strlen(name) + 1);
    char *dot = strrchr(tmp, '.');
    if (strlen(name) > 8 || strlen(dot) > 3)
    {
        printf("错误：SFS_mknod：文件名过长\n\n");
        return -ENAMETOOLONG;
    }
    memcpy(tmp, path, strlen(path) + 1);
    strcat(tmp, "/");
    strcat(tmp, name);
    struct inode *io = malloc(sizeof(struct inode));
    if (get_fd_to_attr(tmp, io) != 0)
    {
        printf("错误：SFS_mknod：已经存在\n\n");
        return -EEXIST;
    }
    get_fd_to_attr(path, io);
    char *dot = strrchr(name, '.');
    mkd(name, io, dot);
    printf("错误：SFS_mknod：成功\n\n");
    return 0;
}
static int SFS_read(const char *path1, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    char *path;
    memcpy(path, path1, strlen(path1) + 1);
    struct inode *io = malloc(sizeof(struct inode));
    char *start = buf;
    if (get_fd_to_attr(path, io) != 2)
    {
        printf("错误：SFS_read：是目录不是文件\n\n");
        return -EISDIR;
    }
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    // 全部读了，再偏移
    short int blocknum = io->st_size / 512;
    int blocklast = io->st_size % 512;
    char *block = malloc(512);
    short int *t1 = malloc(2);
    short int *t2 = malloc(2);
    short int *t3 = malloc(2);
    for (int i = 0; i < blocknum; i++)
    {
        if (i < 4)
        {
            fseek(fp, io->addr[i] * 512, SEEK_SET);
            fread(block, 512, 1, fp);
            memcpy(buf, block, 512);
            buf += 512;
        }
        else if (i < 260)
        {
            fseek(fp, io->addr[4] * 512 + (i - 4) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512, SEEK_SET);
            fread(block, 512, 1, fp);
            memcpy(buf, block, 512);
            buf += 512;
        }
        else if (i < 65796)
        {
            fseek(fp, io->addr[5] * 512 + ((i - 260) / 256) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512 + 2 * ((i - 260) % 256), SEEK_SET);
            fread(t2, 2, 1, fp);
            fseek(fp, (*t2) * 512, SEEK_SET);
            fread(block, 512, 1, fp);
            memcpy(buf, block, 512);
            buf += 512;
        }
        else
        {
            fseek(fp, io->addr[6] * 512 + ((i - 65796) / (256 * 256)) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512 + ((i - 65796) % (256 * 256)) * 2, SEEK_SET);
            fread(t2, 2, 1, fp);
            fseek(fp, (*t2) * 512 + 2 * (((i - 65796) % (256 * 256)) / 256), SEEK_SET);
            fread(t3, 2, 1, fp);
            fseek(fp, (*t3) * 512, SEEK_SET);
            fread(block, 512, 1, fp);
            memcpy(buf, block, 512);
            buf += 512;
        }
    }
    if (blocklast != 0)
    {
        char *last = malloc(blocklast);
        if (blocknum < 4)
        {
            fseek(fp, io->addr[blocknum] * 512, SEEK_SET);
            fread(last, blocklast, 1, fp);
            memcpy(buf, last, blocklast);
        }
        else if (blocknum < 260)
        {
            fseek(fp, io->addr[4] * 512 + (blocknum - 4) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512, SEEK_SET);
            fread(last, blocklast, 1, fp);
            memcpy(buf, last, blocklast);
        }
        else if (blocknum < 65796)
        {
            fseek(fp, io->addr[5] * 512 + ((blocknum - 260) / 256) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512 + 2 * ((blocknum - 260) % 256), SEEK_SET);
            fread(t2, 2, 1, fp);
            fseek(fp, (*t2) * 512, SEEK_SET);
            fread(last, blocklast, 1, fp);
            memcpy(buf, last, blocklast);
        }
        else
        {
            fseek(fp, io->addr[6] * 512 + ((blocknum - 65796) / (256 * 256)) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512 + ((blocknum - 65796) % (256 * 256)) * 2, SEEK_SET);
            fread(t2, 2, 1, fp);
            fseek(fp, (*t2) * 512 + 2 * (((blocknum - 65796) % (256 * 256)) / 256), SEEK_SET);
            fread(t3, 2, 1, fp);
            fseek(fp, (*t3) * 512, SEEK_SET);
            fread(last, blocklast, 1, fp);
            memcpy(buf, last, blocklast);
        }
    }
    buf = start;
    buf += offset;
    printf("错误：SFS_read：成功\n\n");
    return 0;
}
int find_and_remove(short int block, off_t size, char *na, char *ex)
{
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    struct directory *dir = malloc(sizeof(struct directory));
    int i = 0;
    for (; i < size; i += 16)
    {
        fseek(fp, block * 512 + i, SEEK_SET);
        char *buffer[16] = {0};
        fread(buffer, 16, 1, fp);
        readfir(buffer, dir);
        if (strcmp(na, dir->name) && strcmp(ex, dir->expand))
        {
            dir->st_ino = -1;
            char buffer[16] = {0};
            writedir(buffer, dir);
            fwrite(buffer, 16, 1, fp);
            free(dir);
            fclose(fp);
            return 1;
        }
    }
    free(dir);
    fclose(fp);
    return 0;
}
void remove1(struct inode *io, char *na, char *ex)
{
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    // 全部读了，再偏移
    short int blocknum = io->st_size / 512;
    int blocklast = io->st_size % 512;

    short int *t1 = malloc(2);
    short int *t2 = malloc(2);
    short int *t3 = malloc(2);
    for (int i = 0; i < blocknum; i++)
    {
        if (i < 4)
        {
            if (find_and_remove(io->addr[i], 512, na, ex))
            {
                return;
            }
        }
        else if (i < 260)
        {
            fseek(fp, io->addr[4] * 512 + (i - 4) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            if (find_and_remove((*t1), 512, na, ex))
            {
                return;
            }
        }
        else if (i < 65796)
        {
            fseek(fp, io->addr[5] * 512 + ((i - 260) / 256) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512 + 2 * ((i - 260) % 256), SEEK_SET);
            fread(t2, 2, 1, fp);
            if (find_and_remove((*t2), 512, na, ex))
            {
                return;
            }
        }
        else
        {
            fseek(fp, io->addr[6] * 512 + ((i - 65796) / (256 * 256)) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512 + ((i - 65796) % (256 * 256)) * 2, SEEK_SET);
            fread(t2, 2, 1, fp);
            fseek(fp, (*t2) * 512 + 2 * (((i - 65796) % (256 * 256)) / 256), SEEK_SET);
            fread(t3, 2, 1, fp);
            if (find_and_remove((*t3), 512, na, ex))
            {
                return;
            }
        }
    }
    if (blocklast != 0)
    {

        if (blocknum < 4)
        {

            if (find_and_remove(io->addr[blocknum], blocklast, na, ex))
            {
                return;
            }
        }
        else if (blocknum < 260)
        {
            fseek(fp, io->addr[4] * 512 + (blocknum - 4) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            if (find_and_remove((*t1), blocklast, na, ex))
            {
                return;
            }
        }
        else if (blocknum < 65796)
        {
            fseek(fp, io->addr[5] * 512 + ((blocknum - 260) / 256) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512 + 2 * ((blocknum - 260) % 256), SEEK_SET);
            fread(t2, 2, 1, fp);
            if (find_and_remove((*t2), blocklast, na, ex))
            {
                return;
            }
        }
        else
        {
            fseek(fp, io->addr[6] * 512 + ((blocknum - 65796) / (256 * 256)) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512 + ((blocknum - 65796) % (256 * 256)) * 2, SEEK_SET);
            fread(t2, 2, 1, fp);
            fseek(fp, (*t2) * 512 + 2 * (((blocknum - 65796) % (256 * 256)) / 256), SEEK_SET);
            fread(t3, 2, 1, fp);
            if (find_and_remove((*t3), blocklast, na, ex))
            {
                return;
            }
        }
    }
}
void clear(int block)
{
    int myArray[64];
    memset(myArray, 0, sizeof(myArray));
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    fseek(fp, 512 * block, SEEK_SET);
    fwrite(myArray, 512, 1, fp);
    int bitmap[2048];
    fseek(fp, 512 * 2, SEEK_SET);
    fread(bitmap, 2048, 1, fp);
    int byteIndex = block / 8;
    int bitOffset = block % 8;

    // 使用位运算将指定位设置为0
    bitmap[byteIndex] &= ~(1 << bitOffset);
    fwrite(bitmap, 2048, 1, fp);
    fclose(fp);
    return;
}
void removeblock(struct inode *io)
{
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    // 全部读了，再偏移
    short int blocknum = io->st_size / 512;
    int blocklast = io->st_size % 512;

    short int *t1 = malloc(2);
    short int *t2 = malloc(2);
    short int *t3 = malloc(2);
    for (int i = 0; i < blocknum; i++)
    {
        if (i < 4)
        {
            clear(io->addr[i]);
        }
        else if (i < 260)
        {
            fseek(fp, io->addr[4] * 512 + (i - 4) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            (clear((*t1)));
        }
        else if (i < 65796)
        {
            fseek(fp, io->addr[5] * 512 + ((i - 260) / 256) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512 + 2 * ((i - 260) % 256), SEEK_SET);
            fread(t2, 2, 1, fp);
            clear((*t2));
        }
        else
        {
            fseek(fp, io->addr[6] * 512 + ((i - 65796) / (256 * 256)) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512 + ((i - 65796) % (256 * 256)) * 2, SEEK_SET);
            fread(t2, 2, 1, fp);
            fseek(fp, (*t2) * 512 + 2 * (((i - 65796) % (256 * 256)) / 256), SEEK_SET);
            fread(t3, 2, 1, fp);
            clear((*t3));
        }
    }
    if (blocklast != 0)
    {

        if (blocknum < 4)
        {

            clear(io->addr[blocknum]);
        }
        else if (blocknum < 260)
        {
            fseek(fp, io->addr[4] * 512 + (blocknum - 4) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            clear((*t1));
        }
        else if (blocknum < 65796)
        {
            fseek(fp, io->addr[5] * 512 + ((blocknum - 260) / 256) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512 + 2 * ((blocknum - 260) % 256), SEEK_SET);
            fread(t2, 2, 1, fp);
            clear((*t2));
        }
        else
        {
            fseek(fp, io->addr[6] * 512 + ((blocknum - 65796) / (256 * 256)) * 2, SEEK_SET);
            fread(t1, 2, 1, fp);
            fseek(fp, (*t1) * 512 + ((blocknum - 65796) % (256 * 256)) * 2, SEEK_SET);
            fread(t3, 2, 1, fp);
            fseek(fp, (*t2) * 512 + 2 * (((blocknum - 65796) % (256 * 256)) / 256), SEEK_SET);
            fread(t3, 2, 1, fp);
            clear((*t3));
        }
    }
}
static int SFS_unlink(const char *path1)
{
    char *path;
    memcpy(path, path1, strlen(path1) + 1);
    struct inode *io = malloc(sizeof(struct inode));
    int flag = get_fd_to_attr(path, io);
    char *name;
    char *expand;
    if (flag == 0)
    {
        printf("SFS_unlink：错误，不存在\n\n");
        return -ENOENT;
    }
    if (flag == 1)
    {
        printf("SFS_unlink：错误，是目录\n\n");
        return -EISDIR;
    }
    char *lastSlash = strrchr(path, '/');
    if (lastSlash != NULL)
    {
        // 找到"."的位置
        char *dot = strrchr(lastSlash, '.');

        if (dot != NULL)
        {
            // 拷贝"name"部分
            strncpy(name, lastSlash + 1, dot - lastSlash - 1);
            name[dot - lastSlash - 1] = '\0';

            // 拷贝"expand"部分
            strcpy(expand, dot + 1);

            // 在"."的位置设置为字符串结束标志
            *dot = '\0';
        }
    }
    struct inode *io2 = malloc(sizeof(struct inode));
    get_fd_to_attr(path, io2);
    remove1(io2, name, expand);
    // 最后改
    io2->ac_size -= io->ac_size;
    io2->st_nlink--;
    // 释放块空间
    removeblock(io);
    // inode区删自己，父级写回去
    int del[8];
    memset(del, 0, sizeof(del));
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    fseek(fp, 512 * 6 + 64 * io->st_ino, SEEK_SET);
    char buffer[64] = {0};
    writeino(buffer, del);
    fwrite(buffer, 64, 1, fp);
    fseek(fp, 512 * 6 + 64 * io2->st_ino, SEEK_SET);
    char buffer[64] = {0};
    writeino(buffer, io2);
    fwrite(buffer, 64, 1, fp);
    // inode位图
    int inomap[500];
    fseek(fp, 512, SEEK_SET);
    fread(inomap, 500, 1, fp);
    int byteIndex = io->st_ino / 8;
    int bitOffset = io->st_ino % 8;

    // 使用位运算将指定位设置为0
    inomap[byteIndex] &= ~(1 << bitOffset);
    fwrite(inomap, 500, 1, fp);
    free(io);
    free(io2);
    fclose(fp);
    printf("SFS_unlink：成功\n\n");
    return 0;
}
static int SFS_rmdir(const char *path1)
{
    char *path;
    memcpy(path, path1, strlen(path1) + 1);
    struct inode *io = malloc(sizeof(struct inode));
    int flag = get_fd_to_attr(path, io);
    char *name;
    char *expand[3];
    expand[0] = '\0';
    if (flag == 0)
    {
        printf("SFS_rmdir：错误，不存在\n\n");
        return -ENOENT;
    }
    if (flag == 2)
    {
        printf("SFS_rmdir：错误，不是目录\n\n");
        return -ENOTDIR;
    }
    if (io->ac_size != 0)
    {
        printf("SFS_rmdir：错误，目录非空\n\n");
        return -ENOTEMPTY;
    }
    char *lastSlash = strrchr(path, '/');
    if (lastSlash != NULL)
    {
        // 找到"/"的位置
        strcpy(name, lastSlash + 1);
        *lastSlash = '\0';
    }
    struct inode *io2 = malloc(sizeof(struct inode));
    get_fd_to_attr(path, io2);
    remove1(io2, name, expand);
    // 最后改
    io2->ac_size -= io->ac_size;
    io2->st_nlink--;
    // 释放块空间
    removeblock(io);
    // inode区删自己，父级写回去
    int del[8];
    memset(del, 0, sizeof(del));
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    fseek(fp, 512 * 6 + 64 * io->st_ino, SEEK_SET);
    char buffer[64] = {0};
    writeino(buffer, del);
    fwrite(buffer, 64, 1, fp);
    fseek(fp, 512 * 6 + 64 * io2->st_ino, SEEK_SET);
    char buffer[64] = {0};
    writeino(buffer, io2);
    fwrite(buffer, 64, 1, fp);
    // inode位图
    int inomap[500];
    fseek(fp, 512, SEEK_SET);
    fread(inomap, 500, 1, fp);
    int byteIndex = io->st_ino / 8;
    int bitOffset = io->st_ino % 8;

    // 使用位运算将指定位设置为0
    inomap[byteIndex] &= ~(1 << bitOffset);
    fwrite(inomap, 500, 1, fp);
    free(io);
    free(io2);
    fclose(fp);
    printf("SFS_rmdir：成功\n\n");
    return 0;
}
static int SFS_write(char *path, char *buf, off_t size, off_t offset)
{
    struct inode *io = malloc(sizeof(struct inode));
    int flag = get_fd_to_attr(path, io);
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");

    if (flag == 0)
    {
        printf("SFS_write：错误，不存在\n\n");
        return -ENOENT;
    }
    if (flag == 2)
    {
        printf("SFS_write：错误，是文件\n\n");
        return -ENOTDIR;
    }
    if (offset > io->st_size)
    {
        printf("SFS_write：太大了\n\n");
        return -EFBIG;
    }
    int blockindex = offset / 512;
    int blockstart = offset % 512;
    int remain = size;

    while (remain > 0 && blockindex < 260)
    {
        if (blockindex < 4)
        {
            if (io->addr[blockindex] == 0)
            {
                unsigned char datamap[2048];
                fseek(fp, 1024, SEEK_SET);
                fread(datamap, 2048, 1, fp);
                int nblock = findAndSetFirstZeroBit(datamap, 2048);
                fwrite(datamap, 2048, 1, fp);
                io->addr[blockindex] = nblock;
            }
            fseek(fp, io->addr[blockindex] * 512 + blockstart, SEEK_SET);
            if (remain < 512 - blockstart)
            {
                fwrite(buf, remain, 1, fp);
                // 改inode
                if (io->ac_size < offset + size)
                {
                    io->ac_size = offset + size;
                }
                if (io->st_size < offset + size)
                {
                    io->st_size = offset + size;
                }
                char bu[64] = {0};
                writeino(bu, io);
                fseek(fp, 6 * 512 + io->st_ino * 64, SEEK_SET);
                fwrite(bu, 64, 1, fp);
                free(io);
                fclose(fp);
                return 0;
            }
            else
            {
                fwrite(buf, 512 - blockstart, 1, fp);
                remain -= (512 - blockstart);
                blockindex++;
                buf += (512 - blockstart);
                blockstart = 0;
            }
        }
        else
        { //<260
            if (io->addr[4] == 0)
            { // 还没启动过一级
                unsigned char datamap[2048];
                fseek(fp, 1024, SEEK_SET);
                fread(datamap, 2048, 1, fp);
                int nblock = findAndSetFirstZeroBit(datamap, 2048);
                fwrite(datamap, 2048, 1, fp);
                io->addr[4] = nblock;
            }
            short int *tmp = malloc(2);
            fseek(fp, io->addr[4] * 512 + (blockindex - 4) * 2, SEEK_SET);
            fread(tmp, 2, 1, fp);
            if (tmp == 0)
            { // 这一块刚好是空的
                unsigned char datamap[2048];
                fseek(fp, 1024, SEEK_SET);
                fread(datamap, 2048, 1, fp);
                int nblock = findAndSetFirstZeroBit(datamap, 2048);
                fwrite(datamap, 2048, 1, fp);
                *tmp = nblock;
                fwrite(tmp, 2, 1, fp);
            }
            fseek(fp, (*tmp) * 512, SEEK_SET);
            if (remain < 512 - blockstart)
            {
                fwrite(buf, remain, 1, fp);
                // 改inode
                if (io->ac_size < offset + size)
                {
                    io->ac_size = offset + size;
                }
                if (io->st_size < offset + size)
                {
                    io->st_size = offset + size;
                }
                char bu[64] = {0};
                writeino(bu, io);
                fseek(fp, 6 * 512 + io->st_ino * 64, SEEK_SET);
                fwrite(bu, 64, 1, fp);
                free(io);
                free(tmp);
                fclose(fp);
                return 0;
            }
            else
            {
                free(tmp);
                fwrite(buf, 512 - blockstart, 1, fp);
                remain -= (512 - blockstart);
                blockindex++;
                buf += (512 - blockstart);
                blockstart = 0;
            }
        }
    }

    if (io->ac_size < offset + size - remain)
    {
        io->ac_size = offset + size - remain;
    }
    if (io->st_size < offset + size - remain)
    {
        io->st_size = offset + size - remain;
    }
    char bu[64] = {0};
    writeino(bu, io);
    fseek(fp, 6 * 512 + io->st_ino * 64, SEEK_SET);
    fwrite(bu, 64, 1, fp);
    free(io);
    fclose(fp);
    if (remain > 0)
    {
        printf("SFS_write：错误，太大了但是写了260\n\n");
        // 写了260个
        return -EFBIG;
    }
    else
    {
        printf("SFS_write：成功\n\n");
        return 0;
    }
}
void write_to_file_system(char *buf, int size, int offset, int *addr, int num_blocks)
{
    int current_offset = offset; // 当前写入位置
    int remaining_size = size;   // 剩余要写入的数据大小
    int block_index = 0;         // 当前块号索引

    while (remaining_size > 0 && block_index < num_blocks)
    {
        int block_start = addr[block_index] * 512;                                                          // 当前块的起始位置
        int block_end = (block_index + 1 < num_blocks) ? addr[block_index + 1] * 512 : (block_start + 512); // 当前块的结束位置

        // 计算当前块中可写入的数据大小
        int bytes_to_write = (remaining_size < (block_end - current_offset)) ? remaining_size : (block_end - current_offset);

        // 复制数据到当前块
        memcpy(&buf[offset - block_start], &buf[size - remaining_size], bytes_to_write);

        // 更新偏移和剩余数据大小
        current_offset += bytes_to_write;
        remaining_size -= bytes_to_write;

        // 如果当前块已经写满，移动到下一个块
        if (current_offset >= block_end)
        {
            block_index++;
        }
    }

    // 这里需要更新文件系统的块分配信息，确保标记已使用的块

    // 最后，你可能需要更新文件的元数据，如文件大小等信息
}
static void *SFS_init(struct fuse_conn_info *conn)
{
    printf("SFS_init：函数开始\n\n");
    (void)conn;

    // 用超级块中的fs_size初始化全局变量
    TOTAL_BLOCK_NUM = 16384； fclose(fp);
    printf("SFS_init：函数结束返回\n\n");
    // return (long*)TOTAL_BLOCK_NUM;
}
static struct fuse_operations SFS_opener
{
    .init = SFS_init,
    .getattr = SFS_getattr,
    .readdir = SFS_readdir,
    .mkdir = SFS_mkdir,
    .rmdir = SFS_rmdir,
    .mknod = SFS_mknod,
    .write = SFS_write,
    .read = SFS_read,
    .unlink = SFS_unlink,
};
int main(int argc, char *argv[])
{
    /*
    int ret;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);//初始化

    //设置默认值——我们必须使用strdup，以便fuse-opt-parse可以在用户指定其他值时释放默认值
    options.filename = strdup("hello");
    options.contents = strdup("Hello World!\n");
    // Parse options
    if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)//fuse_opt_parse出问题了，直接返回
        return 1;
    // 当指定--help时，首先打印我们自己的文件系统特定的帮助文本(show_help)，然后信号fuse_main显示附加帮助\
        （fuse_main再次向选项添加“--help”）而不使用：line（通过将argv[0]设置为空字符串）
    if (options.show_help) {//如果options里面有 -h
        show_help(argv[0]);
        assert(fuse_opt_add_arg(&args, "--help") == 0);//添加--help选项
        args.argv[0] = (char*) "";
    }
    ret = fuse_main(args.argc, args.argv, &MFS_oper, NULL);
    fuse_opt_free_args(&args);
    return ret;*/
    umask(0);
    return fuse_main(argc, argv, &SFS_oper, NULL);
}