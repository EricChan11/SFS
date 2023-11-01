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
struct sb {
    long fs_size;  //文件系统的大小，以块为单位
    long first_blk;  //数据区的第一块块号，根目录也放在此
    long datasize;  //数据区大小，以块为单位 
    long first_inode;    //inode区起始块号
    long inode_area_size;   //inode区大小，以块为单位
    long fisrt_blk_of_inodebitmap;   //inode位图区起始块号
    long inodebitmap_size;  // inode位图区大小，以块为单位
    long first_blk_of_databitmap;   //数据块位图起始块号
    long databitmap_size;      //数据块位图大小，以块为单位
};

struct inode {//共64字节 
    short int st_mode; /* 权限，2字节 */ 
    short int st_ino; /* i-node号，2字节 */ 
    char st_nlink; /* 连接数，1字节 */ 
    uid_t st_uid; /* 拥有者的用户 ID ，4字节 */ 
    gid_t st_gid; /* 拥有者的组 ID，4字节  */ 
    off_t st_size; /*文件大小，4字节 */ 
    struct timespec st_atim; /* 16个字节time of last access */ 
    short int addr [7];    /*磁盘地址，14字节*/
};

struct stat{
    bool is_file;
    short int st_mode; /* 权限，2字节 */
    off_t st_size; /*文件大小，4字节 */  
};

