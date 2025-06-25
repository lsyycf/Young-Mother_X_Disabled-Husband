#include <args.h>
#include <lib.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"
#define CHAR "0123456789_qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM"

void runcmd(char *s);
char history_lines[MAXHISTORY + 1][MAXPATHLEN + 1];
char current_input[MAXPATHLEN + 1];
char buf[MAXPATHLEN];
int history_fd = -1;
int history_index = 0;
int history_count = 0;
int history_current = -1;
int conditional = 0;

/* Overview:
 *   Parse the next token from the string at s.
 *
 * Post-Condition:
 *   Set '*p1' to the beginning of the token and '*p2' to just past the token.
 *   Return:
 *     - 0 if the end of string is reached.
 *     - '<' for < (stdin redirection).
 *     - '>' for > (stdout redirection).
 *     - '|' for | (pipe).
 *     - 'w' for a word (command, argument, or file name).
 *
 *   The buffer is modified to turn the spaces after words into zero bytes
 * ('\0'), so that the returned token is a null-terminated string.
 */
int _gettoken(char *s, char **p1, char **p2) {
    *p1 = 0;
    *p2 = 0;
    if (s == 0) {
        return 0;
    }

    while (strchr(WHITESPACE, *s)) {
        *s++ = 0;
    }
    if (*s == 0) {
        return 0;
    }

    if (*s == '$') {
        char key[MAXVARLEN];
        char value[MAXVARLEN];
        char after[1024];
        int i;
        for (i = 1; s[i] && strchr(CHAR, s[i]); i++) {
            key[i - 1] = s[i];
        }
        key[i - 1] = '\0';
        strcpy(after, s + i);
        syscall_get_var(key, value);
        strcpy(s, value);
        strcpy(s + strlen(value), after);
        *p1 = s;
        s += strlen(value);
        while (s[0] && !strchr(WHITESPACE, s[0])) {
            s++;
        }
        *p2 = s;
        return 'w';
    }

    if (*s == '>' && *(s + 1) == '>') {
        *p1 = s;
        *s++ = 0;
        *s++ = 0;
        *p2 = s;
        return 'a';
    }

    if (*s == '|' && *(s + 1) == '|') {
        *p1 = s;
        *s++ = 0;
        *s++ = 0;
        *p2 = s;
        return 'O';
    }

    if (*s == '&' && *(s + 1) == '&') {
        *p1 = s;
        *s++ = 0;
        *s++ = 0;
        *p2 = s;
        return 'A';
    }

    if (strchr(SYMBOLS, *s)) {
        int t = *s;
        *p1 = s;
        *s++ = 0;
        *p2 = s;
        return t;
    }

    *p1 = s;
    while (*s && !strchr(WHITESPACE SYMBOLS, *s)) {
        s++;
    }
    *p2 = s;
    return 'w';
}

int gettoken(char *s, char **p1) {
    static int c, nc;
    static char *np1, *np2;

    if (s) {
        nc = _gettoken(s, &np1, &np2);
        return 0;
    }
    c = nc;
    *p1 = np1;
    nc = _gettoken(np2, &np1, &np2);
    return c;
}

#define MAXARGS 128

