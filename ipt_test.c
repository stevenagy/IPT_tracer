#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <syscall.h>
#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>

#define DATA_PAGES 1024
#define AUX_PAGES 16

#define PERF_MAP_SZ (1024 * 512)
#define PERF_AUX_SZ (1024 * 1024)


int main(int argc, char **argv)
{
    long start, end;
    struct timeval timecheck;

    struct perf_event_attr attr;
    struct perf_event_mmap_page *header;
    void *base, *data, *aux;
    int page_size = getpagesize();

    memset(&attr, 0, sizeof(attr));

    attr.type = 6;
    attr.size = sizeof(attr);
    attr.disabled = 1;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    
    /*
    attr.sample_type = 0x10086;
    //attr.config = 0x300e601; 
    attr.sample_id_all = 1;
    attr.config = 0x9; //???
    attr.mmap = 1;
    attr.mmap2 = 1;
    attr.comm_exec = 1;
    attr.context_switch = 1;
    attr.task = 1;
    // these two below crash for some reason
    attr.inherit = 1;
    attr.read_format = 1;
    */

    // Set up parent, child pids; child will be the target program execution
    pid_t parent = getpid();
    pid_t child = fork();

    // Set up perf syscall
    int perf_fd = syscall(SYS_perf_event_open, &attr, child, -1, -1, 0);
    if (perf_fd == -1) {
        err(EXIT_FAILURE, "syscall");
    }

    //
    if (child == -1)
    {
        err(EXIT_FAILURE, "fork");
    } 
    else if (child > 0)
    {
        int status;
        waitpid(child, &status, 0);
    }
    else 
    {
        ioctl(perf_fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(perf_fd, PERF_EVENT_IOC_ENABLE, 0);
        int res = system("/mnt/bigdata/UnTracer-Fuzzing/benchmarks/libjpeg/jpeg-9a/djpeg /mnt/bigdata/UnTracer-Fuzzing/benchmarks/libjpeg/.cur_input");
        if (res != 0) {
            err(EXIT_FAILURE, "system");
        }
        ioctl(perf_fd, PERF_EVENT_IOC_DISABLE, 0);
    }   

    base = mmap(NULL, PERF_MAP_SZ + page_size, PROT_WRITE, MAP_SHARED, perf_fd, 0);
    if (base == MAP_FAILED) err(EXIT_FAILURE, "mmap");

    header = base;
    data = base + header->data_offset;
    header->aux_offset = header->data_offset + header->data_size;
    header->aux_size = PERF_AUX_SZ;

    aux = mmap(NULL, header->aux_size, PROT_READ, MAP_SHARED, perf_fd, header->aux_offset);
    if (aux == MAP_FAILED) err(EXIT_FAILURE, "mmap");

    //gettimeofday(&timecheck, NULL);
    //start = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;

    //gettimeofday(&timecheck, NULL);
    //end = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;

    //printf("Trace time: %.3f seconds\n", (float) (end - start)/1000);

    close(perf_fd);

    __u64 written = 0;
    ssize_t res;
    int out_fd = open("trace.txt", O_WRONLY | O_CREAT | O_TRUNC);

    while (written < header->aux_size) {
        res = write(out_fd, aux, header->aux_size);
        written += res;
    }

    close(perf_fd);
    munmap(aux, header->aux_size);
    munmap(base, PERF_MAP_SZ + page_size);
}
