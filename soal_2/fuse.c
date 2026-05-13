#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

static const char *dirpath = "encrypted_storage";
static const unsigned char KEY = 0x76;

void xor_data(char *buf, size_t size)
{
    for(size_t i = 0; i < size; i++)
    {
        buf[i] ^= KEY;
    }
}

void make_path(char fpath[1024], const char *path)
{
    if(strcmp(path, "/") == 0)
    {
        sprintf(fpath, "%s", dirpath);
    }
    else
    {
        sprintf(fpath, "%s%s.enc", dirpath, path);
    }
}

static int xmp_getattr(const char *path,
                       struct stat *stbuf,
                       struct fuse_file_info *fi)
{
    (void) fi;

    int res;
    char fpath[1024];

    memset(stbuf, 0, sizeof(struct stat));

    if(strcmp(path, "/") == 0)
    {
        res = lstat(dirpath, stbuf);
    }
    else
    {
        make_path(fpath, path);
        res = lstat(fpath, stbuf);
    }

    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_readdir(const char *path,
                       void *buf,
                       fuse_fill_dir_t filler,
                       off_t offset,
                       struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags)
{
    (void) offset;
    (void) fi;
    (void) flags;
    (void) path;

    DIR *dp;
    struct dirent *de;

    dp = opendir(dirpath);

    if(dp == NULL)
        return -errno;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    while((de = readdir(dp)) != NULL)
    {
        if(strcmp(de->d_name, ".") == 0 ||
           strcmp(de->d_name, "..") == 0)
            continue;

        char name[1024];

        strcpy(name, de->d_name);

        char *ext = strstr(name, ".enc");

        if(ext != NULL)
            *ext = '\0';

        filler(buf, name, NULL, 0, 0);
    }

    closedir(dp);

    return 0;
}

static int xmp_create(const char *path,
                      mode_t mode,
                      struct fuse_file_info *fi)
{
    char fpath[1024];

    make_path(fpath, path);

    int fd = open(fpath,
                  fi->flags | O_CREAT,
                  mode);

    if(fd == -1)
        return -errno;

    fi->fh = fd;

    return 0;
}

static int xmp_open(const char *path,
                    struct fuse_file_info *fi)
{
    char fpath[1024];

    make_path(fpath, path);

    int fd = open(fpath, O_RDWR);

    if(fd == -1)
        return -errno;

    fi->fh = fd;

    return 0;
}

static int xmp_read(const char *path,
                    char *buf,
                    size_t size,
                    off_t offset,
                    struct fuse_file_info *fi)
{
    (void) path;

    int res;

    res = pread(fi->fh, buf, size, offset);

    if(res == -1)
        return -errno;

    xor_data(buf, res);

    return res;
}

static int xmp_write(const char *path,
                     const char *buf,
                     size_t size,
                     off_t offset,
                     struct fuse_file_info *fi)
{
    (void) path;

    char *tmp = malloc(size);

    memcpy(tmp, buf, size);

    xor_data(tmp, size);

    int res = pwrite(fi->fh, tmp, size, offset);

    free(tmp);

    if(res == -1)
        return -errno;

    return res;
}

static int xmp_release(const char *path,
                       struct fuse_file_info *fi)
{
    (void) path;

    close(fi->fh);

    return 0;
}

static const struct fuse_operations oper =
{
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .create = xmp_create,
    .open = xmp_open,
    .read = xmp_read,
    .write = xmp_write,
    .release = xmp_release,
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &oper, NULL);
}
