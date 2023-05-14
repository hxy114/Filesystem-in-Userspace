#define FUSE_USE_VERSION 31
#include <stdio.h>
#include <string.h>
#include <fuse.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#define MAX_FILE_NAME 15
#define BLOCK_SIZE 1024 //一块大小1024bit
#define MAX_DATA_IN_BLOCK (BLOCK_SIZE-32)
#define INODE_MAP_BLOCK_SIZE 1
#define DATA_MAP_BLOCK_SIZE 1
#define INODE_START_BLOCK (1+INODE_MAP_BLOCK_SIZE+DATA_MAP_BLOCK_SIZE)
#define DATA_START_BLOCK  (INODE_START_BLOCK+INODE_MAP_BLOCK_SIZE*BLOCK_SIZE)
#define GET_BIT(x,bit) ((x&(1<<bit))>>bit)
//占1块
struct super_block{
    int inode_map_size_;
    int data_map_size_;
};
//占一块 刚好等于1024bit
struct  inode{
    int inode_num_;//从0开始
    char file_name_[MAX_FILE_NAME+1];
    int file_size_;//目录是文件数量，文件是字节数
    int block_size_;
    int file_type_; //type=1是目录，type=2是文件
    int gid_;
    int uid_;
    int block_num_[22];
};
struct data_block{
    int use_byte_;
    char data_[BLOCK_SIZE/8-4];
};
char *disk_path="diskimg";
struct super_block super_block;
int fd;
//根据文件的块号，从磁盘中读取数据
int read_cpy_data_block(long blk_no,struct data_block *data_blk)
{

    //int fd=open(disk_path,O_RDWR,0644);
    if (fd == -1)
    {
        printf("错误：read_cpy_data_block：打开文件失败\n\n");
        return -1;
    }
    //文件打开成功以后，就用blk_no * FS_BLOCK_SIZE作为偏移量
    lseek(fd, blk_no*BLOCK_SIZE, SEEK_SET);
    read(fd,data_blk,sizeof(struct data_block));
    return 0;
}
//根据文件的块号，数据写入磁盘
int write_cpy_data_block(long blk_no,struct data_block *data_blk)
{

    if (fd == -1)
    {
        printf("错误：read_cpy_data_block：打开文件失败\n\n");
        return -1;
    }
    //文件打开成功以后，就用blk_no * FS_BLOCK_SIZE作为偏移量
    lseek(fd, blk_no*BLOCK_SIZE, SEEK_SET);
    write(fd,data_blk,sizeof(struct data_block));
    return 0;
}
//拷贝inode
void read_cpy_inode(struct inode *dest,struct inode *src)
{
    dest->inode_num_=src->inode_num_;
    strcpy(dest->file_name_, src->file_name_);
    dest->file_size_ = src->file_size_;
    dest->block_size_ = src->block_size_;
    dest->file_type_ = src->file_type_;
    dest->gid_ = src->gid_;
    dest->uid_ = src->uid_;
    for(int i=0;i<dest->block_size_;i++){
        dest->block_num_[i]=src->block_num_[i];
    }
}
int find_next_inode(struct inode* root_inode,int16_t file_type,char *file_name){
    struct data_block *dataBlock= malloc(sizeof(struct data_block));
    struct data_block *dataBlock1= malloc(sizeof(struct data_block));
    printf("find_next_index :root_inode:inode_num:%d,block_size_:%d\n",root_inode->inode_num_,root_inode->block_size_);
    for(int i=0;i<root_inode->block_size_;i++){
        printf("find_next_index :root_inode: root_inode->block_num_[i]:%d\n",root_inode->block_num_[i]);
        read_cpy_data_block(root_inode->block_num_[i],dataBlock);
        int offset=0;
        printf("use_byte:%d\n",dataBlock->use_byte_);
        while(offset<dataBlock->use_byte_){
            int inode_num= *(int *) (dataBlock->data_ + offset);
            if(inode_num>=0){
                read_cpy_data_block(inode_num+INODE_START_BLOCK,dataBlock1);
                struct inode * inode=(struct inode*)dataBlock1;
                printf("inode_num %d,inode_name:%s\n",inode->inode_num_,inode->file_name_);
                if(strcmp(inode->file_name_,file_name)==0&&(inode->file_type_==file_type||file_type==3)){
                    read_cpy_inode(root_inode,inode);
                    free(dataBlock);
                    free(dataBlock1);
                    return 1;
                }
            }

            offset+= sizeof(int);

        }
    }
    free(dataBlock);
    free(dataBlock1);
    return -1;

}
//读取这个路径的inode
int get_inode(const char *path,struct inode* inode){
    //先要读取超级块，获取磁盘根目录块的位置
    printf("get_inode：函数开始\n\n");
    printf("get_inode：要寻找的file_directory的路径是%s\n\n",path);

    char *tmp_path,*m=NULL;//tmp_path用来临时记录路径，然后m指针是用来定位文件名
    tmp_path=strdup(path);

    //如果路径为空，则出错返回1
    if (!tmp_path){
        printf("错误：get_inode：路径为空，函数结束返回\n\n");
        return -1;
    }

    struct data_block *dataBlock= malloc(sizeof(struct data_block));
    read_cpy_data_block(INODE_START_BLOCK,dataBlock);
    struct inode * root_inode=(struct inode*)dataBlock;
    //如果路径为根目录路径(注意这里的根目录是指/home/linyueq/homework/diskimg/???，/???之前的路径是被忽略的)
    if (strcmp(tmp_path, "/") == 0){
        read_cpy_inode(inode,root_inode);
        printf("inode_num=%d inode_block_size=%d,inode_fist_block_num=%d\n",inode->inode_num_,inode->block_size_,inode->block_num_[0]);
        free(dataBlock);
        printf("get_inode：这是一个根目录，函数结束返回\n\n");
        return 0;
    }
    tmp_path++;
    m=strchr(tmp_path,'/');
    while(m!=NULL){
        printf("get_inode：有二级目录\n");
        (*m)='\0';
        m++;
        if(find_next_inode(root_inode,1,tmp_path)==1){
            tmp_path=m;
            m=strchr(tmp_path,'/');
        }else{
            free(dataBlock);
            return -1;
        }
    }
    printf("get_inode:tmp_path:%s\n",tmp_path);
    if(find_next_inode(root_inode,3,tmp_path)==1){
        read_cpy_inode(inode,root_inode);
        printf(" inode_addr=%d,inode_num=%d inode_block_size=%d,inode_fist_block_num=%d\n",inode,inode->inode_num_,inode->block_size_,inode->block_num_[0]);
        free(dataBlock);
        return 0;
    }
    printf("get inode:没有找到相关文件\n");
    return -1;


}
//通过bitmap获取空的inode块号
int find_empty_index(){
    struct data_block *dataBlock= malloc(sizeof(struct data_block));
    read_cpy_data_block(1,dataBlock);
    unsigned  char *bitmap=(unsigned char *)dataBlock;
    for(int i=0;i<1024;i++){
        if( GET_BIT(bitmap[i/8],(7-i%8))==0){
            printf("find_empty_index:bitmap:%d\n",bitmap[i/8]);
            unsigned char x=1;
            bitmap[i/8]+=x<<(7-i%8);
            write_cpy_data_block(1,dataBlock);
            free(dataBlock);
            printf("find_empty_index :%d\n",i);
            return i;
        }
    }
    free(dataBlock);
    return -1;


}
//通过bitmap获取新的数据块的块号
int find_empty_data(){
    struct data_block *dataBlock= malloc(sizeof(struct data_block));
    read_cpy_data_block(1+INODE_MAP_BLOCK_SIZE,dataBlock);
    unsigned  char *bitmap=(unsigned char *)dataBlock;
    for(int i=0;i<1024;i++){
        if( GET_BIT(bitmap[i/8],(7-i%8))==0){
            printf("find_empty_data:bitmap:%d\n",bitmap[i/8]);
            unsigned char x=1;
            bitmap[i/8]+=x<<(7-i%8);
            write_cpy_data_block(1+INODE_MAP_BLOCK_SIZE,dataBlock);
            free(dataBlock);
            printf("find_empty_data :%d\n",i);
            return i;
        }
    }
    free(dataBlock);
    return -1;
}
//清除数据的bitmap
int clear_data_bit(int bit_num){
    struct data_block *dataBlock= malloc(sizeof(struct data_block));
    read_cpy_data_block(1+INODE_MAP_BLOCK_SIZE,dataBlock);
    unsigned  char *bitmap=(unsigned char *)dataBlock;
    unsigned char x=1;
    bitmap[bit_num/8]-=x<<(7-bit_num%8);
    write_cpy_data_block(1+INODE_MAP_BLOCK_SIZE,dataBlock);
    free(dataBlock);
}
//清除inode的bitmap
int clear_inode_bit(int bit_num){
    struct data_block *dataBlock= malloc(sizeof(struct data_block));
    read_cpy_data_block(1,dataBlock);
    unsigned  char *bitmap=(unsigned char *)dataBlock;
    unsigned char x=1;
    bitmap[bit_num/8]-=x<<(7-bit_num%8);
    write_cpy_data_block(1,dataBlock);
    free(dataBlock);
}
//初始化inode块
int init_inode_block(struct inode *inode,int block_no){
    inode->file_type_=1;
    inode->inode_num_=0;
    inode->gid_=getgid();
    inode->uid_=getuid();
    inode->file_size_=0;
    inode->block_size_=1;
    inode->block_num_[0]=DATA_START_BLOCK;
}
//初始数据块
int init_data_block(int block_no){
    struct data_block *dataBlock= malloc(sizeof(struct data_block));
    dataBlock->use_byte_=0;
    write_cpy_data_block(block_no,dataBlock);
}
//把inode块信息写会磁盘
int update_inode(struct inode *inode){
    struct data_block*dataBlock= (struct data_block *) inode;
    write_cpy_data_block(inode->inode_num_+INODE_START_BLOCK,dataBlock);
}
//把目录插入到父节点的inode中
int insert_directory(struct inode *inode, const char *path){
    inode->file_size_++;
    struct data_block *dataBlock= malloc(sizeof(struct data_block));
    read_cpy_data_block(inode->block_num_[inode->block_size_-1],dataBlock);
    if(dataBlock->use_byte_>=BLOCK_SIZE/8-4){//如果父节点的一个数据块满了，申请另一块数据块。
        printf("insert_directory 块溢出\n");
        inode->block_size_+=1;
        int block_no=find_empty_data()+DATA_START_BLOCK;
        inode->block_num_[inode->block_size_-1]=block_no;
        init_data_block(block_no);
        read_cpy_data_block(inode->block_num_[inode->block_size_-1],dataBlock);
    }
    //获取新文件的inode,并且获取新文件的一个数据块，并且在父节点的inode设置相关信息。
    update_inode(inode);
    int inode_num=find_empty_index(),block_no=inode_num+INODE_START_BLOCK;
    *(int *)(dataBlock->data_+dataBlock->use_byte_)=inode_num;
    dataBlock->use_byte_+=4;
    write_cpy_data_block(inode->block_num_[inode->block_size_-1],dataBlock);

    read_cpy_data_block(block_no,dataBlock);
    struct inode * inode1=(struct inode*)dataBlock;
    int block_no1=find_empty_data()+DATA_START_BLOCK;
    init_data_block(block_no1);
    inode1->file_type_=1;
    strcpy(inode1->file_name_,path);
    printf("insert_directory ---path:%s",path);
    printf("insert_directory ---inode1->file_name_:%s\n\n",inode1->file_name_);
    inode1->inode_num_=inode_num;
    inode1->gid_=getgid();
    inode1->uid_=getuid();
    inode1->file_size_=0;
    inode1->block_size_=1;
    inode1->block_num_[0]=block_no1;
    write_cpy_data_block(block_no,dataBlock);

    read_cpy_data_block(4,dataBlock);
    struct inode *inode2=(struct inode*)dataBlock;
    printf("check file_name %s\n",inode2->file_name_);
}

