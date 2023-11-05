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
        read(fp, dir, sizeof(struct directory));
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
        read(fp, ndir, sizeof(short int));
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
        read(fp, ndir, sizeof(short int));
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
        read(fp, ndir, sizeof(short int));
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
        read(fp, naddr, sizeof(short int));
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
            read(fp, ndir, sizeof(short int));
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
            read(fp, naddr, sizeof(short int));
            naddr = (*ndir);
            res = find_twice_indirect(file_name, extensions, naddr, last_block_3);
        }
        free(ndir);
        fclose(fp);
        return res;
    }
}
int get_fd_to_attr(const char *path, struct inode *io)
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
        read(fp, now, sizeof(struct inode));
        if (strcmp(file_names[i], "") && strcmp(extensions[i], ""))
        {
            // 就是当前这个目录
            memcpy(io, now, sizeof(struct inode));
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
    read(fp, now, sizeof(struct inode));
    memcpy(io, now, sizeof(struct inode));
    free(now);
    fclose(fp);
    return 2;
}
static int SFS_getattr(const char *path, struct stat *stbuf)
{
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

    printf("FS_getattr：getattr成功，函数结束返回\n\n");
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
        read(fp, dir, sizeof(struct directory));
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
        read(fp, ndir, sizeof(short int));
        noff = (*ndir) * 512;
        list_block(buf, noff, 512, filler);
    }
    if (last0 != 0)
    {
        offset = addr * 512 + i * 2;
        fseek(fp, offset, SEEK_SET);
        read(fp, ndir, sizeof(short int));
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
        read(fp, ndir, sizeof(short int));
        // noff = (*ndir) * 512;
        list_once(buf, ndir, 133120, filler);
    }
    if (last0 != 0)
    {
        offset = addr * 512 + i * 2;
        fseek(fp, offset, SEEK_SET);
        read(fp, ndir, sizeof(short int));
        // noff = (*ndir) * 512;
        list_once(buf, ndir, last0, filler);
    }
    free(ndir);
    fclose(fp);
}
static int MFS_readdir(const char *path, void *buf, fuse_fill_dir_t filler) //,enum use_readdir_flags flags)
{
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
            read(fp, ndir, sizeof(short int));
            naddr = (*ndir);
            list_twice(buf, naddr, 33687552, filler);
        }
        if (last0 != 0)
        {
            offset = io->addr[6] * 512 + i * 2;
            fseek(fp, offset, SEEK_SET);
            read(fp, ndir, sizeof(short int));
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
void mkd(char *name, struct inode *root)
{
    if (root->st_size < 2048)
    {
        int block = root->st_size / 512;
        int inblock = root->st_size % 512;
        FILE *fp = NULL;
        fp = fopen(FILEADDR, "r+");

        struct directory *ndir = malloc(sizeof(struct directory));
        memcpy(ndir->name, name, strlen(name));
        ndir->expand[0] = '\0';
        // 搞个inode号
        // 同时把inode位图的那一位置1
        unsigned char bitmap[512];
        fseek(fp, 512, SEEK_SET);
        read(fp, bitmap, 500);
        int node = findFirstSetBit(bitmap, 512);
        write(fp, bitmap, 500);
        // 如果正好新开一块，找个新块号，改位图和addr
        if (inblock == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            read(fp, datamap, 2048);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            write(fp, datamap, 2048);
            root->addr[block] = nblock;
        }
        // 目录项写回去
        ndir->st_ino = node;
        fseek(fp, root->addr[block] * 512 + inblock, SEEK_SET);
        write(fp, ndir, sizeof(struct directory));
        // 改inode区
        struct inode *nnode = malloc(sizeof(struct inode));
        nnode->st_ino = node;
        nnode->st_nlink = 0;
        nnode->ac_size = 0;
        nnode->st_size = 0;
        fseek(fp, 512 + 64 * node, SEEK_SET);
        write(fp, nnode, sizeof(struct inode));
        // 改root
        root->st_nlink++;
        fseek(fp, 512 * 6, SEEK_SET);
        write(fp, root, sizeof(struct inode));
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
        memcpy(ndir->name, name, strlen(name));
        ndir->expand[0] = '\0';
        // 搞个inode号
        // 同时把inode位图的那一位置1
        unsigned char bitmap[512];
        fseek(fp, 512, SEEK_SET);
        read(fp, bitmap, 500);
        int node = findFirstSetBit(bitmap, 512);
        write(fp, bitmap, 500);
        // 如果正好新开一块，找个新块号，改位图和addr
        if (root->st_size == 2048)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            read(fp, datamap, 2048);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            write(fp, datamap, 2048);
            root->addr[4] = nblock;
        }
        if (inblock == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            read(fp, datamap, 2048);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            write(fp, datamap, 2048);

            short int *tmp = malloc(2);
            *tmp = nblock;
            t1 = nblock;
            fseek(fp, root->addr[4] * 512 + block * 2, SEEK_SET);
            write(fp, tmp, 2);
            free(tmp);
        }

        short int *tmp = malloc(2);
        fseek(fp, root->addr[4] * 512 + 2 * block, SEEK_SET);
        read(fp, tmp, 2);
        t1 = *tmp;
        free(tmp);
        fseek(fp, t1 * 512 + inblock, SEEK_SET);
        // 目录项写回去
        ndir->st_ino = node;
        write(fp, ndir, sizeof(struct directory));
        // 改inode区
        struct inode *nnode = malloc(sizeof(struct inode));
        nnode->st_ino = node;
        nnode->st_nlink = 0;
        nnode->ac_size = 0;
        nnode->st_size = 0;
        fseek(fp, 512 + 64 * node, SEEK_SET);
        write(fp, nnode, sizeof(struct inode));
        // 改root
        root->st_nlink++;
        fseek(fp, 512 * 6, SEEK_SET);
        write(fp, root, sizeof(struct inode));
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
        memcpy(ndir->name, name, strlen(name));
        ndir->expand[0] = '\0';
        // 搞个inode号
        // 同时把inode位图的那一位置1
        unsigned char bitmap[512];
        fseek(fp, 512, SEEK_SET);
        read(fp, bitmap, 500);
        int node = findFirstSetBit(bitmap, 512);
        write(fp, bitmap, 500);
        // 如果正好新开一块，找个新块号，改位图和addr
        if (root->st_size == 133120)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            read(fp, datamap, 2048);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            write(fp, datamap, 2048);
            root->addr[5] = nblock;
        }
        if (inblock == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            read(fp, datamap, 2048);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            write(fp, datamap, 2048);

            short int *tmp = malloc(2);
            *tmp = nblock;
            t1 = nblock;
            fseek(fp, root->addr[5] * 512 + block * 2, SEEK_SET);
            write(fp, tmp, 2);
            free(tmp);
        }
        short int *tmp = malloc(2);
        fseek(fp, root->addr[4] * 512 + 2 * block, SEEK_SET);
        read(fp, tmp, 2);
        t1 = *tmp;
        free(tmp);
        if (inblock % 512 == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            read(fp, datamap, 2048);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            write(fp, datamap, 2048);

            short int *tmp = malloc(2);
            *tmp = nblock;
            t2 = nblock;
            fseek(fp, t1 * 512 + (inblock / 512) * 2, SEEK_SET);
            write(fp, tmp, 2);
            free(tmp);
        }
        short int *tmp = malloc(2);
        fseek(fp, t1 * 512 + (inblock / 512) * 2, SEEK_SET);
        write(fp, tmp, 2);
        t2 = *tmp;
        free(tmp);
        fseek(fp, t2 * 512 + (inblock % 512), SEEK_SET);
        // 目录项写回去
        ndir->st_ino = node;
        write(fp, ndir, sizeof(struct directory));
        // 改inode区
        struct inode *nnode = malloc(sizeof(struct inode));
        nnode->st_ino = node;
        nnode->st_nlink = 0;
        nnode->ac_size = 0;
        nnode->st_size = 0;
        fseek(fp, 512 + 64 * node, SEEK_SET);
        write(fp, nnode, sizeof(struct inode));
        // 改root
        root->st_nlink++;
        fseek(fp, 512 * 6, SEEK_SET);
        write(fp, root, sizeof(struct inode));
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
        memcpy(ndir->name, name, strlen(name));
        ndir->expand[0] = '\0';
        // 搞个inode号
        // 同时把inode位图的那一位置1
        unsigned char bitmap[512];
        fseek(fp, 512, SEEK_SET);
        read(fp, bitmap, 500);
        int node = findFirstSetBit(bitmap, 512);
        write(fp, bitmap, 500);
        // 如果正好新开一块，找个新块号，改位图和addr
        if (root->st_size == 33687552)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            read(fp, datamap, 2048);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            write(fp, datamap, 2048);
            root->addr[6] = nblock;
        }
        if (inblock == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            read(fp, datamap, 2048);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            write(fp, datamap, 2048);

            short int *tmp = malloc(2);
            *tmp = nblock;
            t1 = nblock;
            fseek(fp, root->addr[6] * 512 + block * 2, SEEK_SET);
            write(fp, tmp, 2);
            free(tmp);
        }
        short int *tmp = malloc(2);
        fseek(fp, root->addr[6] * 512 + 2 * block, SEEK_SET);
        read(fp, tmp, 2);
        t1 = *tmp;
        free(tmp);
        if (inblock % (256 * 512) == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            read(fp, datamap, 2048);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            write(fp, datamap, 2048);

            short int *tmp = malloc(2);
            *tmp = nblock;
            t2 = nblock;
            fseek(fp, t1 * 512 + (inblock / (256 * 512)) * 2, SEEK_SET);
            write(fp, tmp, 2);
            free(tmp);
        }
        short int *tmp = malloc(2);
        fseek(fp, t1 * 512 + (inblock / (256 * 512)) * 2, SEEK_SET);
        write(fp, tmp, 2);
        t2 = *tmp;
        free(tmp);
        // fseek(fp, t2 * 512 + (inblock % (256 * 512)) * 2, SEEK_SET);
        int ninblock = inblock % (256 * 512);
        if (ninblock % 512 == 0)
        {
            unsigned char datamap[2048];
            fseek(fp, 1024, SEEK_SET);
            read(fp, datamap, 2048);
            int nblock = findAndSetFirstZeroBit(datamap, 2048);
            write(fp, datamap, 2048);

            short int *tmp = malloc(2);
            *tmp = nblock;
            t3 = nblock;
            fseek(fp, t2 * 512 + (ninblock / (512)) * 2, SEEK_SET);
            write(fp, tmp, 2);
            free(tmp);
        }
        short int *tmp = malloc(2);
        fseek(fp, t2 * 512 + (ninblock / (512)) * 2, SEEK_SET);
        t3 = *tmp;
        write(fp, tmp, 2);
        free(tmp);
        fseek(fp, t3 * 512 + (ninblock % 512), SEEK_SET);
        // 目录项写回去
        ndir->st_ino = node;
        write(fp, ndir, sizeof(struct directory));
        // 改inode区
        struct inode *nnode = malloc(sizeof(struct inode));
        nnode->st_ino = node;
        nnode->st_nlink = 0;
        nnode->ac_size = 0;
        nnode->st_size = 0;
        fseek(fp, 512 + 64 * node, SEEK_SET);
        write(fp, nnode, sizeof(struct inode));
        // 改root
        root->st_nlink++;
        fseek(fp, 512 * 6, SEEK_SET);
        write(fp, root, sizeof(struct inode));
        free(nnode);
        free(ndir);
        fclose(fp);
        return;
    }
}
static int MFS_readdir(char *name)
{
    if (strlen(name) > 8)
    {
        printf("SFS_readattr：文件名过长\n\n");
        return -ENAMETOOLONG;
    }
    for (int i = 0; i < strlen(name); i++)
    {
        if (name[i] == '/')
        {
            printf("SFS_readattr：要创建的目录不在根目录下\n\n");
            return -EPERM;
        }
    }
    char *ex = "\0";
    struct inode *root = malloc(sizeof(struct inode));
    FILE *fp = NULL;
    fp = fopen(FILEADDR, "r+");
    fseek(fp, 6 * 512, SEEK_SET);
    read(fp, root, sizeof(struct inode));
    if (find(name, ex, root->addr, root->st_size) != -1)
    {
        free(root);
        fclose(fp);
        return -EEXIST;
    }
    mkd(name, root);
    free(root);
    fclose(fp);
    printf("SFS_readattr：成功\n\n");
    return 0;
}

static struct fuse_operations SFS_opener
{
    .init = SFS_init,
    .getattr = SFS_getattr,
    .readdir = SFS_readdir,
    .mkdir = SFS_mkdir,
    .rmdir = SFS_rmdir,
    .mknod = SFS_mkknod,
    .write = SFS_write,
    .read = SFS_read,
    .unlink = SFS_unlink,
};