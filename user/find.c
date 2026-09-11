#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fs.h"

void find(char *path, char *target) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0) {
        printf("find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        printf("find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue; 

        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
            continue;

        if (strlen(path) + 1 + strlen(de.name) + 1 > sizeof(buf)) {
            printf("find: path too long\n");
            continue;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p = '/';
        p ++;
        strcpy(p, de.name);

        if (stat(buf, &st) < 0) {//stat函数会读取文件类型并把类型返回至st.type
            printf("find: cannot stat %s\n", buf);
            continue;
        }


        switch (st.type) {
            case T_FILE:
                if (strcmp(de.name, target) == 0) {
                    printf("%s\n", buf);
                }
                break;
            case T_DIR:
                if (strcmp(de.name, target) == 0) {
                    printf("%s\n", buf);   
                }
                find(buf, target);
                break;
        }
    }

    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("usage: find <path> <filename>\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}