int parsecmd(char **argv, int *rightpipe) {
    int argc = 0;
    while (1) {
        char *t;
        int fd, r;
        int fork_id;
        conditional = 0;
        int c = gettoken(0, &t);
        switch (c) {
        case 0:
            return argc;
        case 'w':
            if (argc >= MAXARGS) {
                debugf("too many arguments\n");
                exit();
            }
            argv[argc++] = t;
            break;
        case '<':
            if (gettoken(0, &t) != 'w') {
                debugf("syntax error: < not followed by word\n");
                exit();
            }
            // Open 't' for reading, dup it onto fd 0, and then close the
            // original fd. If the 'open' function encounters an error, utilize
            // 'debugf' to print relevant messages, and subsequently terminate
            // the process using 'exit'.
            /* Exercise 6.5: Your code here. (1/3) */
            fd = open(t, O_RDONLY);
            if (fd < 0) {
                debugf("failed to open '%s'\n", t);
                exit();
            }
            dup(fd, 0);
            close(fd);
            // user_panic("< redirection not implemented");

            break;
        case '>':
            if (gettoken(0, &t) != 'w') {
                debugf("syntax error: > not followed by word\n");
                exit();
            }
            // Open 't' for writing, create it if not exist and trunc it if
            // exist, dup it onto fd 1, and then close the original fd. If the
            // 'open' function encounters an error, utilize 'debugf' to print
            // relevant messages, and subsequently terminate the process using
            // 'exit'.
            /* Exercise 6.5: Your code here. (2/3) */
            fd = open(t, O_WRONLY | O_CREAT | O_TRUNC);
            if (fd < 0) {
                debugf("failed to open '%s'\n", t);
                exit();
            }
            dup(fd, 1);
            close(fd);
            // user_panic("> redirection not implemented");

            break;
        case '|':;
            /*
             * First, allocate a pipe.
             * Then fork, set '*rightpipe' to the returned child envid or zero.
             * The child runs the right side of the pipe:
             * - dup the read end of the pipe onto 0
             * - close the read end of the pipe
             * - close the write end of the pipe
             * - and 'return parsecmd(argv, rightpipe)' again, to parse the rest
             * of the command line. The parent runs the left side of the pipe:
             * - dup the write end of the pipe onto 1
             * - close the write end of the pipe
             * - close the read end of the pipe
             * - and 'return argc', to execute the left of the pipeline.
             */
            int p[2];
            /* Exercise 6.5: Your code here. (3/3) */
            r = pipe(p);
            if (r != 0) {
                debugf("pipe: %d\n", r);
                exit();
            }
            r = fork();
            if (r < 0) {
                debugf("fork: %d\n", r);
                exit();
            }
            *rightpipe = r;
            if (r == 0) {
                dup(p[0], 0);
                close(p[0]);
                close(p[1]);
                return parsecmd(argv, rightpipe);
            } else {
                dup(p[1], 1);
                close(p[1]);
                close(p[0]);
                return argc;
            }
            // user_panic("| not implemented");

            break;
        case ';':
            fork_id = fork();
            if (fork_id < 0) {
                exit();
            } else if (fork_id == 0) {
                return argc;
            } else {
                wait(fork_id);
                return parsecmd(argv, rightpipe);
            }

            break;
        case 'a':
            if (gettoken(0, &t) != 'w') {
                exit();
            }
            if ((fd = open(t, O_RDONLY)) < 0) {
                fd = open(t, O_CREAT);
                if (fd < 0) {
                    exit();
                }
            }
            close(fd);
            if ((fd = open(t, O_WRONLY | O_APPEND)) < 0) {
                exit();
            }
            if ((r = dup(fd, 1)) < 0) {
                exit();
            }
            close(fd);

            break;
        case 'O':;
            fork_id = fork();
            if (fork_id < 0) {
                debugf("failed to fork in sh.c\n");
                exit();
            } else if (fork_id == 0) {
                conditional = 1;
                return argc;
            } else {
                u_int caller;
                int res = ipc_recv(&caller, 0, 0);
                if (res != 0) {
                    return parsecmd(argv, rightpipe);
                } else {
                    return 0;
                }
            }

            break;
        case 'A':;
            fork_id = fork();
            if (fork_id < 0) {
                debugf("failed to fork in sh.c\n");
                exit();
            } else if (fork_id == 0) {
                conditional = 1;
                return argc;
            } else {
                u_int caller;
                int res = ipc_recv(&caller, 0, 0);
                if (res == 0) {
                    return parsecmd(argv, rightpipe);
                } else {
                    return 0;
                }
            }

            break;
        }
    }

    return argc;
}

int judge_inner(char *s) {
    char argv[MAXPATHLEN];
    int i = 0, pos = 0;
    for (; s[i] && strchr(WHITESPACE, s[i]); i++)
        ;
    for (; s[i] && !strchr(WHITESPACE, s[i]); i++) {
        argv[pos++] = s[i];
    }
    argv[pos] = '\0';
    if ((strcmp(argv, "exit") == 0) || (strcmp(argv, "exit.b") == 0)) {
        return 1;
    } else if ((strcmp(argv, "history") == 0) ||
               (strcmp(argv, "history.b") == 0)) {
        return 1;
    } else if ((strcmp(argv, "cd") == 0) || (strcmp(argv, "cd.b") == 0)) {
        return 1;
    } else if ((strcmp(argv, "pwd") == 0) || (strcmp(argv, "pwd.b") == 0)) {
        return 1;
    } else if ((strcmp(argv, "declare") == 0) ||
               (strcmp(argv, "declare.b") == 0)) {
        return 1;
    } else if ((strcmp(argv, "unset") == 0) || (strcmp(argv, "unset.b") == 0)) {
        return 1;
    }
    return 0;
}