int insert_file(struct inode *inode, const char *path) {
    inode->file_size_++;
    struct data_block *dataBlock = malloc(sizeof(struct data_block));
    read_cpy_data_block(inode->block_num_[inode->block_size_ - 1], dataBlock);
    if (dataBlock->use_byte_ >= BLOCK_SIZE / 8 - 4) {
        printf("insert_file 块溢出\n");
        inode->block_size_ += 1;
        int block_no = find_empty_data() + DATA_START_BLOCK;
        inode->block_num_[inode->block_size_ - 1] = block_no;
        init_data_block(block_no);
        read_cpy_data_block(block_no, dataBlock);
    }
    update_inode(inode);
    int inode_num = find_empty_index(), block_no = inode_num + INODE_START_BLOCK;
    *(int *) (dataBlock->data_ + dataBlock->use_byte_) = inode_num;
    dataBlock->use_byte_ += 4;
    write_cpy_data_block(inode->block_num_[inode->block_size_ - 1], dataBlock);

    read_cpy_data_block(block_no, dataBlock);
    struct inode *inode1 = (struct inode *) dataBlock;
    int block_no1 = find_empty_data() + DATA_START_BLOCK;
    init_data_block(block_no1);
    inode1->file_type_ = 2;
    strcpy(inode1->file_name_, path);
    printf("insert_file ---path:%s", path);
    printf("insert_file ---inode1->file_name_:%s\n\n", inode1->file_name_);
    inode1->inode_num_ = inode_num;
    inode1->gid_ = getgid();
    inode1->uid_ = getuid();
    inode1->file_size_ = 0;
    inode1->block_size_ = 1;
    inode1->block_num_[0] = block_no1;
    write_cpy_data_block(block_no, dataBlock);

    read_cpy_data_block(4, dataBlock);
    struct inode *inode2 = (struct inode *) dataBlock;
    printf("check file_name %s\n", inode2->file_name_);
}
int clear_root_record(struct inode *root_inode,struct inode *inode){

    struct data_block *dataBlock= malloc(sizeof(struct data_block));
    struct data_block *dataBlock1= malloc(sizeof(struct data_block));
    for(int i=0;i<root_inode->block_size_;i++){
        printf("read i:%d\n",i);
        read_cpy_data_block(root_inode->block_num_[i],dataBlock);
        int offset=0;
        printf("clear_root_record:use_byte:%d\n",dataBlock->use_byte_);
        while(offset<dataBlock->use_byte_){
            printf("clear_root_record:offset:%d\n",offset);
            int inode_num= *(int *) (dataBlock->data_ + offset);
            if(inode_num>=0){
                read_cpy_data_block(inode_num+INODE_START_BLOCK,dataBlock1);
                struct inode * inode1=(struct inode*)dataBlock1;
                if(strcmp(inode1->file_name_,inode->file_name_)==0&&inode1->file_type_==inode->file_type_){
                    root_inode->file_size_--;
                    *(int *) (dataBlock->data_ + offset)=-1;
                    write_cpy_data_block(root_inode->inode_num_+INODE_START_BLOCK, (struct data_block *) root_inode);
                    write_cpy_data_block(root_inode->block_num_[i],dataBlock);
                    free(dataBlock);
                    free(dataBlock1);
                    return 1;

                }

            }
            offset+= sizeof(int);


        }
    }
    free(dataBlock);
    free(dataBlock1);
}

