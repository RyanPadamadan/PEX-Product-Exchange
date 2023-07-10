#include "pe_trader.h"


volatile sig_atomic_t interrupted = 0;
char inp[64];
int get_price(char* string);

void sigint_handler(int sig) {
    interrupted = 1;
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }

    char fifo_exch[128];
    char fifo_trad[128];
    snprintf(fifo_exch, 128, FIFO_EXCHANGE, atoi(argv[1]));
    snprintf(fifo_trad, 128, FIFO_TRADER, atoi(argv[1]));

    struct sigaction sig = {
        .sa_handler = sigint_handler,
        .sa_flags = SA_SIGINFO
    };


    
    sigaction(SIGUSR1, &sig, NULL);

    int fd1 = open(fifo_exch, O_RDONLY);
    int fd2 = open(fifo_trad, O_WRONLY);
    
    int i = 0;
    
    while(1){
        pause();
        if(interrupted == 1){
            if(read(fd1, inp, 64) == -1){
                printf("something wrong\n");
                return 1;
            }

            if (i == 0){
                i += 1;
                interrupted = 0;
                continue;
            }

            char *pos = strtok(inp, " ");
            char* operation = strtok(NULL, " ");

            if (strcmp(operation, "SELL") == 0 && strcmp(pos, "MARKET") == 0){

                char* item = strtok(NULL, " ");
                int quantity = atoi(strtok(NULL, " "));
                int price = get_price(strtok(NULL, " "));

                if(quantity >= 1000){
                    close(fd1);

                    close(fd2);
                    break;
                }

                char* buy_op;
                if(0 > asprintf(&buy_op, "BUY %d %s %d %d;", i-1, item,  quantity, price)){ 
                    printf("Something is wrong\n");
                    close(fd1);
                    close(fd2);
                    return 2;
                }

                write(fd2,buy_op,strlen(buy_op));                
                free(buy_op);
                interrupted = 0;
                
                while(interrupted == 0){
                    kill(getppid(), SIGUSR1);
                    sleep(2);
                }

                if(interrupted == 1){
                    if(read(fd1, inp, 64) == -1){
                    return 1;
                    }
                    interrupted = 0;
                }

                i++;
            }

        }

    }

    close(fd1);
    close(fd2);
    unlink(fifo_exch);
    unlink(fifo_trad);
    return 0;
    
}


int get_price(char* string){
    char* temp = string;
    while(*temp != ';'){
        temp++;
    }

    *temp = '\0';
    return atoi(string);
}