void run_outer_cmd(char *s) {
    int fork_id = fork();
    if (fork_id < 0) {
        debugf("fork error: %d\n", fork_id);
        return;
    }
    if (fork_id == 0) {
        gettoken(s, 0);

        char *argv[MAXARGS];
        int rightpipe = 0;
        int argc = parsecmd(argv, &rightpipe);
        if (argc == 0) {
            return;
        }
        argv[argc] = 0;
        int child = spawn(argv[0], argv);
        if (child >= 0) {
            u_int caller;
            int res = ipc_recv(&caller, 0, 0);
            if (conditional) {
                u_int parent = syscall_get_parent();
                ipc_send(parent, res, 0, 0);
            }
            close_all();
            wait(child);
        } else {
            debugf("spawn %s: %d\n", argv[0], child);
        }
        if (rightpipe) {
            wait(rightpipe);
        }
        exit();
    } else {
        wait(fork_id);
    }
    return;
}

void run_inner_cmd(char *s) {
    gettoken(s, 0);

    char *argv[MAXARGS];
    int rightpipe = 0;
    int argc = parsecmd(argv, &rightpipe);
    if (argc == 0) {
        return;
    }
    argv[argc] = 0;
    char buffer[MAXPATHLEN + 1] = {0};

    int fd, n;
    if ((strcmp(argv[0], "exit") == 0) || (strcmp(argv[0], "exit.b") == 0)) {
        int parent = syscall_get_parent();
        ipc_send(parent, 0, 0, 0);
        exit();
    } else if ((strcmp(argv[0], "history") == 0) ||
               (strcmp(argv[0], "history.b") == 0)) {
        if ((fd = open("/.mos_history", O_RDONLY)) < 0) {
            exit();
        }
        while ((n = read(fd, buffer, MAXPATHLEN)) > 0) {
            buffer[n] = '\0';
            printf("%s", buffer);
        }
        close(fd);
    } else if ((strcmp(argv[0], "cd") == 0) || (strcmp(argv[0], "cd.b") == 0)) {
        if (argc == 1) {
            strcpy(buffer, "/");
        } else if (argc > 2) {
            printf("Too many args for cd command\n");
            return;
        } else {
            struct Stat st;
            tackle_path((const char *)argv[1], buffer);
            if ((fd = open(buffer, O_RDONLY)) < 0) {
                printf("cd: The directory '%s' does not exist\n", argv[1]);
                return;
            }
            close(fd);
            stat(buffer, &st);
            if (!st.st_isdir) {
                printf("cd: '%s' is not a directory\n", argv[1]);
                return;
            }
        }
        syscall_set_curpath(buffer);
    } else if ((strcmp(argv[0], "pwd") == 0) ||
               (strcmp(argv[0], "pwd.b") == 0)) {
        if (argc > 1) {
            printf("pwd: expected 0 arguments; got %d\n", argc - 1);
            return;
        }
        syscall_get_curpath(buffer);
        printf("%s\n", buffer);
    } else if ((strcmp(argv[0], "declare") == 0) ||
               (strcmp(argv[0], "declare.b") == 0)) {
        int r = 0, x = 0;
        char value[MAXVARLEN] = {0};
        char key[MAXVARLEN] = {0};
        if (argc == 1) {
            syscall_print_var();
            return;
        } else if (argc > 3) {
            return;
        } else if (argc == 3) {
            if (strcmp(argv[1], "-r") == 0) {
                r = 1;
            } else if (strcmp(argv[1], "-x") == 0) {
                x = 1;
            } else if ((strcmp(argv[1], "-xr") == 0) ||
                       (strcmp(argv[1], "-rx") == 0)) {
                r = 1, x = 1;
            }
        }
        int i = 0, pos = 0;
        for (; argv[argc - 1][i] && argv[argc - 1][i] != '='; i++) {
            key[pos++] = argv[argc - 1][i];
        }
        key[pos] = '\0';
        pos = 0;
        i++;
        for (; argv[argc - 1][i]; i++) {
            value[pos++] = argv[argc - 1][i];
        }
        syscall_set_var(key, value, r, x);
    } else if ((strcmp(argv[0], "unset") == 0) ||
               (strcmp(argv[0], "unset.b") == 0)) {
        if (argc != 2) {
            return;
        }
        syscall_unset_var(argv[1]);
    }
    return;
}

void runcmd(char *s) {
    int inner = judge_inner(s);
    if (inner) {
        run_inner_cmd(s);
    } else {
        run_outer_cmd(s);
    }
}

void redisplay_line(char *buf, int len, int cursor) {
    printf("\r$ ");
    for (int i = 0; i < len; i++) {
        printf("%c", buf[i]);
    }
    printf("\033[K");
    if (cursor < len) {
        printf("\r$ ");
        for (int i = 0; i < cursor; i++) {
            printf("%c", buf[i]);
        }
    }
}