int delete_directory(struct inode *root_inode,struct inode *inode){
    clear_root_record(root_inode,inode);


    for(int i=0;i<inode->block_size_;i++){
        clear_data_bit(inode->block_num_[i]-DATA_START_BLOCK);
    }
    clear_inode_bit(inode->inode_num_-INODE_START_BLOCK);


}
static void* fuse_init(struct fuse_conn_info *conn){

}
static int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t off, struct fuse_file_info *fi){

    printf("fuse_readdir start path:%s\n",path);
    struct inode *inode= malloc(sizeof(struct inode));
    if(get_inode(path,inode)==-1){//找到路径的inode
        free(inode);
        return -ENOENT;
    }
    if(inode->file_type_!=1){//判断这个路径是不是一个目录
        free(inode);
        return -ENOENT;
    }
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    struct data_block *dataBlock= malloc(sizeof(struct data_block));
    struct data_block *dataBlock1= malloc(sizeof(struct data_block));
    for(int i=0;i<inode->block_size_;i++){//读取目录下的文件名，并且保存在buf中
        printf("read i:%d\n",i);
        read_cpy_data_block(inode->block_num_[i],dataBlock);
        int offset=0;
        while(offset<dataBlock->use_byte_){
            int inode_num= *(int *) (dataBlock->data_ + offset);
            if(inode_num>=0){
                read_cpy_data_block(inode_num+INODE_START_BLOCK,dataBlock1);
                struct inode * inode1=(struct inode*)dataBlock1;
                filler(buf, inode1->file_name_, NULL, 0, 0);
            }

            offset+= sizeof(int);

        }
    }
    free(dataBlock);
    free(dataBlock1);
    return 0;
}

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
//获取文件的信息
static int fuse_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    printf("fuse_readdir start path:%s\n",path);
    int res = 0;
    struct inode *inode= malloc(sizeof(struct inode));


    if (get_inode(path, inode) == -1)//获取路径的inode
    {
        free(inode);
        printf("fuse_getattr：get_inode时发生错误，函数结束返回\n\n");
        return -ENOENT;
    }

    memset(stbuf, 0, sizeof(struct stat));//将stat结构中成员的值全部置0
    if (inode->file_type_==1)
    {//从path判断这个文件是		一个目录	还是	一般文件
        printf("fuse_getattr：这个file_directory是一个目录\n\n");
        stbuf->st_mode = S_IFDIR | 0666;//设置成目录,S_IFDIR和0666（8进制的文件权限掩码），这里进行或运算
        stbuf->st_nlink = 2;//st_nlink是连到该文件的硬连接数目,即一个文件的一个或多个文件名。说白点，所谓链接无非是把文件名和计算机文件系统使用的节点号链接起来。因此我们可以用多个文件名与同一个文件进行链接，这些文件名可以在同一目录或不同目录。
        stbuf->st_gid=inode->gid_;
        stbuf->st_uid=inode->uid_;
        stbuf->st_mtime=32435464;
    }
    else if (inode->file_type_==2)
    {
        printf("MFS_getattr：这个file_directory是一个文件\n\n");
        stbuf->st_mode = S_IFREG | 0666;//该文件是	一般文件
        stbuf->st_size = inode->file_size_;
        stbuf->st_nlink = 1;
        stbuf->st_gid=inode->gid_;
        stbuf->st_uid=inode->uid_;
    }
    else {printf("fuse_getattr：这个文件（目录）不存在，函数结束返回\n\n");;res = -ENOENT;}//文件不存在

    free(inode);
    return res;
}
static int fuse_utimens(const char *path, struct utimbuf *tv){
    printf("UTIMENS CALLED\n");
    return 0;
}
static int fuse_mkdir (const char *path, mode_t mode){
    printf("fuse_mkdir start path:%s\n\n",path);
    char *tmp_path=strdup(path),*n=tmp_path,*m=NULL;
    struct  inode *inode=malloc(sizeof(struct inode));
    //如果路径为空，则出错返回1
    if (!tmp_path){
        printf("错误：fuse_mkdir：路径为空，函数结束返回\n\n");
        free(inode);
        return -1;
    }
    //通过路径找到父目录
    n++;
    m= strchr(n,'/');
    while(m!=NULL){
        m++;
        n=m;
        m= strchr(n,'/');
    }
    n--;
    if(n==tmp_path){
        tmp_path="/";
    }else{
        (*n)='\0';

    }
    n++;
    printf("mkdir 父目录是%s\n",tmp_path);
    if(get_inode(tmp_path,inode)==-1){//找到父目录的inode
        printf("错误：fuse_mkdir：父目录不存在,函数结束返回\n\n");
        free(inode);
        return -ENOENT;
    }
    if(inode->file_type_!=1){
        printf("错误：fuse_mkdir：父目录是文件,函数结束返回\n\n");
        free(inode);
        return -ENOENT;
    }
    if(find_next_inode(inode,3,n)==1){//查找是否存在要创建的目录
        printf("错误：fuse_mkdir：文件已经存在,函数结束返回\n\n");
        free(inode);
        return -EEXIST;
    }

    printf("父亲目录inode:inode_num=%d\n,insert file_name:%s\n",inode->inode_num_,n);
    insert_directory(inode,n);//创建目录
    printf("create_file_dir：创建文件成功，函数结束返回\n\n");
    free(inode);
    return 0;

}
//创建文件
static int fuse_mknod (const char *path, mode_t mode, dev_t dev){
    printf("fuse_mknod start path:%s\n\n",path);
    char *tmp_path=strdup(path),*n=tmp_path,*m=NULL;
    struct  inode *inode=malloc(sizeof(struct inode));
    //如果路径为空，则出错返回1
    if (!tmp_path){
        printf("错误：fuse_mkdir：路径为空，函数结束返回\n\n");
        free(inode);
        return -1;
    }
    n++;
    m= strchr(n,'/');
    while(m!=NULL){
        m++;
        n=m;
        m= strchr(n,'/');
    }
    n--;
    if(n==tmp_path){
        tmp_path="/";
    }else{
        (*n)='\0';

    }
    n++;
    printf("fuse_mknod 父目录是%s\n",tmp_path);
    if(get_inode(tmp_path,inode)==-1){
        printf("错误：fuse_mknod：父目录不存在,函数结束返回\n\n");
        free(inode);
        return -ENOENT;
    }
    if(inode->file_type_!=1){
        printf("错误：fuse_mkdir：父目录不存在,函数结束返回\n\n");
        free(inode);
        return -ENOENT;
    }

    if(find_next_inode(inode,3,n)==1){
        printf("错误：fuse_mknod：文件已经存在,函数结束返回\n\n");
        free(inode);
        return -EEXIST;
    }

    printf("父亲目录inode:inode_num=%d\n,insert file_name:%s\n",inode->inode_num_,n);
    insert_file(inode,n);
    printf("create_file_dir：创建文件成功，函数结束返回\n\n");
    free(inode);
    return 0;

}
//打开文件时的操作
static int fuse_open(const char *path, struct fuse_file_info *fi)
{
    return 0;
}
//进入目录
static int fuse_access(const char *path, int flag)
{

    return 0;
}

