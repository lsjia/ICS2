/*
Filesystem Lab disigned and implemented by Zhou Kaijun,RUC
*/
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fuse.h>
#include <errno.h>
#include "disk.h"

#define DIRMODE S_IFDIR | 0755
#define REGMODE S_IFREG | 0644

#define SUPERBLOCK 0
#define IBITMAP 1
#define DBITMAP1 2
#define DBITMAP2 3

#define INODE_START 4
#define DATA_START 1028

#define P_NUMBER 16
#define DIRECT_NUM 12
#define INDIRECT_NUM 2

#define FILENAME_SIZE 28
#define MAX_FILE_NUM 32768

#define INODE_NUM_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)
#define MAX_DATA_NUM (BLOCK_NUM - DATA_START)

#define MAX_DIR_NUM (((BLOCK_SIZE / DIR_SIZE) - 1))
#define MAX_PTR_NUM ((BLOCK_SIZE / sizeof(int)) - 1)

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

typedef struct
{
    mode_t mode;         // 文件或目录
    nlink_t nlink;       // 文件的链接数
    uid_t uid;           // 文件所有者
    gid_t gid;           // 文件所有者的组
    off_t size;          // 文件字节数
    time_t atime;        // 被访问时间
    time_t mtime;        // 被修改时间
    time_t ctime;        // 状态改变时间
    int block_num;       // 一共有几个块
    int block[P_NUMBER]; // 包括直接指针和间接指针指向的block的块号
} inode;

typedef struct
{
    char file_name[FILENAME_SIZE]; // 文件名
    int inode_num;                 // inode编号
} directory;

#define INODE_SIZE (sizeof(struct inode))
#define DIR_SIZE (sizeof(struct directory))

char *get_upper_path(char *path) // 获取上一级目录的路径
{
    char *f_path = (char *)malloc(sizeof(char) * len);
    int len = strlen(path);
    int idx = len - 1;
    while (path[idx] != '/')
        idx--;
    for (int i = 0; i < idx; i++)
        f_path[i] = path[i];
    f_path[idx] = '\0';
    return f_path;
}

struct inode initialize_inode(mode_t f_mode) // 初始化inode
{
    struct inode inode_content;
    inode_content.mode = f_mode;
    inode_content.uid = getuid();
    inode_content.gid = getgid();
    inode_content.nlink = 1;
    inode_content.atime = time(NULL);
    inode_content.mtime = time(NULL);
    inode_content.ctime = time(NULL);

    inode_content.size = 0;
    if (f_mode == (REGMODE)) // 文件模式
        inode_content.block_num = 0;
    else // 目录模式
        inode_content.block_num = 1;

    return inode_content;
}

// 读directory的datablock
void read_data_in_dir(int inum, void *buffer, fuse_fill_dir_t filler)
{
    struct directory *dir_ptr;
    get_dir_content(inum, dir_ptr);

    // 获取目录下的文件数
    char buffer[BLOCK_SIZE];
    disk_read(DATA_START + inum, buffer);
    int dir_num = *(int *)(buffer);

    for (int i = 0; i < dir_num; i++)
        filler(buffer, (dir_ptr + i)->file_name, NULL, 0);
}

// 读取目录内容
void get_dir_content(int dir_pos, struct directory *dir_pointer) // 读取目录内容
{
    char buffer[BLOCK_SIZE];
    disk_read(DATA_START + dir_pos, buffer);
    int file_num = *(int *)(buffer);
    for (int i = 0; i < file_num; i++)
    {
        char *tmp = buffer + (i + 1) * DIR_SIZE;
        struct directory dir = (struct directory *)tmp;
        *(dir_pointer + i) = *dir;
    }
}

int get_indirect_num(int block_pos) // 获取间接指针的块号
{
    char content[BLOCK_SIZE];
    disk_read(DATA_START + block_pos, content);
    int indirect_pointer_num = *(int *)(content);
    if (indirect_pointer_num <= 0)
        return 0;
    else
        return indirect_pointer_num;
}

void get_indirect_block(int block_pos, int *inode_pointer) // 读取indirect block存的指针指向的datablock
{
    char buff[BLOCK_SIZE];
    disk_read(DATA_START + block_pos, buff);
    int indirect_ptr_num = *(int *)(buff);
    for (int i = 0; i < indirect_ptr_num; i++)
    {
        int *tmp = (int *)(buff + (i + 1) * sizeof(int));
        *(inode_num_ptr + i) = *tmp;
    }
}

