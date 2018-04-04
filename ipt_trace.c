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

#define DATA_PAGES 1024
#define AUX_PAGES 16

#define _HF_PERF_MAP_SZ (1024 * 512)
#define _HF_PERF_AUX_SZ (1024 * 1024)

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                int cpu, int group_fd, unsigned long flags)
{
    int ret;

    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
                   group_fd, flags);
    return ret;
}

int main(int argc, char **argv)
{
    struct perf_event_attr attr;
    struct perf_event_mmap_page *header;
    void *base, *data, *aux;
    int page_size = getpagesize();

    memset(&attr, 0, sizeof(attr));
    attr.size = sizeof(attr);
    attr.type = 6; // specific to this computer
    attr.size = sizeof(struct perf_event_attr);

    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.disabled = 1;
    //attr.precise_ip = 3;

    int pid = getpid();
    int perf_fd = syscall(SYS_perf_event_open, &attr, pid, -1, -1, 0);
    if (perf_fd == -1){
        err(EXIT_FAILURE, "syscall");
    }

    base = mmap(NULL, _HF_PERF_MAP_SZ + page_size, PROT_WRITE, MAP_SHARED, perf_fd, 0);
    if (base == MAP_FAILED) {
        err(EXIT_FAILURE, "mmap");
    }

    header = base;
    data = base + header->data_offset;
    header->aux_offset = header->data_offset + header->data_size;
    header->aux_size = _HF_PERF_AUX_SZ;

    aux = mmap(NULL, header->aux_size, PROT_READ | PROT_WRITE, MAP_SHARED, perf_fd, header->aux_offset);
    if (aux == MAP_FAILED) {
        err(EXIT_FAILURE, "mmap");
    }

    char *args[2];

    // sample program to trace
    args[0] = "/bin/ls > /dev/null";        
    args[1] = NULL;            

    ioctl(perf_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(perf_fd, PERF_EVENT_IOC_ENABLE, 0);

    execv( args[0], args );

    ioctl(perf_fd, PERF_EVENT_IOC_DISABLE, 0);

    printf("Done tracing \n");

    close(perf_fd);

    __u64 written = 0;
    ssize_t res;
    int out_fd = open("trace.txt", O_WRONLY | O_CREAT | O_TRUNC);

    while (written < header->aux_size) {
        res = write(out_fd, aux, header->aux_size);
        written += res;
    }

    close(perf_fd);

}
