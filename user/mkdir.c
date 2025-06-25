#include <lib.h>

int mkdir(char *path, int p) {
    int fd;
    if (p) {
        if ((fd = open(path, O_RDONLY)) >= 0) {
            close(fd);
            return 0;
        }
        int i = 0;
        char str[1024];
        for (int i = 0; path[i] != '\0'; i++) {
            if (path[i] == '/') {
                str[i] = '\0';
                if ((fd = open(path, O_RDONLY)) >= 0) {
                    close(fd);
                } else {
                    break;
                }
            }
            str[i] = path[i];
        }
        for (; path[i] != '\0'; i++) {
            if (path[i] == '/') {
                str[i] = '\0';
                fd = open(str, O_MKDIR);
                if (fd >= 0) {
                    close(fd);
                }
            }
            str[i] = path[i];
        }
        str[i] = '\0';
        fd = open(str, O_MKDIR);
        if (fd >= 0) {
            close(fd);
        }
    } else {
        if ((fd = open(path, O_RDONLY)) >= 0) {
            close(fd);
            printf("mkdir: cannot create directory '%s': File exists\n", path);
            return 1;
        }
        fd = open(path, O_MKDIR);
        if (fd == -10) {
            printf("mkdir: cannot create directory '%s': No such file or "
                   "directory\n",
                   path);
            return 1;
        } else if (fd >= 0) {
            close(fd);
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    int p = 0;

    ARGBEGIN {
    case 'p':
        p = 1;
        break;
    }
    ARGEND

    int res = 0;
    if (argc == 0) {
        return 1;
    } else {
        for (int i = 0; i < argc; i++) {
            if (argv[i] == 0) {
                continue;
            }
            res += mkdir(argv[i], p);
        }
    }
    return res;
}