void change_inodebitmap_state(int inode_num) // 改变inodebitmap状态
{
    char inode_bitmap[BLOCK_SIZE];
    disk_read(IBITMAP, inode_bitmap);
    int pos = inode_num / 8;
    int off = inode_num % 8;
    inode_bitmap[pos] = inode_bitmap[pos] ^ (1 << off);
    disk_write(IBITMAP, inode_bitmap);
}

void change_databitmap_state(int data_num) // 改变datablock bitmap状态
{
    // 读取databitmap
    char *data_bitmap = (char *)malloc(2 * sizeof(char) * BLOCK_SIZE);
    disk_read(DBITMAP1, data_bitmap);
    disk_read(DBITMAP2, data_bitmap);

    int pos = data_num / 8;
    int off = data_num % 8;
    data_bitmap[pos] = data_bitmap[pos] ^ (1 << off);
    disk_write(DBITMAP1, data_bitmap);
    disk_write(DBITMAP2, data_bitmap + BLOCK_SIZE);
    free(data_bitmap);
}

// 根据路径找到inode
int get_inode_num_by_path(char *path)
{
    struct inode dir_inode = get_inode_by_num(0); // 获取根目录的inode

    int file_inode_num = 0;
    int idx = 1, len = strlen(path);
    char filename[FILENAME_SIZE];
    while (idx < len)
    {
        int off = 0;
        while (idx < len && path[idx] != '/')
            filename[off++] = path[idx++];

        filename[off] = '\0';
        off++;
        idx++;

        int flag = find_directory(MIN(dir_inode.block_num, DIRECT_NUM), filename, dir_inode.block);
        if (flag == -1) // 在间接指针指向的数据块里
            for (int i = 0; i < MIN(dir_inode.block_num - DIRECT_NUM, 2); i++)
            {
                int indirect_num = get_indirect_num(dir_inode.block[i + DIRECT_NUM]);
                int *inum_pointer;
                get_indirect_block(dir_inode.block[i + DIRECT_NUM], inum_pointer);
                flag = find_directory(indirect_num, filename, inum_pointer);
            }

        if (flag == -1) // 还是没找到
            return -1;

        file_inode_num = flag;
        dir_inode = get_inode_by_num(file_inode_num); // 到下一个目录里
    }
    return file_inode_num;
}

// 根据块号读取inode
struct inode get_inode_by_num(int inode_num)
{
    int pos = inode_num / INODE_NUM_PER_BLOCK; // 所在块的编号
    int off = inode_num % INODE_NUM_PER_BLOCK; // 在块中的偏移量
    char content[BLOCK_SIZE];
    disk_read(INODE_START + pos, content);
    struct inode *inode_ptr = (struct inode *)malloc(INODE_SIZE);
    inode_ptr = (struct inode *)(content + off * INODE_SIZE);
    return inode_ptr[off];
}

int get_data_num(struct inode inode_content, int block_num) // 根据块号找到datablock
{
    if (block_num < DIRECT_NUM) // 直接指针
        return inode_content.block[block_num];

    int data_idx, block_off;
    if (block_num < DIRECT_NUM + MAX_DIR_NUM)
    {
        data_idx = inode_content.block[DIRECT_NUM];
        block_off = block_num - DIRECT_NUM - MAX_DIR_NUM;
    }
    return block_off;
}

void write_inode(int inode_num, struct inode inode_content) // 根据块号写入inode
{
    int pos = inode_num / INODE_NUM_PER_BLOCK;
    int off = inode_num % INODE_NUM_PER_BLOCK;
    char content[BLOCK_SIZE];
    disk_read(INODE_START + pos, content);
    memcpy(content + INODE_SIZE * off, (char *)(&inode_content), INODE_SIZE);
    disk_write(INODE_START + pos, content);
}

void init_datablock(int data_num); // 初始化datablock
{
    char content[BLOCK_SIZE];
    int zero = 0;
    memcpy(content, (char *)(&zero), sizeof(int));
    disk_write(DATA_START + data_num, content);
}

int find_empty_inode() // 找到空闲的inode块
{
    for (int i = 0; i < MAX_FILE_NUM; i++)
    {
        char buff[BLOCK_SIZE];
        diskread(IBITMAP, buff);
        int pos = i / 8;
        int off = i % 8;
        int val = (buff[pos] >> off) & (0x1);
        if (!val)
            return i;
    }
    return -1; // 没有空闲的inode块
}

