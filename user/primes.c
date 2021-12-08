#include "kernel/types.h"
#include "user/user.h"

int child(int fd) {
    int p[2];
    int rc;

    int first;
    if ((rc = read(fd, &first, 4)) == 0) {
        exit(-1);
    }
    printf("prime %d\n", first);
    
    if (pipe(p) < 0) {
        fprintf(2, "primes: create pipe error\n");
        exit(-1);
    }
    if ((rc = fork()) == 0) {
        close(p[1]);
        child(p[0]);
    } else if (rc > 0) {
        close(p[0]);
        int next;
        while ((rc = read(fd, &next, 4)) != 0) {
            if (next % first != 0) {
                write(p[1], &next, 4);
            }
        }
        close(fd);
        close(p[1]);
        wait((int *) 0);
    } else {
        fprintf(2, "primes: fork error\n");
    }
    exit(0);
}

int main(int argc, char *argv[])
{
    int rc;
    int p[2];

    if (pipe(p) < 0) {
        fprintf(2, "primes: create pipe error\n");
        exit(-1);
    }
    if ((rc = fork()) == 0) {
        close(p[1]);
        child(p[0]);
    } else if (rc > 0) {
        int i;
        close(p[0]);
        for (i = 2; i <= 35; i++) {
            write(p[1], &i, 4);
        }
        close(p[1]);
        wait((int *) 0);
    } else {
        fprintf(2, "primes: fork error\n");
    }
    exit(0);
}

