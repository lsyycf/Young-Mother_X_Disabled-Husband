#include <env.h>
#include <lib.h>
#include <mmu.h>

void exit(void) {
    // After fs is ready (lab5), all our open files should be closed before
    // dying.
#if !defined(LAB) || LAB >= 5
    close_all();
#endif

    syscall_env_destroy(0);
    user_panic("unreachable code");
}

const volatile struct Env *env;
extern int main(int, char **);

void libmain(int argc, char **argv) {
    // set env to point at our env structure in envs[].
    env = &envs[ENVX(syscall_getenvid())];

    int res = main(argc, argv);
    u_int parent = syscall_get_parent();
    ipc_send(parent, res, 0, 0);

    // exit gracefully
    exit();
}