int find_empty_datablock() // 找到空闲的datablock块
{
    for (int i = 0; i < MAX_DATA_NUM; i++)
    {
        // 读取databitmap
        char *data_bitmap = (char *)malloc(2 * sizeof(char) * BLOCK_SIZE);
        disk_read(DBITMAP1, data_bitmap);
        disk_read(DBITMAP2, data_bitmap);

        int pos = i / 8;
        int off = i % 8;
        int val = (buff[pos] >> off) & (0x1);
        free(data_bitmap);
        if (!val)
            return i;
    }
    return -1;
}

int find_directory(int inode_num, char *file_name, int *inum_pointer) // 根据inode和name找到目录
{
    for (int i = 0; i < inode_num; i++)
    {
        char buffer[BLOCK_SIZE];
        disk_read(DATA_START + inum_pointer[i], buffer);
        int dir_num = *(int *)(buffer); // 获取目录下的文件个数

        struct directory *dir_pointer;
        get_dir_content(inum_pointer[i], dir_pointer);
        int flag = -1;
        for (int j = 0; j < dir_num; j++)
            if (!strcmp((dir_pointer + j)->file_name, file_name))
                flag = (dir_pointer + j)->inode_num;

        if (flag == -1)
            return -1;
        return flag;
    }
}

int insert_dir_info(int dir_pos, struct directory dir) // 插入目录信息
{
    char buffer[BLOCK_SIZE];
    disk_read(DATA_START + dir_pos, buffer);
    // 获取目录下的文件个数
    int files_num = *(int *)(buffer);

    if (files_num >= MAX_files_num)
        return -1;
    struct directory *dir_pointer;
    get_dir_content(dir_pos, dir_pointer);
    int isSame = 0;
    for (int i = 0; i < files_num; i++)
        if (!strcmp((dir_pointer + i)->file_name, dir.file_name)) // 目录名相同
        {
            *(dir_pointer + i) = dir;
            isSame = 1;
        }

    if (!isSame)
    {
        files_num++;
        *(dir_pointer + files_num - 1) = dir;
    }
    // 写目录
    char buffer[BLOCK_SIZE];
    memcpy(buffer, (char *)(&files_num), sizeof(int));
    memcpy(buffer + DIR_SIZE, (char *)(dir_pointer), DIR_SIZE);
    disk_write(DATA_START + dir_pos, buffer);
    return files_num;
}

int insert_file_to_dir(const char *path, int inode_num) // 往目录插入新的文件/目录
{
    struct directory dir;
    char *filename; // 上一级文件名
    int len = strlen(path);
    int idx = len - 1;
    while (path[idx] != '/') // 找路径最后的文件名
        idx--;
    for (int i = idx + 1; i < len; i++)
        filename[i - idx - 1] = path[i];
    filename[len - idx - 1] = '\0';

    char *father_path = get_upper_path(path);                  // 上一级目录路径
    int father_inum = get_inode_num_by_path(father_path);      // 获取父目录的inode number
    struct inode father_inode = get_inode_by_num(father_inum); // 获取父目录的inode

    strcpy(dir.file_name, filename);
    dir.inode_num = inode_num;
    int flag = -1;

    for (int i = 0; i < MIN(father_inode.block_num, DIRECT_NUM); i++)
    {
        flag = insert_dir_info(father_inode.block[i], dir);
        if (flag != -1)
            break;
    }
    // 如果没插入成功并且block_num小于直接指针的个数，新开辟一个块
    if (flag == -1 && father_inode.block_num <= DIRECT_NUM)
    {
        int new_dnum = find_empty_datablock(); // 找空闲的块
        if (new_dnum == -1)
            return -ENOSPC;
        change_databitmap_state(new_dnum); // 修改data bitmap
        father_inode.block[father_inode.block_num] = new_dnum;
        father_inode.block_num++;
        init_datablock(new_dnum);
        flag = insert_dir_info(new_dnum, dir);
        father_inode.size += BLOCK_SIZE;
    }

    if (flag != -1) // 插入成功了
    {
        father_inode.ctime = time(NULL);
        father_inode.mtime = time(NULL);
        write_inode(father_inum, father_inode);
        return 0;
    }

    for (int i = 0; i < < MIN(2, father_inode.block_num - DIRECT_NUM); i++)
    {
        int indirect_num = get_indirect_num(father_inode.block[i + DIRECT_NUM]);
        int *inum_pointer = (int *)malloc(sizeof(int) * indirect_num);
        get_indirect_block(father_inode.block[i + DIRECT_NUM], inum_pointer);
        for (int j = 0; j < indirect_num; j++)
        {
            flag = insert_dir_info(inum_pointer[j], dir);
            if (flag != -1)
                break;
        }
        if (flag != -1)
        {
            free(inum_pointer);
            break;
        }

        if (indirect_num < MAX_PTR_NUM)
        {
            indirect_num++;
            int new_dnum = find_empty_datablock(); // 寻找空的块
            if (new_dnum == -1)
                return -ENOSPC;

            change_databitmap_state(new_dnum);
            inum_pointer = realloc(inum_pointer, indirect_num * sizeof(int));
            *(inum_pointer + indirect_num - 1) = new_dnum;
            init_datablock(new_dnum);
            flag = insert_dir_info(new_dnum, dir);

            char buffer[BLOCK_SIZE];
            char *num_str = (char *)(&indirect_num);
            char *array_str = (char *)inum_pointer;
            memcpy(buffer, num_str, sizeof(int));
            memcpy(buffer + sizeof(int), array_str, sizeof(int) * indirect_num);
            disk_write(DATA_START + father_inode.block[i + DIRECT_NUM], buffer);
            father_inode.size += BLOCK_SIZE;
        }

        if (indirect_num == MAX_PTR_NUM && father_inode.block_num == P_NUMBER - 1)
        {
            int new_dnum = find_empty_datablock();
            if (new_dnum == -1)
                return -ENOSPC;
            change_databitmap_state(new_dnum);
            father_inode.block[father_inode.block_num] = new_dnum;
            father_inode.block++;
            init_datablock(new_dnum);
            father_inode.size += BLOCK_SIZE;
        }

        free(inum_pointer);
        if (flag != -1)
            break;
    }

    if (flag != -1)
    {
        father_inode.ctime = time(NULL);
        father_inode.mtime = time(NULL);
        write_inode(father_inum, father_inode);
        return 0;
    }
    return -ENOSPC;
}

