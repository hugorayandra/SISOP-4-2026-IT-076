#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

static char source_dir[1024];

void build_path(char fpath[1024], const char *path)
{
    sprintf(fpath, "%s%s", source_dir, path);
}

static void generate_tujuan(char *result)
{
    char filepath[1024];
    char line[1024];

    strcpy(result, "Tujuan Mas Amba: ");

    for (int i = 1; i <= 7; i++)
    {
        sprintf(filepath, "%s/%d.txt", source_dir, i);

        FILE *fp = fopen(filepath, "r");

        if (fp == NULL)
            continue;

        while (fgets(line, sizeof(line), fp))
        {
            if (strncmp(line, "KOORD:", 6) == 0)
            {
                line[strcspn(line, "\n")] = 0;
                strcat(result, line + 6);
            }
        }

        fclose(fp);
    }

    strcat(result, "\n");
}

static int xmp_getattr(const char *path,
                       struct stat *stbuf,
                       struct fuse_file_info *fi)
{
    (void) fi;

    int res;
    char fpath[1024];

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/tujuan.txt") == 0)
    {
        char content[10000];

        generate_tujuan(content);

        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(content);

        return 0;
    }

    build_path(fpath, path);

    res = lstat(fpath, stbuf);

    if (res == -1)
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

    DIR *dp;
    struct dirent *de;

    char fpath[1024];

    build_path(fpath, path);

    dp = opendir(fpath);

    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL)
    {
        struct stat st;

        memset(&st, 0, sizeof(st));

        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        if (filler(buf, de->d_name, &st, 0, 0))
            break;
    }

    closedir(dp);

    filler(buf, "tujuan.txt", NULL, 0, 0);

    return 0;
}

static int xmp_open(const char *path,
                    struct fuse_file_info *fi)
{
    int res;
    char fpath[1024];

    if (strcmp(path, "/tujuan.txt") == 0)
        return 0;

    build_path(fpath, path);

    res = open(fpath, fi->flags);

    if (res == -1)
        return -errno;

    close(res);

    return 0;
}

static int xmp_read(const char *path,
                    char *buf,
                    size_t size,
                    off_t offset,
                    struct fuse_file_info *fi)
{
    (void) fi;

    int fd;
    int res;

    char fpath[1024];

    if (strcmp(path, "/tujuan.txt") == 0)
    {
        char content[10000];

        generate_tujuan(content);

        size_t len = strlen(content);

        if (offset < len)
        {
            if (offset + size > len)
                size = len - offset;

            memcpy(buf, content + offset, size);
        }
        else
        {
            size = 0;
        }

        return size;
    }

    build_path(fpath, path);

    fd = open(fpath, O_RDONLY);

    if (fd == -1)
        return -errno;

    res = pread(fd, buf, size, offset);

    if (res == -1)
        res = -errno;

    close(fd);

    return res;
}

static struct fuse_operations xmp_oper =
{
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .open = xmp_open,
    .read = xmp_read,
};

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr,
                "Usage: %s <source_directory> <mountpoint>\n",
                argv[0]);

        return 1;
    }

    realpath(argv[1], source_dir);

    char *fuse_argv[2];

    fuse_argv[0] = argv[0];
    fuse_argv[1] = argv[2];

    return fuse_main(2, fuse_argv, &xmp_oper, NULL);
}