static int fuse_rmdir (const char *path){
    printf("fuse_rmdir start path:%s\n\n",path);
    char *tmp_path=strdup(path),*n=tmp_path,*m=NULL;
    struct  inode *inode=malloc(sizeof(struct inode));
    //如果路径为空，则出错返回1
    if (!tmp_path){
        printf("错误：fuse_mkdir：路径为空，函数结束返回\n\n");
        return -1;
    }
    n++;
    m= strchr(n,'/');
    while(m!=NULL){
        m++;
        n=m;
        m= strchr(n,'/');
    }
    n--;
    if(n==tmp_path){
        tmp_path="/";
    }else{
        (*n)='\0';

    }
    n++;
    printf("fuse_rmdir 父目录是%s\n",tmp_path);
    if(get_inode(tmp_path,inode)==-1){
        printf("错误：fuse_mknod：父目录不存在,函数结束返回\n\n");
        free(inode);
        return -ENOENT;
    }

    if(inode->file_type_!=1){
        printf("错误：fuse_rmdir：父目录不存在,函数结束返回\n\n");
        free(inode);
        return -ENOENT;
    }
    struct inode *root_inode= malloc(sizeof(struct inode));
    read_cpy_inode(root_inode,inode);
    if(find_next_inode(inode,1,n)!=1){
        printf("错误：fuse_rmdir：文件不存在,函数结束返回\n\n");
        free(inode);
        free(root_inode);
        return -ENOENT;
    }
    if(inode->file_type_!=1){
        printf("错误：fuse_rmdir：不是目录文件,函数结束返回\n\n");
        return -ENOTDIR;
    }
    if(inode->file_size_!=0){
        printf("错误：fuse_rmdir：不是空目录文件,函数结束返回\n\n");
        return -ENOTEMPTY;
    }

    printf("父亲目录inode:inode_num=%d\n,insert file_name:%s\n",root_inode->inode_num_,n);
    delete_directory(root_inode,inode);
    printf("fuse_rmdir：删除文件成功，函数结束返回\n\n");
    free(inode);
    free(root_inode);
    return 0;

}
//删除文件
static int fuse_unlink (const char *path){
    printf("fuse_unlink start path:%s\n\n",path);
    char *tmp_path=strdup(path),*n=tmp_path,*m=NULL;
    struct  inode *inode=malloc(sizeof(struct inode));
    //如果路径为空，则出错返回1
    if (!tmp_path){
        printf("错误：fuse_unlink：路径为空，函数结束返回\n\n");
        return -1;
    }
    n++;
    m= strchr(n,'/');
    while(m!=NULL){
        m++;
        n=m;
        m= strchr(n,'/');
    }
    n--;
    if(n==tmp_path){
        tmp_path="/";
    }else{
        (*n)='\0';

    }
    n++;
    printf("fuse_unlink 父目录是%s\n",tmp_path);
    if(get_inode(tmp_path,inode)==-1){
        printf("错误：fuse_unlink：父目录不存在,函数结束返回\n\n");
        free(inode);
        return -ENOENT;
    }

    if(inode->file_type_!=1){
        printf("错误：fuse_unlink：父目录不存在,函数结束返回\n\n");
        free(inode);
        return -ENOENT;
    }
    struct inode *root_inode= malloc(sizeof(struct inode));
    read_cpy_inode(root_inode,inode);
    if(find_next_inode(inode,2,n)!=1){
        printf("错误：fuse_unlink：文件不存在,函数结束返回\n\n");
        free(inode);
        free(root_inode);
        return -ENOENT;
    }
    if(inode->file_type_!=2){
        printf("错误：fuse_unlink：不是文本文件,函数结束返回\n\n");
        return -EISDIR;
    }


    printf("父亲目录inode:inode_num=%d\n,insert file_name:%s\n",root_inode->inode_num_,n);
    delete_directory(root_inode,inode);
    printf("fuse_unlink：删除文件成功，函数结束返回\n\n");
    free(inode);
    free(root_inode);
    return 0;


}
//读取文件时的操作

