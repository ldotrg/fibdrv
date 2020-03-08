#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci_dev"
#define MAX_LENGTH 100
#define MAX_DIGITS 1000
#define BILLION 1000000000L
#define MILLION 1000000L

static long diff_in_ns(struct timespec t1, struct timespec t2)
{
    struct timespec diff;
    if (t2.tv_nsec - t1.tv_nsec < 0) {
        diff.tv_sec = t2.tv_sec - t1.tv_sec - 1;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec + BILLION;
    } else {
        diff.tv_sec = t2.tv_sec - t1.tv_sec;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec;
    }
    return (diff.tv_sec * BILLION + diff.tv_nsec);
}

int main()
{
    long long sz;

    char buf[MAX_DIGITS] = "";
    char write_buf[] = "testing writing";
    int offset = MAX_LENGTH;
    /* Performance util*/
    struct timespec start, stop;
    FILE *output_text = fopen("./performance.csv", "w");

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "fd = %d\n", fd);
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC, &start);
        sz = read(fd, buf, MAX_DIGITS);
        clock_gettime(CLOCK_MONOTONIC, &stop);
        double ns = diff_in_ns(start, stop);
        fprintf(output_text, "%d, %f\n", i, ns);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }
    fclose(output_text);

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, MAX_DIGITS);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }
    close(fd);
    return 0;
}