int run_quote_cmd(char *cmd, char *output) {
    int p[2];
    if (pipe(p) < 0) {
        return -1;
    }
    int child = fork();
    if (child < 0) {
        close(p[0]);
        close(p[1]);
        return -1;
    } else if (child == 0) {
        close(p[0]);
        dup(p[1], 1);
        close(p[1]);
        runcmd(cmd);
        exit();
    } else {
        close(p[1]);
        int total = 0;
        char buffer[1024];
        int r;
        while ((r = read(p[0], buffer, sizeof(buffer))) > 0) {
            if (r > 0) {
                memcpy(output + total, buffer, r);
                total += r;
            }
        }
        close(p[0]);
        wait(child);
        output[total] = '\0';
        while (total > 0 &&
               (output[total - 1] == '\n' || output[total - 1] == '\r')) {
            output[--total] = '\0';
        }
        return total;
    }
}

void tackle_quote(char *line) {
    char result[1024] = "";
    char *dest = result;
    char *src = line;
    while (*src) {
        if (*src == '`') {
            src++;
            char *start = src;
            while (*src && *src != '`') {
                src++;
            }
            if (*src == '`') {
                int len = src - start;
                char cmd[MAXPATHLEN];
                strcpy(cmd, start);
                cmd[len] = '\0';
                char output[MAXPATHLEN];
                if (run_quote_cmd(cmd, output) >= 0) {
                    for (char *p = output; *p; p++) {
                        if (*p == '\n' || *p == '\r') {
                            if (dest - result < sizeof(result) - 1) {
                                *dest++ = ' ';
                            }
                        } else {
                            if (dest - result < sizeof(result) - 1) {
                                *dest++ = *p;
                            }
                        }
                    }
                }
                src++;
            } else {
                if (dest - result < sizeof(result) - 1) {
                    *dest++ = '`';
                }
                src = start;
            }
        } else {
            if (dest - result < sizeof(result) - 1) {
                *dest++ = *src;
            }
            src++;
        }
    }
    *dest = '\0';
    strcpy(line, result);
}

void readline(char *buf, u_int n) {
    int r;
    int pos = 0;
    int len = 0;
    memset(buf, 0, n);
    history_current = -1;
    memset(current_input, 0, sizeof(current_input));
    while (1) {
        char c;
        if ((r = read(0, &c, 1)) != 1) {
            if (r < 0) {
                debugf("read error: %d\n", r);
            }
            exit();
        }
        if (c == '\r' || c == '\n') {
            buf[len] = '\0';
            return;
        } else if (c == '\b' || c == 0x7f) {
            if (pos > 0) {
                for (int i = pos - 1; i < len - 1; i++) {
                    buf[i] = buf[i + 1];
                }
                pos--;
                len--;
                buf[len] = '\0';
                redisplay_line(buf, len, pos);
            }
        } else if (c == 1) {
            pos = 0;
            printf("\r$ ");
        } else if (c == 5) {
            pos = len;
            printf("\r$ ");
            for (int i = 0; i < len; i++) {
                printf("%c", buf[i]);
            }
        } else if (c == 11) {
            len = pos;
            buf[len] = '\0';
            redisplay_line(buf, len, pos);
        } else if (c == 21) {
            if (pos > 0) {
                for (int i = 0; i < len - pos; i++) {
                    buf[i] = buf[pos + i];
                }
                len = len - pos;
                pos = 0;
                buf[len] = '\0';
                redisplay_line(buf, len, pos);
            }
        } else if (c == 23) {
            if (pos > 0) {
                int old_pos = pos;
                while (pos > 0 &&
                       (buf[pos - 1] == ' ' || buf[pos - 1] == '\t')) {
                    pos--;
                }
                while (pos > 0 && buf[pos - 1] != ' ' && buf[pos - 1] != '\t') {
                    pos--;
                }
                int delete = old_pos - pos;
                for (int i = pos; i < len - delete; i++) {
                    buf[i] = buf[i + delete];
                }
                len -= delete;
                buf[len] = '\0';
                redisplay_line(buf, len, pos);
            }
        } else if (c == 27) {
            char seq[3];
            if (read(0, &seq[0], 1) == 1 && seq[0] == '[') {
                if (read(0, &seq[1], 1) == 1) {
                    if (seq[1] == 'A') {
                        if (history_count > 0) {
                            if (history_current == -1) {
                                strcpy(current_input, buf);
                                history_current = history_count - 1;
                            } else if (history_current > 0) {
                                history_current--;
                            }
                            strcpy(buf, history_lines[history_current]);
                            len = strlen(buf);
                            pos = len;
                            printf("\r$ ");
                            for (int i = 0; i < len; i++) {
                                printf("%c", buf[i]);
                            }
                            printf("\033[K");
                        }
                    } else if (seq[1] == 'B') {
                        if (history_current != -1) {
                            if (history_current < history_count - 1) {
                                history_current++;
                                strcpy(buf, history_lines[history_current]);
                            } else {
                                history_current = -1;
                                strcpy(buf, current_input);
                            }
                            len = strlen(buf);
                            pos = len;
                            printf("\r$ ");
                            for (int i = 0; i < len; i++) {
                                printf("%c", buf[i]);
                            }
                            printf("\033[K");
                        }
                    } else if (seq[1] == 'C') {
                        if (pos < len) {
                            pos++;
                            printf("\033[C");
                        }
                    } else if (seq[1] == 'D') {
                        if (pos > 0) {
                            pos--;
                            printf("\033[D");
                        }
                    }
                }
            }
        } else if (c >= 32 && c <= 126) {
            if (len < n - 1) {
                for (int i = len; i > pos; i--) {
                    buf[i] = buf[i - 1];
                }
                buf[pos] = c;
                pos++;
                len++;
                buf[len] = '\0';
                redisplay_line(buf, len, pos);
            }
        }
    }
}

