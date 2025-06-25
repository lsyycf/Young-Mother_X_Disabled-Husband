#include <lib.h>

int rm(char *path, int r, int f) {
    int fd;
    struct Stat st;
    if ((fd = open(path, O_RDONLY)) < 0) {
        if (!f) {
            printf("rm: cannot remove '%s': No such file or directory\n", path);
        }
        return 1;
    }
    close(fd);
    stat(path, &st);
    if (st.st_isdir && !r) {
        printf("rm: cannot remove '%s': Is a directory\n", path);
        return 1;
    }
    remove(path);
    return 0;
}

int main(int argc, char **argv) {
    int r = 0, f = 0;

    ARGBEGIN {
    case 'r':
        r = 1;
        break;
    case 'f':
        f = 1;
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
            res += rm(argv[i], r, f);
        }
    }
    return res;
}