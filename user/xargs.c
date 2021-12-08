#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

void print_before_ex(char *argv[])
{
    for (int i = 0; argv[i] != 0; i++)
    {
        printf("%s ", argv[i]);
    }
}

int main(int argc, char *argv[])
{
    char *childargv[MAXARG];
    char buf[512];
    int i = 0, j, k;
    char c;
    int rc;

    if (argc < 2)
    {
        fprintf(2, "xargs: missing operand\n");
        exit(-1);
    }

    for (; i < MAXARG && argv[i + 1] != 0; i++)
    {
        childargv[i] = argv[i + 1];
    }
    // childargv[i] = 0;
    // print_before_ex(childargv);
    // exit(-1);

    j = i;
    k = 0;
    while (read(0, &c, 1) != 0)
    {
        // line start
        if (c == ' ')
        {
        }
        else if (c == '\n')
        {
            childargv[j] = 0;
            print_before_ex(childargv);
            if ((rc = fork()) == 0)
            {
                exec(childargv[0], childargv);
            }
            else if (rc > 0)
            {
                wait((int *)0);
                j = i;
                k = 0;
                continue;
            }
            else
            {
                fprintf(2, "xargs: fork error\n");
            }
        }
        else
        {
            buf[k++] = c;
        }
        while (read(0, &c, 1) != 0)
        {
            if (c == ' ' || c == '\n')
            {
                if (k != 0)
                {
                    buf[k++] = '\0';
                    childargv[j] = (char *)malloc(strlen(buf));
                    memcpy(childargv[j], buf, strlen(buf));
                    k = 0;
                    j++;
                }
            }
            else
            {
                buf[k++] = c;
            }
            if (c == '\n')
            {
                if ((rc = fork()) == 0)
                {
                    exec(childargv[0], childargv);
                }
                else if (rc > 0)
                {
                    wait((int *)0);
                    j = i;
                    k = 0;
                    break;
                }
                else
                {
                    fprintf(2, "xargs: fork error\n");
                }
            }
        }
    }
    if (k != 0)
    {
        buf[k++] = '\0';
        childargv[j] = (char *)malloc(strlen(buf));
        memcpy(childargv[j], buf, strlen(buf));
        k = 0;
        j++;
    }
    if (j != i)
    {
        childargv[j] = 0;
        print_before_ex(childargv);
        if ((rc = fork()) == 0)
        {
            exec(childargv[0], childargv);
        }
        else if (rc > 0)
        {
            wait((int *)0);
        }
        else
        {
            fprintf(2, "xargs: fork error\n");
        }
    }
    exit(0);
}
