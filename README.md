### 1\. 相对路径

  - 在内核态中开一个大数组，存储每个进程对应的当前路径

```c
char curpath[NENV][MAXPATHLEN] = {0};
struct Var var[NENV][MAXVARNUM];
```

  - 新增系统调用，读写当前进程的当前路径

```c
void sys_get_curpath(char *s) { strcpy(s, curpath[curenv->env_id >> 11]); }

void sys_set_curpath(char *s) { strcpy(curpath[curenv->env_id >> 11], s); }
```

  - 在进程初始化时，给进程的当前路径设置初值

```c
void env_init(void) {
    int i;
    /* Step 1: Initialize 'env_free_list' with 'envs[0]' ~ 'envs[NENV-1]'. */
    LIST_INIT(&env_free_list);
    TAILQ_INIT(&env_sched_list);
    for (i = NENV - 1; i >= 0; --i) {
        envs[i].env_status = ENV_FREE;
        LIST_INSERT_HEAD(&env_free_list, &envs[i], env_link);
        strcpy(curpath[i], "/");
    }
```

  - 新创建进程时，继承其父进程的相对路径

```c
    e->env_id = mkenvid(e);
    e->env_parent_id = parent_id;
    u_int new_index = e->env_id >> 11;
    u_int parent_index = parent_id >> 11;
    strcpy(curpath[new_index], curpath[parent_index]);
    for (int i = 0; i < MAXVARNUM; i++) {
        struct Var temp = var[parent_index][i];
        if (temp.valid == 1 && temp.x == 1) {
            set_env_var(e->env_id, temp.key, temp.value, temp.r, temp.x);
        }
    }
```

  - 新增一个从相对路径到绝对路径的转换函数

```c
void tackle_path(const char *in, char *out) {
    char temp[MAXPATHLEN] = {0};
    char cur[MAXPATHLEN] = {0};
    strcpy(temp, in);
    int len = strlen(temp);
    syscall_get_curpath(cur);
    int curlen = strlen(cur);
    if (len > 1 && temp[len - 1] == '/') {
        temp[len - 1] = '\0';
        len--;
    }
    if (len > 2 && temp[len - 1] == 'b' && temp[len - 2] == '.') {
        strcpy(out, temp);
    } else if (temp[0] == '/') {
        strcpy(out, temp);
    } else if (temp[0] == '.' && temp[1] == '.') {
        int slash = -1;
        for (int i = curlen - 1; i >= 0; i--) {
            if (cur[i] == '/') {
                slash = i;
                break;
            }
        }
        int i = 0, pos = 0;
        if (slash <= 0) {
            out[pos++] = '/';
        } else {
            for (; i < slash; i++) {
                out[pos++] = cur[i];
            }
        }
        if (out[pos - 1] != '/') {
            out[pos++] = '/';
        }
        for (i = 3; temp[i] == '.' || temp[i] == '/'; i++)
            ;
        for (; temp[i]; i++) {
            out[pos++] = temp[i];
        }
        if (out[pos - 1] == '/' && pos != 1) {
            out[pos - 1] = '\0';
        } else {
            out[pos] = '\0';
        }
    } else if (temp[0] == '.' && (temp[1] == '/' || temp[1] == '\0')) {
        int i = 0, pos = 0;
        for (; cur[i]; i++) {
            out[pos++] = cur[i];
        }
        if (out[pos - 1] != '/') {
            out[pos++] = '/';
        }
        for (i = 2; temp[i]; i++) {
            out[pos++] = temp[i];
        }
        if (out[pos - 1] == '/' && pos != 1) {
            out[pos - 1] = '\0';
        } else {
            out[pos] = '\0';
        }
    } else {
        int i = 0, pos = 0;
        for (i = 0; cur[i]; i++) {
            out[pos++] = cur[i];
        }
        if (out[pos - 1] != '/') {
            out[pos++] = '/';
        }
        for (i = 0; temp[i]; i++) {
            out[pos++] = temp[i];
        }
        if (out[pos - 1] == '/' && pos != 1) {
            out[pos - 1] = '\0';
        } else {
            out[pos] = '\0';
        }
    }
}
```

  - 在打开和删除文件的函数中，将传入的路径转换成绝对路径

```c
int open(const char *path, int mode) {
    int r;
    char out[MAXPATHLEN] = {0};
    tackle_path(path, out);
    // Step 1: Alloc a new 'Fd' using 'fd_alloc' in fd.c.
    // Hint: return the error code if failed.
    struct Fd *fd;
    if ((r = fd_alloc(&fd)) < 0) {
        return r;
    }

    // Step 2: Prepare the 'fd' using 'fsipc_open' in fsipc.c.
    r = fsipc_open((const char *)out, mode, fd);
    if (r) {
        return r;
    }
...
}

int remove(const char *path) {
    // Call fsipc_remove.
    char out[MAXPATHLEN] = {0};
    tackle_path(path, out);
    return fsipc_remove((const char *)out);
}
```

  - 实现内建指令的外部指令的分离

```c
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
...
}


void runcmd(char *s) {
    int inner = judge_inner(s);
    if (inner) {
        run_inner_cmd(s);
    } else {
        run_outer_cmd(s);
    }
}
```

  - 通过系统调用，实现内建指令cd和pwd

```c
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
```

### 2\. 新增指令

  - 创建目录

```c
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
```

  - 创建文件

```c
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
```

  - 删除文件

```c
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
```

### 3\. 环境变量

  - 在内核态中开一个大数组，存储每个进程对应的环境变量

