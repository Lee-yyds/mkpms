#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

__attribute__((naked, aligned(4096))) int probe(int value)
{
    __asm__("add w0, w0, #1\nret");
}

static int run_client(const char *client, unsigned long addr,
                      const char *operation)
{
    char pid_arg[32], addr_arg[32];
    int status;
    snprintf(pid_arg, sizeof(pid_arg), "%d", getpid());
    snprintf(addr_arg, sizeof(addr_arg), "0x%lx", addr);

    pid_t child = fork();
    if (child == 0) {
        if (!strcmp(operation, "bp"))
            execl(client, client, "-p", pid_arg, "-a", addr_arg,
                  "-r", "x0=41", (char *)NULL);
        if (!strcmp(operation, "patch"))
            execl(client, client, "-p", pid_arg, "-a", addr_arg,
                  "--patch", "00140011", (char *)NULL);
        execl(client, client, "-p", pid_arg, "-a", addr_arg,
              "--release", (char *)NULL);
        perror("exec client");
        _exit(127);
    }
    if (child < 0 || waitpid(child, &status, 0) < 0) {
        perror("fork/waitpid");
        return -1;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

__attribute__((aligned(4096))) int main(int argc, char **argv)
{
    if (argc != 2 && !(argc == 3 && !strcmp(argv[2], "--disabled"))) {
        fprintf(stderr, "usage: %s <wxshadow_client> [--disabled]\n", argv[0]);
        return 1;
    }

    int (*volatile call)(int) = probe;
    unsigned long addr = (unsigned long)probe;
    uint32_t original = *(volatile uint32_t *)addr;
    int before = call(7);

    if (argc == 3) {
        int rejected = run_client(argv[1], addr, "bp") != 0;
        int after = call(7);
        printf("disabled: before=%d rejected=%d after=%d\n",
               before, rejected, after);
        return before == 8 && rejected && after == 8 ? 0 : 2;
    }

    if (run_client(argv[1], addr, "bp")) return 1;
    int hooked = call(7);
    uint32_t visible = *(volatile uint32_t *)addr;
    int hooked_after_read = call(7);
    if (run_client(argv[1], addr, "release")) return 1;
    int restored = call(7);

    if (run_client(argv[1], addr, "patch")) return 1;
    int patched = call(7);
    uint32_t visible_after_patch = *(volatile uint32_t *)addr;
    if (run_client(argv[1], addr, "release")) return 1;
    int restored_after_patch = call(7);

    printf("before=%d hooked=%d after_read=%d patched=%d restored=%d/%d read=%08x/%08x expected=%08x\n",
           before, hooked, hooked_after_read, patched, restored,
           restored_after_patch, visible, visible_after_patch, original);
    return before == 8 && hooked == 42 && hooked_after_read == 42 &&
                   patched == 12 && restored == 8 && restored_after_patch == 8 &&
                   visible == original && visible_after_patch == original ? 0 : 2;
}
