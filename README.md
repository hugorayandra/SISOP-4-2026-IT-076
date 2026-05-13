# README Soal 2 SISOP Modul 4

## Identitas

Nama : Muhammad Hugo Rayandra
NRP : 5027251076

---

# Deskripsi Soal

Project MOO adalah mini database service dengan struktur folder database dan file CSV sebagai tabel. Program dapat diakses melalui TCP connection pada port 9000.

Tugas utama pada soal ini adalah membuat filesystem virtual menggunakan FUSE yang menghubungkan direktori asli `encrypted_storage` dengan mount point `fuse_mount`.

Filesystem harus:

* Mendukung operasi filesystem dasar.
* Menyimpan file terenkripsi XOR.
* Menampilkan file normal pada mount point.
* Mendukung containerization menggunakan Docker.
* Mendukung komunikasi client ke server menggunakan TCP.

---

# Struktur Folder

```text
soal_2/
├── Dockerfile
├── client.c
├── encrypted_storage/
├── fuse.c
├── fuse_mount/
└── server
```

---

# Persiapan Dependency

Install dependency yang dibutuhkan:

```bash
sudo apt update

sudo apt install gcc fuse3 libfuse3-dev docker.io -y
```

Penjelasan:

* `gcc` digunakan untuk compile program C.
* `fuse3` digunakan untuk filesystem virtual.
* `libfuse3-dev` berisi header development FUSE.
* `docker.io` digunakan untuk containerization.

---

# Konfigurasi FUSE pada WSL

Buka file konfigurasi:

```bash
sudo nano /etc/fuse.conf
```

Cari:

```text
#user_allow_other
```

Ubah menjadi:

```text
user_allow_other
```

Save:

```text
CTRL + O
ENTER
CTRL + X
```

Penjelasan:

Konfigurasi ini mengizinkan filesystem FUSE dapat diakses dengan benar pada environment WSL.

---

# Pembersihan Environment

Unmount mount point lama:

```bash
sudo umount -l fuse_mount
```

Hapus file build dan folder lama:

```bash
rm -rf fuse

rm -rf client

rm -rf encrypted_storage

rm -rf fuse_mount
```

Penjelasan:

Tahapan ini dilakukan agar tidak terjadi ghost mount atau corrupt mount point pada WSL.

---

# Membuat Direktori

```bash
mkdir encrypted_storage

mkdir fuse_mount
```

Penjelasan:

* `encrypted_storage` digunakan untuk menyimpan file asli dalam keadaan terenkripsi.
* `fuse_mount` digunakan sebagai mount point filesystem virtual.

---

# Source Code fuse.c

```c
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
```

---

# Penjelasan Per Bagian fuse.c

## Konstanta dan Direktori

```c
static const char *dirpath = "encrypted_storage";
static const unsigned char KEY = 0x76;
```

Penjelasan:

* `dirpath` digunakan sebagai direktori penyimpanan asli.
* `KEY` digunakan untuk enkripsi XOR.

---

## Fungsi xor_data

```c
void xor_data(char *buf, size_t size)
```

Penjelasan:

Fungsi ini melakukan enkripsi dan dekripsi menggunakan operasi XOR.

Karena XOR bersifat reversible, fungsi yang sama dapat digunakan untuk:

* encrypt
* decrypt

---

## Fungsi make_path

```c
void make_path(char fpath[1024], const char *path)
```

Penjelasan:

Fungsi ini mengubah path virtual menjadi path asli terenkripsi.

Contoh:

```text
/test
```

menjadi:

```text
encrypted_storage/test.enc
```

---

## Callback getattr

```c
static int xmp_getattr
```

Penjelasan:

Digunakan untuk mengambil metadata file seperti:

* ukuran file
* permission
* inode
* timestamp

---

## Callback readdir

```c
static int xmp_readdir
```

Penjelasan:

Digunakan untuk membaca isi direktori.

Pada callback ini:

* file `.enc` disembunyikan
* file ditampilkan tanpa ekstensi `.enc`

Contoh:

```text
test.enc
```

akan terlihat menjadi:

```text
test
```

---

## Callback create

```c
static int xmp_create
```

Penjelasan:

Digunakan ketika user membuat file baru pada mount point.

File yang dibuat otomatis:

```text
terenkripsi
```

pada direktori `encrypted_storage`.

---

## Callback open

```c
static int xmp_open
```

Penjelasan:

Digunakan untuk membuka file terenkripsi.

---

## Callback read

```c
static int xmp_read
```

Penjelasan:

Data dibaca dari file `.enc` lalu dilakukan decrypt XOR sebelum diberikan ke user.

---

## Callback write

```c
static int xmp_write
```

Penjelasan:

Data yang ditulis user akan dienkripsi terlebih dahulu menggunakan XOR sebelum disimpan.

---

## Callback release

```c
static int xmp_release
```

Penjelasan:

Digunakan untuk menutup file descriptor.