static int fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    printf("fuse_read start path:%s\n",path);
    printf("fuse_read :size:%zu\n",size);
    printf("fuse_read: offset:%ld\n",offset);
    struct inode *inode= malloc(sizeof(struct inode));
    if(get_inode(path,inode)==-1){
        free(inode);
        return -ENOENT;
    }
    if(inode->file_type_!=2){
        free(inode);
        return -ENOENT;
    }
    //查找文件数据块,读取并读入buf中
    //要保证 读取的位置 和 加上size后的位置 在文件范围内
    if (offset < inode->file_size_)
    {
        if (offset + size > inode->file_size_) size = inode->file_size_ - offset;
    }
    else size = 0;
    int blk_num = offset / MAX_DATA_IN_BLOCK;//到达offset处要跳过的块的数目
    int real_offset = offset % MAX_DATA_IN_BLOCK;//offset所在块的偏移量

    struct data_block *dataBlock= malloc(sizeof(struct data_block));
    read_cpy_data_block(inode->block_num_[blk_num],dataBlock);

    //文件内容可能跨多个block块，所以要用一个变量temp记录size的值，stb_size是offset所在块中需要读取的数据量
    int temp = size, stb_size = 0;
    char *pt = dataBlock->data_;//先读出offset所在块的数据
    pt += real_offset;//将数据指针移动到offset所指的位置

    //这里判断在offset这个文件块中要读取的byte数目
    if(MAX_DATA_IN_BLOCK - real_offset < size) stb_size = MAX_DATA_IN_BLOCK - real_offset;
    else stb_size = size;

    strncpy(buf, pt, stb_size);
    temp -= stb_size;

    //把剩余未读完的内容读出来
    while (temp > 0)
    {
        blk_num++;
        read_cpy_data_block(inode->block_num_[blk_num],dataBlock);
        if (temp > MAX_DATA_IN_BLOCK){ //如果剩余的数据量仍大于一个块的数据量
            memcpy(buf + size - temp, dataBlock->data_, MAX_DATA_IN_BLOCK);//从buf上一次结束的位置开始继续记录数据
            temp -= MAX_DATA_IN_BLOCK;
        }
        else //剩余的数据量少于一个块的量，说明size大小的数据读完了
        {
            memcpy(buf + size - temp, dataBlock->data_, temp);
            break;
        }
    }
    printf("fuse_read：文件读取成功，函数结束返回\n\n");
    free(inode);
    free(dataBlock);
    return size;


}
//修改文件,将buf里大小为size的内容，写入path指定的起始块后的第offset
//步骤：① 找到path所指对象的file_directory；② 根据nStartBlock和offset将内容写入相应位置；
static int fuse_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    printf("fuse_write start path:%s\n",path);
    printf("fuse_write buffer:%s\n",path);
    printf("fuse_write sizd:%d\n",size);
    struct inode ionde_;
    //struct inode *inode= malloc(sizeof(struct inode));
    struct inode *inode= &ionde_;
    if(get_inode(path,inode)==-1){
        free(inode);
        return -ENOENT;
    }

    if(inode->file_type_!=2){
        free(inode);
        return -ENOENT;
    }

    //然后检查要写入数据的位置是否越界
    if (offset > inode->file_size_){
        free(inode);
        printf("fuse_write：offset越界，函数结束返回\n\n");
        return -EFBIG;
    }


    int res, num, p_offset = offset;//p_offset用来记录修改前最后一个文件块的位置
    struct data_block *dataBlock = malloc(sizeof(struct data_block));

    int blk_num = offset / MAX_DATA_IN_BLOCK;//到达offset处要跳过的块的数目
    offset=offset%MAX_DATA_IN_BLOCK;
    //找到offset指定的块位置和块内的偏移量位置，注意，offset有可能很大，大于一个块的数据容量
    //而通过find_off_blk可以找到相应的文件的块位置start_blk和块内偏移位置（用offset记录）
    read_cpy_data_block(inode->block_num_[blk_num],dataBlock);

    //创建一个指针管理数据
    char* pt = dataBlock->data_;
    //找到offset所在块中offset位置
    pt += offset;
    int towrite = 0;
    int writen = 0;

    //磁盘块剩余空间小于文件大小，则写满该磁盘块剩余的数据空间，否则，说明磁盘足够大，可以写入整个文件
    if(MAX_DATA_IN_BLOCK - offset < size) towrite = MAX_DATA_IN_BLOCK - offset;
    else towrite = size;
    strncpy(pt, buf, towrite);	//写入长度为towrite的内容
    buf += towrite;	//移到字符串待写处
    dataBlock->use_byte_ += towrite;	//该数据块的size增加已写数据量towrite
    writen += towrite;	//buf中已写的数据量
    size -= towrite;	//buf中待写的数据量
    write_cpy_data_block(inode->block_num_[blk_num],dataBlock);
    //如果size>0，说明数据还没被写完，要构造空闲块作为待写入文件的新块


    while (1){
        blk_num++;
        if(blk_num>=inode->block_size_){
            int block_no=find_empty_data()+DATA_START_BLOCK;
            init_data_block(block_no);
            inode->block_size_++;
            inode->block_num_[blk_num]=block_no;
        }
        read_cpy_data_block(inode->block_num_[blk_num],dataBlock);
        //在新块写入数据，如果需要写入的剩余数据size已经不足一块的容量，那么towrite为size
        if(MAX_DATA_IN_BLOCK < size ){
            towrite = MAX_DATA_IN_BLOCK;
        }else{
            towrite = size;
        }

        dataBlock->use_byte_ = towrite;
        strncpy(dataBlock->data_, buf, towrite);
        buf += towrite;//buf指针移动
        size -= towrite;//待写入数据量减少
        writen += towrite;//已写入数据量增加
        printf("fuse_write writen:%d",writen);

        write_cpy_data_block(inode->block_num_[blk_num],dataBlock);
        //更新块为扩容后的块
        if (size == 0) break;	//已写完
    }

    size = writen;

    //修改被写文件file_directory的文件大小信息为:写入起始位置+写入内容大小
    inode->file_size_ = p_offset + size;
    write_cpy_data_block(inode->inode_num_+INODE_START_BLOCK, (struct data_block *) inode);

    printf("fuse_write：文件写入成功，函数结束返回\n\n");
    free(dataBlock);
    printf("free dataBlock ok\n");
    printf("inode_num:%d,inode_file_size:%d,inode_block_size:%d\n",inode->inode_num_,inode->file_size_,inode->block_size_);
    //free(inode);
    printf("free inode ok\n");

    return size;

}
static struct fuse_operations oufs_ops = {
        .init       = (void *(*)(struct fuse_conn_info *, struct fuse_config *)) fuse_init,//初始化
        .readdir    =   (int (*)(const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *,
                                 enum fuse_readdir_flags)) fuse_readdir,//读取目录
        .getattr    =   (int (*)(const char *, struct stat *, struct fuse_file_info *)) fuse_getattr,//获取文件属性
        .utimens    =    (int (*)(const char *, const struct timespec *, struct fuse_file_info *)) fuse_utimens,//修改访问时间
        .mkdir      = fuse_mkdir,//新建目录
        .mknod      = fuse_mknod,//新建文件
        .rmdir      = fuse_rmdir,//删除目录
        .unlink     = fuse_unlink,//删除文件
        .open       =fuse_open,//打开文件
        .read		= fuse_read,//读取文件内容
        .write      = fuse_write,//修改文件内容
        .access     =fuse_access,//访问路径
};