int rm_directory(int inode_pos, int inode_num, int *dir_inum_list) // 删除目录
{
    int flag = -1;
    for (int i = 0; i < inode_num; i++)
    {
        char buffer[BLOCK_SIZE];
        disk_read(DATA_START + dir_inum_list[i], buffer);
        int files_num = *(int *)(buffer); // 获取目录下的文件个数
        struct directory *dir_pointer;
        get_dir_content(dir_inum_list[i], dir_pointer);

        flag = -1;
        for (int j = 0; j < files_num; j++) // 根据编号找目录
        {
            if ((dir_pointer + j)->inode_num == INODE_START)
                flag = j;
        }

        if (flag == -1)
            continue;

        // 删除目录
        int k = flag;
        while (k < files_num - 1)
        {
            *(dir_pointer + k) = *(dir_pointer + k + 1);
            k++;
        }
        // 写目录
        char buffer[BLOCK_SIZE];
        char *char_num = (char *)(&(files_num - 1));
        memcpy(buffer, char_num, sizeof(int));
        char *char_dir_pointer = (char *)(dir_pointer);
        memcpy(buffer + DIR_SIZE, char_dir_pointer, DIR_SIZE);
        disk_write(DATA_START + dir_inum_list[i], buffer);

        return flag;
    }
}

// Format the virtual block device in the following function
int mkfs()
{
    printf("Mkfs is called.\n");
    // printf("mode_t%d\n\n",sizeof(mode_t));
    // printf("dir_data%d\n\n", sizeof(dir_data));
    struct statevfs fs;
    fs.f_blocks = BLOCK_NUM;
    fs.f_bsize = BLOCK_SIZE;
    fs.f_bfree = BLOCK_NUM - 4;
    fs.f_bavail = BLOCK_NUM - 4;
    fs.f_files = MAX_FILE_NUM;
    fs.f_ffree = MAX_FILE_NUM;
    fs.f_favail = MAX_FILE_NUM;
    fs.f_namemax = FILENAME_SIZE;
    // 写入superblock
    char *fs_str = (char *)(&fs);
    char buff[BLOCK_SIZE];
    memcpy(buff, fs_str, sizeof(struct statevfs));
    disk_write(SUPERBLOCK, buff);
    // 初始化inode
    struct inode root_inode;
    root_inode = initialize_inode(DIRMODE);
    // 找空闲块
    int inum = find_empty_inode();
    int dnum = find_empty_datablock();
    root_inode.block[0] = dnum;
    // 写入bitmap
    change_inodebitmap_state(inum);
    change_databitmap_state(dnum);
    // 写inode和datablock
    write_inode(inum, root_inode);
    init_datablock(dnum);
    return 0;
}

