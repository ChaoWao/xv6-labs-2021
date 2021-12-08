#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int rc;
    int p0[2];
    int p1[2];
    char byte;

    if (pipe(p0) < 0) {
        fprintf(2, "pingpang: create pipe error\n");
    }
    if (pipe(p1) < 0) {
        fprintf(2, "pingpang: create pipe error\n");
    }
    if ((rc = fork()) == 0) {
        byte = 'c';
        close(p0[1]);
        read(p0[0], &byte, 1);
        close(p0[0]);
        printf("%d: received ping\n", getpid());
        // printf("%c\n", byte);
        close(p1[0]);
        write(p1[1], &byte, 1);
        close(p1[1]);
    } else if (rc > 0) {
        byte = 'w';
        close(p0[0]);
        write(p0[1], &byte, 1);
        close(p0[1]);
        byte = 'c';
        close(p1[1]);
        read(p1[0], &byte, 1);
        close(p1[0]);
        printf("%d: received pong\n", getpid());
        // printf("%c\n", byte);
    } else {
        fprintf(2, "pingpang: fork error\n");
    }
    exit(0);
}