int main(int argc, char* argv[])
{
    //初始化，如果没有磁盘镜像文件那么就创建磁盘镜像并且初始化，如果有磁盘镜像文件，就把超级块读取出来。
    if(access(disk_path, F_OK) == -1){

        fd = open(disk_path, O_CREAT | O_RDWR | O_TRUNC,0644);
        if(fd < 0){
            perror("Data Disk Creation Error!\n");
            return 0;
        }

        super_block.data_map_size_=BLOCK_SIZE*DATA_MAP_BLOCK_SIZE;
        super_block.inode_map_size_=BLOCK_SIZE*INODE_MAP_BLOCK_SIZE;
        write(fd,&super_block,sizeof(struct super_block));
        lseek(fd,BLOCK_SIZE*1,SEEK_SET);
        uint8_t a[BLOCK_SIZE/8*INODE_MAP_BLOCK_SIZE];
        memset(a,0,sizeof(a));
        a[0]=128;
        write(fd,a,sizeof(a));
        lseek(fd,BLOCK_SIZE*(1+INODE_MAP_BLOCK_SIZE),SEEK_SET);
        write(fd,a,sizeof(a));

        lseek(fd,BLOCK_SIZE*(1+DATA_MAP_BLOCK_SIZE+INODE_MAP_BLOCK_SIZE),SEEK_SET);
        struct inode * inode= malloc(sizeof(struct inode));
        inode->file_type_=1;
        inode->inode_num_=0;
        inode->gid_=getgid();
        inode->uid_=getuid();
        inode->file_size_=0;
        inode->block_size_=1;
        inode->block_num_[0]=DATA_START_BLOCK;
        strcpy(inode->file_name_,"");
        write(fd,inode, sizeof(struct inode));
        lseek(fd,BLOCK_SIZE*(1+DATA_MAP_BLOCK_SIZE+INODE_MAP_BLOCK_SIZE*(1+BLOCK_SIZE)),SEEK_SET);
        struct data_block dataBlock;
        dataBlock.use_byte_=0;
        write(fd,&dataBlock, sizeof(struct data_block));
        free(inode);

    }
    else{
        fd = open(disk_path, O_RDWR, 0644);
        char *data=malloc(sizeof (char)*BLOCK_SIZE);
        read(fd,data,BLOCK_SIZE);
        struct super_block *superBlock= (struct super_block *) data;
        super_block=*superBlock;
        printf("super_block inode_map_size=%d data_map_size=%d\n\n",super_block.inode_map_size_,super_block.data_map_size_);
        free(data);

    }
    return fuse_main(argc, argv, &oufs_ops, NULL);
}