```c
struct Var {
    char key[MAXVARLEN];
    char value[MAXVARLEN];
    int r;
    int x;
    int valid;
};
...
extern char curpath[NENV][MAXPATHLEN];
extern struct Var var[NENV][MAXVARNUM];
```

  - 新增系统调用，读写和打印当前进程的环境变量

```c
void set_env_var(u_int envid, char *key, char *value, int r, int x) {
    int index = envid >> 11;
    for (int i = 0; i < MAXVARNUM; i++) {
        if (strcmp(var[index][i].key, key) == 0 && var[index][i].valid == 1) {
            if (var[index][i].r == 0) {
                strcpy(var[index][i].value, value);
                var[index][i].r = r;
                var[index][i].x = x;
            }
            return;
        }
    }
    for (int i = 0; i < MAXVARNUM; i++) {
        if (var[index][i].valid == 0) {
            strcpy(var[index][i].key, key);
            strcpy(var[index][i].value, value);
            var[index][i].valid = 1;
            var[index][i].r = r;
            var[index][i].x = x;
            return;
        }
    }
}

void get_env_var(u_int envid, char *key, char *res) {
    int index = envid >> 11;
    for (int i = 0; i < MAXVARNUM; i++) {
        if (strcmp(var[index][i].key, key) == 0 && var[index][i].valid == 1) {
            strcpy(res, var[index][i].value);
            return;
        }
    }
}

void unset_env_var(u_int envid, char *key) {
    int index = envid >> 11;
    for (int i = 0; i < MAXVARNUM; i++) {
        if (strcmp(var[index][i].key, key) == 0 && var[index][i].valid == 1) {
            if (var[index][i].r == 0) {
                strcpy(var[index][i].key, "\0");
                strcpy(var[index][i].value, "\0");
                var[index][i].r = 0;
                var[index][i].x = 0;
                var[index][i].valid = 0;
            }
            return;
        }
    }
}

void print_env_var(u_int envid) {
    int index = envid >> 11;
    for (int i = 0; i < MAXVARNUM; i++) {
        if (var[index][i].valid == 1) {
            printk("%s=%s\n", var[index][i].key, var[index][i].value);
        }
    }
}
```

  - 新创建进程时，继承其父进程的全局环境变量‘

```c
int env_alloc(struct Env **new, u_int parent_id) {
...
    e->env_id = mkenvid(e);
    e->env_parent_id = parent_id;
    u_int new_index = e->env_id >> 11;
    u_int parent_index = parent_id >> 11;
    strcpy(curpath[new_index], curpath[parent_index]);
    for (int i = 0; i < MAXVARNUM; i++) {
        struct Var temp = var[parent_index][i];
        if (temp.valid == 1 && temp.x == 1) {
            set_env_var(e->env_id, temp.key, temp.value, temp.r, temp.x);
        }
    }
...
}
```

  - 在获取当前token的函数中，展开环境变量的引用

```c
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
```

  - 通过系统调用，实现内建指令declare和unset

```c
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
```

### 4\. 注释

  - 在指令输入后，将指令井号后部分截断后执行

```c
        if (buf[0] == '\0' || buf[0] == '#') {
            continue;
        }
        for (int i = 0; i < strlen(buf); i++) {
            if (buf[i] == '#') {
                buf[i] = '\0';
            }
        }
```

### 5\. 指令输入优化

  - 删除读取字符的不必要循环

```c
int sys_cgetc(void) {
    int ch = scancharc();
    return ch;
}
```

  - 实现指令的重新打印展示

```c
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
```

  - 修改读取键盘输入的函数

```c
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
```

### 6\. 历史指令

  - 在创建终端时，创建或加载已有历史指令文件

```c
    history_fd = open("/.mos_history", O_RDWR | O_CREAT);
    load_history();
```

  - 每次输入指令后，将新指令保存到文件中

```c
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
```

### 7\. 追加重定向

  - 解析追加重定向符号

```c
    if (*s == '>' && *(s + 1) == '>') {
        *p1 = s;
        *s++ = 0;
        *s++ = 0;
        *p2 = s;
        return 'a';
    }
```

  - 添加追加打开方式

```c
#define O_APPEND 0x0004  /* open for appending */
```

  - 追加模式打开时，修改文件偏移量

```c
    ff->f_fd.fd_omode = o->o_mode;
    ff->f_fd.fd_dev_id = devfile.dev_id;
    if (rq->req_omode & O_APPEND) {
        ff->f_fd.fd_offset = ff->f_file.f_size;
    }
```

  - 解析命令时，仿照普通重定向实现

```c
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
```

### 8\. 条件执行

  - 解析条件执行符号

```c
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
```

  - 将无返回值的外部命令改为有返回值

```c
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
```

  - 捕获main函数的返回值，并传递给父进程

```c
void libmain(int argc, char **argv) {
    // set file handles
    ...
    // set env to point at our env structure in envs[].
    env = &envs[ENVX(syscall_getenvid())];

    int res = main(argc, argv);
    u_int parent = syscall_get_parent();
    ipc_send(parent, res, 0, 0);

    // exit gracefully
    exit();
}
```

  - 运行外部指令时，接受返回值，并传递给父进程

```c
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
```

  - 解析命令时，右侧指令对应进程等待左侧指令的返回值消息，决定是否执行

```c
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
```

  - 实现内建exit指令

```c
    if ((strcmp(argv[0], "exit") == 0) || (strcmp(argv[0], "exit.b") == 0)) {
        int parent = syscall_get_parent();
        ipc_send(parent, 0, 0, 0);
        exit();
```

### 9\. 反引号

  - 对指令进行预处理，执行反引号内的指令，结果通过管道返回，进行字符串替换

```c
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
```

### 10\. 一行多指令

  - 递归创建新进程，执行右侧指令

```c
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
```