struct directory{//共16字节
    char name [9];
    char expand [4];
    short int st_ino; /* i-node号，2字节 */ 
    //3字节备用

};
short int find_in_block(char*file_name,char*extensions,int off,off_t size){
    struct directory *dir = malloc(sizeof(struct directory));
    FILE *fp=NULL;
    fp = fopen(FILEADDR, "r+");
    short int i = 0;
    short int res = -1;
    while(i<size){
        fseek(fp, off, SEEK_SET);
        read(fp,dir,sizeof(struct directory));
        if(strcmp(file_names, dir->name)&&strcmp(extensions,dir->expand)){
            res=dir->st_ino;
            free(dir);
            fp.close();
            return res;
        }
        offset+=16;
        i+=16;
    }
    free(dir);
    fp.close();
    return res;                    
}
short int find_once_indirect(char*file_name,char*extensions,short int addr,off_t size){
    short int res=-1;
    short int offset = 0;
    //在一级间接里找
    int block_bit=size-2048;
    int block_num=block_bit/512;
    int last_block=block_bit%512;
    short int *ndir=NULL;
    int noff=0;
    FILE *fp=NULL;
    fp = fopen(FILEADDR, "r+");
    int i=0
    for(;i<block_num;i++){
        offset=addr*512+i*2;
        fseek(fp, offset, SEEK_SET);
        read(fp,ndir,sizeof(short int));
        noff=(*ndir)*512;
        res=find_in_block(file_name,extensions,noff,512);
        if(res!=-1){
            free(ndir);
            fp.close();
            return res;
        }
    }
    if(last_block!=0){
    offset=addr*512+i*2;
    fseek(fp, offset, SEEK_SET);
    read(fp,ndir,sizeof(short int));
    noff=(*ndir)*512;
    res=find_in_block(file_name,extensions,noff,last_block);
    }
    free(ndir);
    fp.close();
    return res;
    
}
short int find_twice_indirect(char*file_name,char*extensions,short int addr,off_t size){
    short int res=-1;
    short int offset = 0;
    offset=addr*512;
    int block_bit=size-133120;
    int block_num_2=block_bit/(512*256);
    int last_block_2=block_bit%(512*256);
    short int *ndir=NULL;
    int naddr=0;
    FILE *fp=NULL;
    fp = fopen(FILEADDR, "r+");
    int i=0
    for(;i<block_num_2;i++){
        offset=addr*512+i*2;
        fseek(fp, offset, SEEK_SET);
        read(fp,ndir,sizeof(short int));
        naddr=(*ndir);
        res=find_once_indirect(filename,extensions,naddr,133120);
        if(res!=-1){
            free(ndir);
            fp.close();
            return res;
        } 
    }
    if(lask_block_2!=0){
        offset=addr*512+i*2;
        fseek(fp, offset, SEEK_SET);
        read(fp,naddr,sizeof(short int));
        naddr=(*ndir);
        res=find_in_block(file_name,extensions,naddr,last_block_2);
    }
    free(ndir);
    fp.close();
    return res;
}
short int find(char*file_name,char*extensions,char addr[],off_t size){
    //0，1，2，3直接，4一级间接，5二级间接，6三级间接
    //目录项16字节，一个块512字节，可以放32个
    //地址项2字节，一个块可以放256个
    short int res=-1;
    short int offset = 0;
    if(size<=2048){//不大于4*512=2048的，直接访问
        int time = size/512;
        int remind = time%512;
        int index=0;
        while(time){
            offset = addr[index]*512;
            res=find_in_block(file_name,extensions,offset,512);
            if(res!=-1){
                return res;
            }
            time--;
            index++;
        }
        if(remind!=0){
            offset = addr[index]*512;
            res=find_in_block(file_name,extensions,offset,remind);
        }
        return res;      
        
    }
    else if(size<=133120){//不大于2048+256*512=133120的，直接加一级
        int time=4;
        int index=0;
        while(time){
            offset = addr[index]*512;
            res=find_in_block(file_name,extensions,offset,512);
            if(res!=-1){
                return res;
            }
            time--;
            index++;
        }
        res=find_once_indirect(file_name,extensions,addr[4],size);
        return res;
    }
    else if(size<=33687552){//不大于133120+256*256*512=33687552的，再加二级
        int time=4;
        int index=0;
        while(time){
            offset = addr[index]*512;
            res=find_in_block(file_name,extensions,offset,512);
            if(res!=-1){
                return res;
            }
            time--;
            index++;
        }
        res=find_once_indirect(file_name,extensions,addr[4],133120);
        if(res!=-1){
            return res;
        }
        //启动第二级
       res=find_twice_indirect(file_name,extensions,addr[5],size);
        return res;
        
    }
    else{//剩下的都是上了三级的
        int time=4;
        int index=0;
        while(time){
            offset = addr[index]*512;
            res=find_in_block(file_name,extensions,offset,512);
            if(res!=-1){
                return res;
            }
            time--;
            index++;
        }
        res=find_once_indirect(file_name,extensions,addr[4],133120);
        if(res!=-1){
            return res;
        }
        //启动第二级
        res=find_twice_indirect(file_name,extensions,addr[5],33687552);
        if(res!=-1){
            return res;
        }
        //第三级
        offset=addr[6]*512;
        int block_bit=size-33687552;
        int block_num_3=block_bit/(512*256*256);
        int last_block_3=block_bit%(512*256*256);
        short int *ndir=NULL;
        int naddr=0;
        FILE *fp=NULL;
        fp = fopen(FILEADDR, "r+");
        int i=0
        for(;i<block_num_3;i++){
            offset=addr[6]*512+i*2;
            fseek(fp, offset, SEEK_SET);
            read(fp,ndir,sizeof(short int));
            naddr=(*ndir);
            res=find_twice_indirect(filename,extensions,naddr,33687552);
            if(res!=-1){
                free(ndir);
                fp.close();
                return res;
            } 
        }
        if(lask_block_3!=0){
            offset=addr*512+i*2;
            fseek(fp, offset, SEEK_SET);
            read(fp,naddr,sizeof(short int));
            naddr=(*ndir);
            res=find_in_block(file_name,extensions,naddr,last_block_3);
        }
        free(ndir);
        fp.close();
        return res;
    }
}
int get_fd_to_attr(const char *path,struct inode *io){
    //0 not exist
    //1 exist and is dir
    //2 exsit and is file
    char *token = strtok((char *)path, "/");
    char *file_names[100]; // 假设最多有10层
    char *extensions[100];
    int i = 0;
    while (token != NULL) {
        file_names[i] = token;
        char *dot = strrchr(file_names[i], '.');
        if (dot != NULL) {
            extensions[i] = dot + 1;
            *dot = '\0'; // 在点之前截断文件名
        } else {
            extensions[i] = ""; // 如果没有后缀，设置为空字符串
        }
        token = strtok(NULL, "/");
        i++;
    }
    FILE *fp=NULL;
    fp = fopen(FILEADDR, "r+");//打开文件
    int offset = 6*512;//inode of root dir
    short int curnode = 0;
    struct inode *now = malloc(sizeof(struct inode));
    for (int j = 0; j < i; j++) {
        fseek(fp, offset, SEEK_SET);
        read(fp,now,sizeof(struct inode));
        if(strcmp(file_names[i], "")&&strcmp(extensions[i],"")){
            //就是当前这个目录
            memcpy(io,now,sizeof(struct inode));
            free(now);
            fp.close();
            return 1;
        }
       
        if(now->st_size==0){
            //这一级目录空的，肯定没有你要找的东西
            free(now);
            fp.close();
            return 0;
        }
        curnode=find(file_name[i],extensions[i],now->addr,now->st_size);
        if(curnode!=-1){
            //找到下一级的inode号了
            offset=6*512+curnode*64;
            continue;
        }
        else{
            //找不到
            free(now);
            fp.close();
            return 0;
        }
    }
    fseek(fp, offset, SEEK_SET);
    read(fp,now,sizeof(struct inode));
    memcpy(io,now,sizeof(struct inode));
    free(now);
    fp.close();
    return 2;
}
static int SFS_getattr(const char *path, struct stat *stbuf)
{
	//(void) fi;
	int res = 0;
	struct inode *io = malloc(sizeof(struct inode));
    int type = get_fd_to_attr(path, io);
	//非根目录
	if (type == 0) 
	{
		free(io);	
        printf("SFS_getattr：get_fd_to_attr时发生错误，函数结束返回\n\n");
        return -ENOENT;
	}
	memset(stbuf, 0, sizeof(struct stat));//将stat结构中成员的值全部置0
	else if (type == 1)
	{//从path判断这个文件是		一个目录	还是	一般文件
		printf("SFS_getattr：这个file_directory是一个目录\n\n");
		stbuf->st_mode = S_IFDIR | 0666;//设置成目录,S_IFDIR和0666（8进制的文件权限掩码），这里进行或运算
	} 
	else if (type == 2) 
	{
		printf("SFS_getattr：这个file_directory是一个文件\n\n");
		stbuf->st_mode = S_IFREG | 0666;//该文件是	一般文件
		stbuf->st_size = io->st_size;
		//stbuf->st_nlink = 1;
	} 
	else {
        printf("SFS_getattr：这个文件（目录）不存在，函数结束返回\n\n");
        res = -ENOENT;
    }//文件不存在
	
	printf("FS_getattr：getattr成功，函数结束返回\n\n");
	free(io);
	return res;
}

static struct fuse_operations SFS_opener{
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