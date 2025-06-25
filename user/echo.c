#include <lib.h>

int main(int argc, char **argv) {
    int i, n = 0;

    ARGBEGIN {
    case 'n':
        n = 1;
        break;
    }
    ARGEND

    for (i = 0; i < argc; i++) {
        if (i > 0) {
            printf(" ");
        }
        printf("%s", argv[i]);
    }
    if (!n) {
        printf("\n");
    }
    return 0;
}
