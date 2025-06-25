#include <lib.h>

int flag[256];

int lsdir(char *, char *);
void ls1(char *, u_int, u_int, char *);

int ls(char *path, char *prefix) {
    int r;
    struct Stat st;

    if ((r = stat(path, &st)) < 0) {
        printf("stat %s: %d", path, r);
        return 1;
    }
    if (st.st_isdir && !flag['d']) {
        return lsdir(path, prefix);
    } else {
        ls1(0, st.st_isdir, st.st_size, path);
        return 0;
    }
}

int lsdir(char *path, char *prefix) {
    int fd, n;
    struct File f;

    if ((fd = open(path, O_RDONLY)) < 0) {
        printf("open %s: %d", path, fd);
    }
    while ((n = readn(fd, &f, sizeof f)) == sizeof f) {
        if (f.f_name[0]) {
            ls1(prefix, f.f_type == FTYPE_DIR, f.f_size, f.f_name);
        }
    }
    if (n > 0) {
        printf("short read in directory %s", path);
        return 1;
    }
    if (n < 0) {
        printf("error reading directory %s: %d", path, n);
        return 1;
    }
    return 0;
}

void ls1(char *prefix, u_int isdir, u_int size, char *name) {
    char *sep;

    if (flag['l']) {
        printf("%11d %c ", size, isdir ? 'd' : '-');
    }
    if (prefix) {
        if (prefix[0] && prefix[strlen(prefix) - 1] != '/') {
            sep = "/";
        } else {
            sep = "";
        }
        printf("%s%s", prefix, sep);
    }
    printf("%s", name);
    if (flag['F'] && isdir) {
        printf("/");
    }
    printf(" ");
}

void usage(void) {
    printf("usage: ls [-dFl] [file...]\n");
    exit();
}

int main(int argc, char **argv) {
    int i;

    ARGBEGIN {
    default:
        usage();
    case 'd':
    case 'F':
    case 'l':
        flag[(u_char)ARGC()]++;
        break;
    }
    ARGEND

    if (argc == 0) {
        ls("/", "");
    } else {
        for (i = 0; i < argc; i++) {
            ls(argv[i], argv[i]);
        }
    }
    printf("\n");
    return 0;
}
