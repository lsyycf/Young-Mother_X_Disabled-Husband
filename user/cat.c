#include <lib.h>

char buf[8192];

int cat(int f, char *s) {
    long n;
    int r;

    while ((n = read(f, buf, (long)sizeof buf)) > 0) {
        if ((r = write(1, buf, n)) != n) {
            printf("write error copying %s: %d", s, r);
            return 1;
        }
    }
    if (n < 0) {
        printf("error reading %s: %d", s, n);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    int f, i;

    int res = 0;
    if (argc == 1) {
        cat(0, "<stdin>");
    } else {
        for (i = 1; i < argc; i++) {
            f = open(argv[i], O_RDONLY);
            if (f < 0) {
                printf("can't open %s: %d", argv[i], f);
                return 1;
            } else {
                res += cat(f, argv[i]);
                close(f);
            }
        }
    }
    return res;
}
