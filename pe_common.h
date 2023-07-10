#ifndef PE_COMMON_H
#define PE_COMMON_H

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE 
#define BUFFER_SIZE 128

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define FIFO_EXCHANGE "/tmp/pe_exchange_%d"
#define FIFO_TRADER "/tmp/pe_trader_%d"
#define FEE_PERCENTAGE 1

struct trader{
    long trader_id;
    pid_t pid;
    char* exch_fifo;
    char* trad_fifo;
    int exch_fd;
    int trader_fd;
    int connected;
    int not_printed;
    int* products;
    long* money_exch;
    int order_id;
};




#endif