// Filesystem operations that you need to implement
// 查询一个目录文件或常规文件的信息
int fs_getattr(const char *path, struct stat *attr)
{
    printf("Getattr is called:%s\n", path);

    int inode_num = get_inode_num_by_path(path);
    if (inode_num == -1) // 没有找到
        return -ENOENT;

    struct inode Inode = get_inode_by_num(inode_num);
    // 逐一赋值
    attr->st_mode = Inode.mode;
    attr->st_nlink = 1;
    attr->st_uid = getuid();
    attr->st_gid = getgid();
    attr->st_size = Inode.size;
    attr->st_atime = Inode.atime;
    attr->st_mtime = Inode.mtime;
    attr->st_ctime = Inode.ctime;
    // printf("attr:%d %d\n", attr->st_mode, attr->st_nlink);
    return 0;
}

// 查询一个目录文件下的所有文件
int fs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    printf("Readdir is called:%s\n", path);
    int inum = get_inode_num_by_path(path);
    struct inode dir_inode = get_inode_by_num(inum);

    for (int i = 0; i < MIN(dir_inode.block_num, DIRECT_NUM); i++)
        read_data_in_dir(dir_inode.block[i], buffer, filler);
    // 访问间接指针
    for (int i = 0; i < MIN(2, dir_inode.block_num - DIRECT_NUM); i++)
    {
        int indirect_num = get_indirect_num(dir_inode.block[i + DIRECT_NUM]);
        int *inum_pointer;
        get_indirect_block(dir_inode.block[i + DIRECT_NUM], inum_pointer);
        for (int j = 0; j < indirect_num; j++)
            read_data_in_dir(inum_pointer[j], path, filler);
    }
    dir_inode.atime = time(NULL); // 更新访问时间
    write_inode(inum, dir_inode);
    return 0;
}

// 对一个常规文件进行读操作
int fs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("Read is called:%s\n", path);
    printf("size:%d\n", size);
    printf("offset:%d\n", offset);
    char *file_content;
    char block_content[BLOCK_SIZE];
    off_t off = 0;
    int read_flag = 0;
    int inode_num = get_inode_num_by_path(path);
    struct inode f_inode = get_inode_by_num(inode_num);
    size_t read_size = MIN(size, f_inode.size - offset);

    for (int i = 0; i < MIN(DIRECT_NUM, f_inode.block_num); i++)
    {
        disk_read(DATA_START + f_inode.block[i], block_content);
        size_t deal_size = MIN(BLOCK_SIZE, f_inode.size - off);
        memcpy(file_content + off, block_content, deal_size);
        off += deal_size;
        if (offset + size <= off)
        {
            read_flag = 1;
            break;
        }
    }

    if (read_flag)        // 读完了
        return read_size; // 返回读到的字节数

    for (int i = 0; i < MIN(2, f_inode.block_num - DIRECT_NUM); i++)
    {
        int indirect_num = read_indirect_num(f_inode.block[i + DIRECT_NUM]);
        int *inum_pointer;
        get_indirect_block(f_inode.block[i + DIRECT_NUM], inum_pointer);
        for (int j = 0; j < indirect_num; j++)
        {
            disk_read(DATA_START + f_inode.block[j], block_content);
            size_t deal_size = MIN(BLOCK_SIZE, f_inode.size - off); // 一次处理的大小
            memcpy(file_content + off, block_content, deal_size);
            off += deal_size;
            if (off - size >= offset)
            {
                read_flag = 1;
                break;
            }
        }
        if (read_flag)
            return read_size; // 返回读到的字节数
    }
}

// 创建一个目录文件
int fs_mkdir(const char *path, mode_t mode)
{
    printf("Mkdir is called:%s\n", path);
    struct inode dir_inode = initialize_inode(DIRMODE); // 创建并初始化dir_inode
    int inum = find_empty_inode();                      // 找到未使用的inode
    if (inum == -1)
    {
        printf("Cannot find the free inode!\n");
        return -ENOSPC;
    }
    int dnum = find_empty_datablock(); // 找到未使用的datablock
    if (dnum == -1)
    {
        printf("Cannot find the free datablock!\n");
        return -ENOSPC;
    }
    dir_inode.block[0] = dnum;
    change_inodebitmap_state(inum); // 更新bitmap
    change_databitmap_state(dnum);
    int val = insert_file_to_dir(path, inum); // 插入新文件
    if (val == -ENOSPC)
        return val;
    write_inode(inum, dir_inode); // 写入inode
    init_datablock(dnum);         // 写入datablock
    return 0;
}

