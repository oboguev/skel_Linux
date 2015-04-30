#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <asm-generic/ioctl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mytest.h"

static int open_device(int flags);

int
main(int argc, char **argv)
{
    int error = 0;
    const char* verb;
    int fd;

    verb = (argc <= 1) ? "" : argv[1];

    if (0 == strcmp(verb, "print") && argc == 3)
    {
        fd = open_device(O_WRONLY);
        if (ioctl(fd, IOC_MYTEST_PRINT, argv[2]))
        {
            error = errno;
            perror("ioctl");
        }
    }
    else if (0 == strcmp(verb, "panic") && argc == 3)
    {
        fd = open_device(O_WRONLY);
        sync(); sync(); sync();
        if (ioctl(fd, IOC_MYTEST_PANIC, argv[2]))
        {
            error = errno;
            perror("ioctl");
        }
    }
    else if (0 == strcmp(verb, "oops") && argc == 2)
    {
        fd = open_device(O_WRONLY);
        sync(); sync(); sync();
        if (ioctl(fd, IOC_MYTEST_OOPS))
        {
            error = errno;
            perror("ioctl");
        }
    }
    else
    {
        printf("usage: test print string\n");
        printf("       test panic string\n");
        printf("       test oops\n");
        error = EINVAL;
    }

    return error;
}

static int 
open_device(int flags)
{
    int error;
    int fd = open("/dev/mytest", flags);
    if (fd < 0 && errno == ENOENT)
        fd = open("/dev/mytest0", flags);
    if (fd >= 0)  return fd;
    error = errno;
    perror("unable to open device");
    exit(error);
}
