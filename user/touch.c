#include <lib.h>

int touch(char *path) {
    int fd;
    if ((fd = open(path, O_RDONLY)) >= 0) {
        close(fd);
        return 0;
    }
    fd = open(path, O_CREAT);
    if (fd == -10) {
        printf("touch: cannot touch '%s': No such file or directory\n", path);
        return 1;
    } else if (fd >= 0) {
        close(fd);
    }
    return 0;
}

int main(int argc, char **argv) {
    int res = 0;
    if (argc < 2) {
        return 1;
    } else {
        for (int i = 1; i < argc; i++) {
            res += touch(argv[i]);
        }
    }
    return res;
}