// 删除一个目录文件
int fs_rmdir(const char *path)
{
    printf("Rmdir is called:%s\n", path);
    int inum = get_inode_num_by_path(path); // 根据路径找到inode编号
    if (inum == -1)
    {
        printf("The directory does not exit!\n");
        return 0;
    }
    struct inode dir_inode = get_inode_by_num(inum); // 根据编号获取inode
    // 释放直接指针指向的块
    for (int i = 0; i < MIN(DIRECT_NUM, dir_inode.block_num); i++)
        change_databitmap_state(dir_inode.block[i]); // 改变databitmap状态
    // 释放间接指针指向的块
    for (int i = 0; i < MIN(2, dir_inode.block_num - DIRECT_NUM); i++)
    {
        int indirect_num = get_indirect_num(dir_inode.block[i + DIRECT_NUM]);
        int *inum_pointer;
        get_indirect_block(dir_inode.block[i + DIRECT_NUM], inum_pointer); // 获取间接指针指向的数据块
        for (int j = 0; j < indirect_num; j++)                             // 也要将其状态改为未占用
            change_databitmap_state(inum_pointer[j]);
        change_databitmap_state(dir_inode.block[i + DIRECT_NUM]);
    }
    change_inodebitmap_state(inum);
    // 更新父目录
    char *father_path = get_upper_path(path);                  // 获取上一级目录
    int father_inum = get_inode_num_by_path(father_path);      // 找到inode number
    struct inode father_inode = get_inode_by_num(father_inum); // 根据编号获取inode
    father_inode.mtime = time(NULL);                           // 更新时间
    father_inode.ctime = time(NULL);
    write_inode(father_inum, father_inode);
    return 0;
}

// 删除一个常规文件
int fs_unlink(const char *path)
{
    printf("Unlink is callded:%s\n", path);

    int inode_num = get_inode_num_by_path(path);
    if (inum == -1)
    {
        printf("The file is not exited!\n");
        return 0;
    }
    struct inode file_inode = get_inode_by_num(inode_num);
    // 释放直接指针指向的块
    for (int i = 0; i < MIN(DIRECT_NUM, file_inode.block_num); i++)
        change_databitmap_state(file_inode.block[i]);
    // 释放间接指针指向的块
    for (int i = 0; i < MIN(2, file_inode.block_num - DIRECT_NUM); i++)
    {
        int indirect_num = get_indirect_num(file_inode.block[i + DIRECT_NUM]);
        int *inum_pointer;
        get_indirect_block(file_inode.block[i + DIRECT_NUM], inum_pointer);

        for (int j = 0; j < indirect_num; j++)
            change_databitmap_state(inum_pointer[j]);
        change_databitmap_state(file_inode.block[i + DIRECT_NUM]);
    }

    // 改变inode bitmap状态
    change_inodebitmap_state(inode_num);

    // 更新父目录
    char *father_path = get_upper_path(path);                  // 获取上一级目录
    int father_inum = get_inode_num_by_path(father_path);      // 找到inode number
    struct inode father_inode = get_inode_by_num(father_inum); // 根据编号获取inode
    father_inode.mtime = time(NULL);                           // 更新时间
    father_inode.ctime = time(NULL);
    write_inode(father_inum, father_inode);
    return 0;
}

// 更改一个目录文件或常规文件的名称（或路径）
int fs_rename(const char *oldpath, const char *newname)
{
    printf("Rename is called:%s\n", newname);
    int inode_num = get_inode_num_by_path(oldpath);
    if (inode_num == -1)
    {
        printf("The file is not exited!\n");
        return 0;
    }

    char *father_path = get_upper_path(oldpath);               // 获取上一级目录
    int father_inum = get_inode_num_by_path(father_path);      // 找到inode number
    struct inode father_inode = get_inode_by_num(father_inum); // 根据编号获取inode

    int flag = -1;
    flag = rm_directory(inode_num, MIN(father_inode.block_num, DIRECT_NUM), father_inode.block);
    if (flag == -1) // 未成功删除,找间接指针
    {
        for (int i = 0; i < MIN(father_inode.block_num - DIRECT_NUM， 2); i++)
        {
            int indirect_num = get_indirect_num(father_inode.block[i + DIRECT_NUM]);
            int *father_inum_pointer;
            get_indirect_block(father_inode.block[i + DIRECT_NUM], father_inum_pointer);
            flag = rm_directory(inode_num, indirect_num, father_inum_pointer);
        }
    }

    father_inode.mtime = time(NULL); // 更新时间
    father_inode.ctime = time(NULL);
    write_inode(father_inum, father_inode);

    int val = insert_file_to_dir(newname, father_inum);
    if (val == -ENOSPC)
        return val;
    return 0;
}