---

# Source Code client.c

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

int main()
{
    int sock;

    struct sockaddr_in server_addr;

    char buffer[4096];

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if(sock < 0)
    {
        printf("Socket Error\n");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(sock,
               (struct sockaddr *)&server_addr,
               sizeof(server_addr)) < 0)
    {
        printf("Connection Failed\n");
        return 1;
    }

    printf("CONNECTED TO SERVER\n");

    while(1)
    {
        memset(buffer, 0, sizeof(buffer));

        fgets(buffer, sizeof(buffer), stdin);

        send(sock, buffer, strlen(buffer), 0);

        memset(buffer, 0, sizeof(buffer));

        recv(sock, buffer, sizeof(buffer), 0);

        printf("%s\n", buffer);
    }

    close(sock);

    return 0;
}
```

---

# Penjelasan client.c

## socket

```c
socket(AF_INET, SOCK_STREAM, 0)
```

Penjelasan:

Digunakan untuk membuat socket TCP.

---

## connect

```c
connect(sock,
        (struct sockaddr *)&server_addr,
        sizeof(server_addr))
```

Penjelasan:

Digunakan untuk melakukan koneksi ke server pada port 9000.

---

## send

```c
send(sock, buffer, strlen(buffer), 0)
```

Penjelasan:

Digunakan untuk mengirim command ke server.

---

## recv

```c
recv(sock, buffer, sizeof(buffer), 0)
```

Penjelasan:

Digunakan untuk menerima response dari server.

---

# Source Code Dockerfile

```dockerfile
FROM ubuntu:latest

WORKDIR /app

COPY server /app/server

RUN chmod +x /app/server

EXPOSE 9000

CMD ["./server"]
```

---

# Penjelasan Dockerfile

## FROM

```dockerfile
FROM ubuntu:latest
```

Penjelasan:

Menggunakan image Ubuntu terbaru sebagai base image.

---

## WORKDIR

```dockerfile
WORKDIR /app
```

Penjelasan:

Menentukan direktori kerja utama container.

---

## COPY

```dockerfile
COPY server /app/server
```

Penjelasan:

Menyalin binary server ke dalam container.

---

## EXPOSE

```dockerfile
EXPOSE 9000
```

Penjelasan:

Membuka port 9000 untuk koneksi TCP.

---

## CMD

```dockerfile
CMD ["./server"]
```

Penjelasan:

Menjalankan binary server ketika container dijalankan.

---

# Compile Program

Compile FUSE:

```bash
gcc fuse.c -o fuse `pkg-config fuse3 --cflags --libs`
```

Compile client:

```bash
gcc client.c -o client
```

Permission server:

```bash
chmod +x server
```

---

# Menjalankan FUSE

```bash
sudo ./fuse -f fuse_mount
```

Penjelasan:

* `-f` digunakan agar FUSE berjalan pada foreground.
* `sudo` digunakan agar permission mount pada WSL tidak gagal.

Ketika berhasil:

* terminal akan diam
* filesystem virtual sudah aktif

---

# Testing FUSE

Buka terminal baru:

```bash
cd ~/SISOP-04-2026-IT-076/soal_2
```

Membuat file:

```bash
echo "halo" > fuse_mount/test
```

Membaca file:

```bash
cat fuse_mount/test
```

Output:

```text
halo
```

Cek file terenkripsi:

```bash
ls encrypted_storage
```

Output:

```text
test.enc
```

Melihat isi terenkripsi:

```bash
cat encrypted_storage/test.enc
```

Output akan berupa karakter acak hasil XOR.

---

# Build Docker

```bash
docker build -t soal-2-modul-4-sisop .
```

Penjelasan:

Command ini digunakan untuk membuat Docker image.

---

# Menjalankan Docker

```bash
docker run -d \
--name db_app \
-p 9000:9000 \
-v $(pwd)/fuse_mount:/app/db \
soal-2-modul-4-sisop
```

Penjelasan:

* `-d` menjalankan container di background.
* `--name db_app` memberi nama container.
* `-p 9000:9000` melakukan port forwarding.
* `-v` melakukan bind mount filesystem FUSE ke container.

---

# Mengecek Container

```bash
docker ps
```

Melihat log:

```bash
docker logs db_app
```

---

# Menjalankan Client

```bash
./client
```

Output:

```text
CONNECTED TO SERVER
```

---

# Testing Database

Menampilkan command:

```text
HELP
```

Melihat database:

```text
LIST
```

Membuat database:

```text
CREATE DATABASE tests
```

Membuat tabel:

```text
CREATE TABLE tests users email password
```

Melihat tabel:

```text
LIST TABLE tests
```

---

# Menghentikan Program

Stop Docker:

```bash
docker stop db_app
```

Hapus container:

```bash
docker rm db_app
```

Stop FUSE:

```text
CTRL + C
```

Unmount FUSE:

```bash
fusermount3 -u fuse_mount
```