void load_history(void) {
    if (history_fd < 0) {
        return;
    }
    seek(history_fd, 0);
    char buffer[4096];
    int r = read(history_fd, buffer, sizeof(buffer) - 1);
    if (r <= 0) {
        return;
    }
    buffer[r] = '\0';
    history_count = 0;
    char *start = buffer;
    char *end;
    while (history_count < MAXHISTORY && start < buffer + r) {
        end = (char *)strchr(start, '\n');
        if (end) {
            *end = '\0';
        } else {
            end = buffer + r;
        }
        if (strlen(start) > 0) {
            strcpy(history_lines[history_count], start);
            history_lines[history_count]
                         [sizeof(history_lines[history_count]) - 1] = '\0';
            history_count++;
        }
        if (end < buffer + r) {
            start = end + 1;
        } else {
            break;
        }
    }
}

void save_history(void) {
    if (history_fd < 0) {
        return;
    }
    close(history_fd);
    history_fd = open("/.mos_history", O_WRONLY | O_CREAT | O_TRUNC);
    if (history_fd < 0) {
        return;
    }
    for (int i = 0; i < history_count; i++) {
        write(history_fd, history_lines[i], strlen(history_lines[i]));
        write(history_fd, "\n", 1);
    }
}

void store_history(char *buf) {
    if (strlen(buf) == 0) {
        return;
    }
    int len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
        len--;
    }
    if (len == 0) {
        return;
    }
    if (history_count > 0 &&
        strcmp(buf, history_lines[history_count - 1]) == 0) {
        return;
    }
    if (history_count >= MAXHISTORY) {
        for (int i = 0; i < MAXHISTORY - 1; i++) {
            strcpy(history_lines[i], history_lines[i + 1]);
        }
        history_count = MAXHISTORY - 1;
    }
    strcpy(history_lines[history_count], buf);
    history_count++;
    save_history();
}

void usage(void) {
    printf("usage: sh [-ix] [script-file]\n");
    exit();
}

int main(int argc, char **argv) {
    int r;
    int interactive = iscons(0);
    int echocmds = 0;
    printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
    printf("::                                                         ::\n");
    printf("::                     MOS Shell 2024                      ::\n");
    printf("::                                                         ::\n");
    printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
    ARGBEGIN {
    case 'i':
        interactive = 1;
        break;
    case 'x':
        echocmds = 1;
        break;
    default:
        usage();
    }
    ARGEND

    if (argc > 1) {
        usage();
    }
    if (argc == 1) {
        close(0);
        if ((r = open(argv[0], O_RDONLY)) < 0) {
            user_panic("open %s: %d", argv[0], r);
        }
        user_assert(r == 0);
    }
    history_fd = open("/.mos_history", O_RDWR | O_CREAT);
    load_history();
    for (;;) {
        if (interactive) {
            printf("\n$ ");
        }
        readline(buf, sizeof buf);

        if (strlen(buf) > 0) {
            store_history(buf);
        }
        if (buf[0] == '\0' || buf[0] == '#') {
            continue;
        }
        for (int i = 0; i < strlen(buf); i++) {
            if (buf[i] == '#') {
                buf[i] = '\0';
            }
        }
        if (echocmds) {
            printf("# %s\n", buf);
        }
        tackle_quote(buf);
        runcmd(buf);
    }
    return 0;
}