// 修改一个常规文件的大小信息
int fs_truncate(const char *path, off_t size)
{
    printf("Truncate is called:%s\n", path);
    int inum = get_inode_num_by_path(path);
    struct inode file_inode = get_inode_by_num(inum);

    int old_blocks = file_inode.block_num;
    int new_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE - 1;

    if (size > file_inode.size) // 大于原来的大小
    {
        int diff_blocks = new_blocks - old_blocks;
        while (diff_blocks > 0 && file_inode.block_num < DIRECT_NUM)
        { // 新增的块可以放在直接指针里
            diff_blocks -= 1;
            int new_dnum = find_empty_datablock(); // 找空的datablock
            if (new_dnum == -1)                    // 没有足够空间
                return -ENOSPC;
            change_databitmap_state(new_dnum);
            new_dnum += DATA_START;
            file_inode.block[file_inode.block_num] = new_dnum;
            file_inode.block_num++;
        }

        // 新增的块还没分配完
        while (diff_blocks > 0)
        {

            if (inode_content.block_num == DIRECT_NUM)
            {
                int new_dnum = find_empty_datablock();
                if (new_dnum == -1)
                    return -ENOSPC;

                change_databitmap_state(new_dnum);
                file_inode.block[file_inode.block_num] = new_dnum;
                file_inode.block_num++;
                init_datablock(new_dnum);
                file_inode.size += BLOCK_SIZE;
            }
            // 访问间接指针
            for (int i; i < MIN(2, file_inode.block_num - DIRECT_NUM); i++)
            {
                int indirect_num = get_indirect_num(file_inode.block[i + DIRECT_NUM]);
                int *inum_pointer;
                get_indirect_block(file_inode.block[i + DIRECT_NUM], inum_pointer);

                int increased_num = MIN(diff_blocks, MAX_DIR_NUM - indirect_num);
                diff_blocks -= increased_num;

                for (int j = indirect_num; j < indirect_num + increased_num; j++)
                {
                    int new_dnum = find_empty_datablock();
                    if (new_dnum == -1)
                        return -ENOSPC;
                    change_databitmap_state(new_dnum);
                    *(inum_pointer + j) = new_dnum;
                }
                indirect_num += increased_num;
                char buff[BLOCK_SIZE];
                memcpy(buff, (char *)(&indirect_num), sizeof(int));
                memcpy(buff + sizeof(int), (char *)inum_pointer, sizeof(int) * indirect_num);
                disk_write(DATA_START + file_inode.block[i + DIRECT_NUM], buff);
            }
        }
        if (diff_blocks > 0)
            return -ENOSPC;
    }
    else
    {
        for (int i = old_blocks; i > new_blocks; i--)
        {
            int indirect_num;
            if (i < DIRECT_NUM)
                indirect_num = 0;
            else if (i < DIRECT_NUM + MAX_DIR_NUM)
                indirect_num = DIRECT_NUM;
            else
                indirect_num = DIRECT_NUM + 1;

            int dnum = get_data_num(inode_content, i);

            change_databitmap_state(dnum);
            if (!indirect_num) // 没有间接指针
                inode_content.block_num -= 1;
            else
            {
                char buff[BLOCK_SIZE];
                disk_read(DATA_START + indirect_num, buff);

                int num = (int *)(buff)[0];
                num--;
                file_inode.block_num--;
                if (!num) // 如果刚好所有indirectblock都空了
                {
                    change_databitmap_state(indirect_num);
                    inode_content.block_num--;
                }
                else
                {
                    memcpy(buff, (char *)(&num), sizeof(int));
                    disk_write(DATA_START + indirect_num, buff);
                }
            }
        }
    }
    file_inode.size = size;
    file_inode.ctime = time(NULL);
    write_inode(inode_num, file_inode);
    return 0;
}

// 修改一个目录文件或常规文件的时间信息
int fs_utime(const char *path, struct utimbuf *buffer)
{
    printf("Utime is called:%s\n", path);
    int inum = get_inode_num_by_path(path);
    struct inode inode_content = get_inode_by_num(inum);
    // 修改buffer的atime mtime
    buffer->actime = inode_content.atime;
    buffer->modtime = inode_content.mtime;
    inode_content.ctime = time(NULL); // 更新文件的ctime
    write_inode_by_inum(inum, inode_content);
    return 0;
}

// 创建一个常规文件
int fs_mknod(const char *path, mode_t mode, dev_t dev)
{
    printf("Mknod is called:%s\n", path);
    int father_inum = get_inode_num_by_path(path); // 寻找父目录
    if (father_inum == -1)
        return -ENOSPC;

    int file_inum = find_empty_inode();
    if (file_inum == -1)
        return -ENOSPC;
    change_inodebitmap_state(file_inum);

    struct inode file_inode;
    file_inode = initialize_inode(REGMODE); // 初始化常规文件的inode

    int flag = insert_file_to_dir(path, inum);
    if (flag == -ENOSPC)
        return flag;
    write_inode(inum, file_inode);
    return 0;
}

// 对文件进行写操作
int fs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("Write is called:%s\n", path);
    // printf("size:%d\n", size);
    // printf("offset:%d\n", offset);
    off_t new_size = offset + size; // 写后的大小
    if (fs_truncate(path, new_size) == -ENOSPC)
        return 0;

    int begin_block = offset / BLOCK_SIZE; // 开始写的块
    int begin_off = offset % BLOCK_SIZE;
    int end_block = (offset + size - 1) / BLOCK_SIZE; // 结束的块
    int end_off = (offset + size - 1) % BLOCK_SIZE;

    struct inode file_inode = get_inode_by_num(get_inode_num_by_path(path));
    char content[BLOCK_SIZE];
    // 记录写到哪里了
    int buffer_off;

    for (int i = begin_block; i <= end_block; i++)
    {
        // 要写入的datablock的块号
        int dnum = get_data_num(file_inode, i);

        if (i == begin_block)
        {
            disk_read(DATA_START + dnum, content);
            memcpy(content + begin_off, buffer, BLOCK_SIZE - begin_off);
            buffer_off += BLOCK_SIZE - begin_off;
            if (i == end_block)
                memcpy(content + begin_off, buffer, end_off - begin_off + 1);
        }
        else
        {
            if (i == end_block)
                memcpy(content, buffer + buffer_off, end_off + 1);
            else
            {
                memcpy(content, buffer + buffer_off, BLOCK_SIZE);
                buffer_off += BLOCK_SIZE;
            }
        }
        disk_write(DATA_START + dnum, content);
    }
    // 更新mtime和ctime
    file_inode.mtime = time(NULL);
    file_inode.ctime = time(NULL);
    return size;
}

// 查询文件系统整体的统计信息
int fs_statfs(const char *path, struct statvfs *stat)
{
    printf("Statfs is called:%s\n", path);
    struct statevfs fs;
    // 获取superblock
    char buff[BLOCK_SIZE];
    struct statevfs fs_content;
    disk_read(SUPERBLOCK, buff);
    fs = *(struct statevfs *)buff;

    stat->f_bsize = fs.f_bsize;
    stat->f_blocks = fs.f_blocks;
    stat->f_bfree = fs.f_bfree;
    stat->f_bavail = fs.f_bavail;
    stat->f_files = fs.f_files;
    stat->f_ffree = fs.f_ffree;
    stat->f_favail = fs.f_favail;
    stat->f_namemax = fs.f_namemax;
    return 0;
}

int fs_open(const char *path, struct fuse_file_info *fi)
{
    printf("Open is called:%s\n", path);
    return 0;
}

// Functions you don't actually need to modify
int fs_release(const char *path, struct fuse_file_info *fi)
{
    printf("Release is called:%s\n", path);
    return 0;
}

int fs_opendir(const char *path, struct fuse_file_info *fi)
{
    printf("Opendir is called:%s\n", path);
    return 0;
}

int fs_releasedir(const char *path, struct fuse_file_info *fi)
{
    printf("Releasedir is called:%s\n", path);
    return 0;
}

static struct fuse_operations fs_operations = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .read = fs_read,
    .mkdir = fs_mkdir,
    .rmdir = fs_rmdir,
    .unlink = fs_unlink,
    .rename = fs_rename,
    .truncate = fs_truncate,
    .utime = fs_utime,
    .mknod = fs_mknod,
    .write = fs_write,
    .statfs = fs_statfs,
    .open = fs_open,
    .release = fs_release,
    .opendir = fs_opendir,
    .releasedir = fs_releasedir};

int main(int argc, char *argv[])
{
    if (disk_init())
    {
        printf("Can't open virtual disk!\n");
        return -1;
    }
    if (mkfs())
    {
        printf("Mkfs failed!\n");
        return -2;
    }
    return fuse_main(argc, argv, &fs_operations, NULL);
}