/**
 * comp2017 - assignment 3
 * <your name>
 * <your unikey>
 */

#include "priority_queue.h"
#include "order_operations.h"

void message_all_traders(char* message);
void message_all_other_traders(int trader_id, char* message);
void begin_pex(int num_products, char **products);
void message_trader(int trader_id, char *message);
void call_disc(int disc_trader);
int get_trader_id(pid_t pid);
//variable that stores the last operation done,
//If it was a buy then value is 0
//if its a sell then value 1
//This is done to get the correct corresponding orders
//Product book is a linked list, for each product, and a priority queue stored in each

struct trader* traders;
struct product_book* product_book;
char **products;
long exchange_fee = 0;
int num_products;
int num_traders;

int most_rec_order = -1;
int most_rec_op = -1;
struct product_book* most_rec_log;
int qty_match;
int total_match;
int matched = 0;

volatile sig_atomic_t op_recieved = 0;       
volatile sig_atomic_t inactive_traders = 0;
volatile sig_atomic_t who_sent;


void sig_handler(int sig, siginfo_t *sinfo, void *context) {
	if (sig == SIGUSR1) {
        who_sent = get_trader_id(sinfo->si_pid);
		if(traders[who_sent].connected){
        	op_recieved = 1;  // set flag to true
        	usleep(1);
        	return;
		}
    }
    if (sig == SIGCHLD) {
		int disc_trad = get_trader_id(sinfo->si_pid);
        inactive_traders++;  // increment to count traders disconnected
		traders[disc_trad].connected = 0;
		char output[BUFFER_SIZE];
		int slength = sprintf(output, "[PEX] Trader %d disconnected\n",disc_trad);
		write(1, output, slength);
        usleep(1);
        return;
    }
}

int main(int argc, char **argv)
{
	FILE *fptr;
	num_traders = argc - 2;
	char *file_name = argv[1];
	if (!(fptr = fopen(file_name, "r")))
	{
		printf("File not found\n");
		return 1;
	}

	if (num_traders <= 0)
	{
		printf("There aren't enough traders to begin trading.\n");
		return 2;
	}

	pid_t *pids = (pid_t *)(malloc(sizeof(pid_t) * (num_traders)));
	traders = (struct trader*)(malloc(sizeof(struct trader) * (num_traders)));

	// setting it to -1 to avoid any confusion with child processes

	char buffer[BUFFER_SIZE];
	int check1 = 1;

	int product_count = 0;
	while (1)
	{
		if (check1)
		{
			fgets(buffer, 32, fptr);
			num_products = atoi(buffer);
			check1 = 0;
			products = (char **)(malloc(sizeof(char *) * num_products));
			continue;
		}

		if (!fgets(buffer, BUFFER_SIZE, fptr))
		{
			break;
		}
		buffer[strcspn(buffer, "\r\n")] = '\0';
		products[product_count] = (char *)(malloc(sizeof(char) * BUFFER_SIZE));
		// Add a check to make sure product has length of 16
		strncpy(products[product_count], buffer, BUFFER_SIZE);
		add_to_list(&product_book, buffer);
		product_book->product_id = product_count;
		product_count++;
	}

	fclose(fptr);
	// Just reading the values, and initialising the necessary conditions
	begin_pex(num_products, products);

	struct sigaction sig = {
        .sa_sigaction = &sig_handler,
		.sa_flags = SA_SIGINFO 
    };
	
	sigaction(SIGUSR1, &sig, NULL);  
    sigaction(SIGCHLD, &sig, NULL);

	 // Create the corresponding FIFOs
	// char **pe_exch_fifo = (char **)(malloc(sizeof(char *) * num_traders));
	// char **trader_fifo = (char **)(malloc(sizeof(char *) * num_traders));

	for (int i = 0; i < num_traders; i++)
	{
		char *exch_name;
		char *trader_name;
		traders[i].trader_id = i;

		if (0 > asprintf(&exch_name, FIFO_EXCHANGE, i))
		{
			printf("Sum ting wong %d\n", i);
			return 3;
		}

		if (0 > asprintf(&trader_name, FIFO_TRADER, i))
		{
			printf("Anoda ting wong %d\n", i);
			return 3;
		}

		traders[i].exch_fifo = (char *)(malloc(sizeof(char) * BUFFER_SIZE));
		traders[i].trad_fifo = (char *)(malloc(sizeof(char) * BUFFER_SIZE));
		traders[i].order_id = 0;


		strncpy(traders[i].exch_fifo, exch_name, BUFFER_SIZE);
		strncpy(traders[i].trad_fifo, trader_name, BUFFER_SIZE);
		free(exch_name);
		free(trader_name);

		traders[i].products = (int*)calloc(num_products,sizeof(long));
		traders[i].money_exch = (long*)calloc(num_products,sizeof(long));

		unlink(traders[i].exch_fifo);
		unlink(traders[i].trad_fifo);

		if (mkfifo(traders[i].exch_fifo, 0666) < 0)
		{
			printf("Unable to create the exchange fifo %d", i);
			return 4;
		}
		printf("[PEX] Created FIFO %s\n", traders[i].exch_fifo);

		if (mkfifo(traders[i].trad_fifo, 0666) < 0)
		{
			printf("Unable to create trader fifo %d", i);
			return 4;
		}
		
		printf("[PEX] Created FIFO %s\n", traders[i].trad_fifo);
		pid_t curr_pid;
		if ((curr_pid = fork()) < 0)
		{
			printf("Unable to create the child process for the trader.");
			return 3;
		}

		if (curr_pid == 0)
		{
			// start the child process
			// connect the fifos
			printf("[PEX] Starting trader %d (%s)\n", i, argv[2 + i]);
			// exec over here.
			char trader_id[BUFFER_SIZE];
			sprintf(trader_id, "%d", i);
			char *custom_argv[] = {argv[2 + i], trader_id, NULL};
			if(execvp(argv[2 + i], custom_argv) < 0){
				perror("Unable to create a child process");
				return 7;
			}
		}
		else
		{
			//Connecting all the fifos from exchange to the traders
			traders[i].pid = curr_pid;
			traders[i].exch_fd = open(traders[i].exch_fifo, O_WRONLY);
			printf("[PEX] Connected to %s\n", traders[i].exch_fifo);
			traders[i].trader_fd = open(traders[i].trad_fifo, O_RDONLY);
			printf("[PEX] Connected to %s\n", traders[i].trad_fifo);
			traders[i].connected = 1;
			usleep(1);
		}
	}
	//starting the market and main functionalities
	message_all_traders("MARKET OPEN;");
	int order_num = 0;
	//event loop:
	while(num_traders > inactive_traders){
		pause();
		if(op_recieved){	
			if(parse_message(who_sent, order_num++)){
				match_orderbook();
			
				if(matched){
					free_required_nodes(most_rec_log);
					matched = 0;
				}
				print_orderbook();
				print_positions();
			}
			op_recieved = 0;
		}
		//make the priority queue
		//read and split the input nicely
		//push order onto pq
		//do the order matching
		//sigstop and sigcont everytime you recieve a signal 
	}

	//closing pipes
	for(int i = 0; i < num_traders; i++){
		close(traders[i].exch_fd);
		close(traders[i].trader_fd);
	}

	//Tear down from here!
	printf("[PEX] Trading completed\n");
	printf("[PEX] Exchange fees collected: $%ld\n", exchange_fee);
	// Freeing anything we malloced

	for (int i = 0; i < product_count; i++)
	{
		free(products[i]);
	}

	for(int i = 0; i < num_traders; i++){
		unlink(traders[i].trad_fifo);
		unlink(traders[i].exch_fifo);
	}

	for (int i = 0; i < num_traders; i++)
	{
		free(traders[i].exch_fifo);
		free(traders[i].trad_fifo);
		free(traders[i].products);
		free(traders[i].money_exch);
	}
	free_product_book(&product_book);
	free(pids);
	free(products);
	free(traders);
	return 0;
}
//------------------------------------------------------------------------------
void begin_pex(int num_products, char **products)
{
	printf("[PEX] Starting\n");
	printf("[PEX] Trading %d products:", num_products);
	for (int i = 0; i < num_products; i++)
	{
		printf(" %s", products[i]);
	}
	printf("\n");
}

//helper functions, with different cases for messaging traders.
void message_trader(int trader_id, char *message) {
	if(traders[trader_id].connected){
		write(traders[trader_id].exch_fd, message, strlen(message));
		kill(traders[trader_id].pid, SIGUSR1);
	}
}

void message_all_traders(char* message){
	for(int i = 0; i < num_traders; i++){
		if(traders[i].connected){
			message_trader(i, message);
		}
	}
}

void message_all_other_traders(int trader_id,char* message){
	for(int i = 0; i < num_traders; i++){
		if(i != trader_id){
			if(traders[i].connected){
				message_trader(i, message);	
			}
		} 
	}
}
//helper function to get the trader id from the corresponding Process id
int get_trader_id(pid_t pid) {
    for (int i = 0; i < num_traders; i++) {
        if (pid == traders[i].pid)
            return i;
    }
    return -1;
}

void call_disc(int disc_trader){
	printf("[PEX] Trader %d disconnected\n", disc_trader);
}

//---------------------------------------------------------------------------------------------------------------
//reading the message from the trader pipe.
int parse_message(int trader_id, int order_num){
    char message[BUFFER_SIZE];
    if(read(traders[trader_id].trader_fd, message, BUFFER_SIZE) < 0){
        perror("Unable to read message.\n");
    }
	char* ptr = message;
	//The exchange reads the message till a semi colon, and removes it.
	while(*ptr){
		if(*ptr == ';'){
			*ptr = '\0';
			break;
		}
		ptr++;
	}


    if(strncmp(message, "SELL", strlen("SELL")) == 0 || strncmp(message, "BUY", strlen("BUY")) == 0){
		if(traders[trader_id].connected){
			printf("[PEX] [T%d] Parsing command: <%s>\n", trader_id, message);
	
			char operation[BUFFER_SIZE], product[BUFFER_SIZE], ending[BUFFER_SIZE];
			int order_id, qty, price;
			//each message has a set format, sscanf returns the number of variables. In case of an error in formatting,
			//the number of variables will be incorrect.
			int n = sscanf(message, "%s %d %s %d %d %s",operation, &order_id, product, &qty, &price, ending);
			if(n != 5){
				message_trader(trader_id, "INVALID;");
				return 0;
			}
			//process the buy/sell orders
			return handle_buy_sell(trader_id, operation, order_id, product, qty, price, order_num);
		}

    }
	//process commands amend and delete
    else if(strncmp(message, "AMEND", strlen("AMEND")) == 0){

		if(traders[trader_id].connected){
			printf("[PEX] [T%d] Parsing command: <%s>\n", trader_id, message);
			char operation[BUFFER_SIZE], ending[BUFFER_SIZE];
			int order_id, qty, price;
			int n = sscanf(message, "%s %d %d %d %s", operation, &order_id, &qty, &price, ending);
			if(n != 4){
				message_trader(trader_id, "INVALID;");
				return 0;
			}
			if(!invalid_amend(order_id, qty, price)){
				return amend_val(trader_id,order_id, qty, price);
			}

			message_trader(trader_id, "INVALID;");
			return 0;;
		}

    }

    else if(strncmp(message, "CANCEL", strlen("CANCEL")) == 0){
		if(traders[trader_id].connected){
			printf("[PEX] [T%d] Parsing command: <%s>\n", trader_id, message);
			char operation[BUFFER_SIZE], ending[BUFFER_SIZE];
			int order_id;
			int n = sscanf(message, "%s %d %s", operation, &order_id, ending);
			if(n != 2){
				message_trader(trader_id, "INVALID;");
				return 0;
			}

			if(!(order_id < 0 || order_id > 999999)){
				return cancel_item(trader_id, order_id);
			}
		message_trader(trader_id, "INVALID;");
		return 0;
		}
    }

    else{
		message_trader(trader_id, "INVALID;");
		return 0;
    }

	message_trader(trader_id, "INVALID;");
	return 0;
}
int product_exists(char* product){
	for(int i = 0; i < num_products; i++){
		if(strcmp(products[i], product) == 0){
			return 1;
		}
	}
	return 0;
}
int handle_buy_sell(int trader_id, char* operation, int order_id, char* product, int qty, int price, int order_num){
	if(traders[trader_id].connected){
		//basic error handling
		if(invalid_buy_sell(order_id, product, qty, price)){
			message_trader(trader_id, "INVALID;");
			return 0;
		}

		if(!product_exists(product)){
			message_trader(trader_id, "INVALID;");
			return 0;
		}

		if(traders[trader_id].order_id != order_id){
			message_trader(trader_id, "INVALID;");
			return 0;
		}
	
		//now we push the valid order onto the order book.
		struct product_book * ptr = product_book;
		while(ptr){
			if(strcmp(ptr -> product, product) == 0){
				if(strcmp(operation, "BUY") == 0){	
					enqueue(&(ptr->buy_pq), order_num, order_id, 0, qty, price, trader_id);		
					traders[trader_id].order_id++;
					most_rec_order = order_num;
					most_rec_op = 0;
					most_rec_log = ptr;
					break;
				}

				else if(strcmp(operation, "SELL") == 0){
					enqueue(&(ptr->sell_pq), order_num, order_id, 1, qty, price, trader_id);
					traders[trader_id].order_id++;
					most_rec_order = order_num;
					most_rec_op = 1;
					most_rec_log = ptr;
					break;
				}
			}
			ptr = ptr -> next;
		}

		char* response1;
		char* response2;
		//response to trader after succesful transaction.
		if (0 > asprintf(&response1, "ACCEPTED %d;", order_id))
		{
			printf("not able to print accepted %d\n", order_id);
			return 0;
		}

		if (0 > asprintf(&response2, "MARKET %s %s %d %d;", operation, product, qty, price))
		{
			printf("not able to print accepted %d\n", order_id);
			return 0;
		}
		message_trader(trader_id, response1);
		message_all_other_traders(trader_id, response2);
		free(response1);
		free(response2);
	}

	return 1;
}

int cancel_item(int trader_id, int order_id){
	struct product_book* ptr = product_book;
	while(ptr){
		if(cancel(&(ptr->buy_pq), order_id, trader_id)){
			char* message1;
			char* message2;
			asprintf(&message1, "MARKET BUY %s %d %d;", ptr -> product, 0, 0);
			message_all_other_traders(trader_id,message1);
			asprintf(&message2, "CANCELLED %d;", order_id);
			message_trader(trader_id,message2);
			free(message1);
			free(message2);
			return 1;
		}

		if(cancel(&(ptr->sell_pq), order_id, trader_id)){
			char* message1;
			char* message2;
			asprintf(&message1, "MARKET SELL %s %d %d;", ptr -> product, 0, 0);
			message_all_other_traders(trader_id,message1);
			asprintf(&message2, "CANCELLED %d;", order_id);
			message_trader(trader_id,message2);
			free(message1);
			free(message2);
			return 1;
		}

		ptr = ptr -> next;
	}
	message_trader(trader_id, "INVALID;");
	return 0;
}

int amend_val(int trader_id,int order_id,int qty,int price){
	struct product_book* ptr = product_book;
	while(ptr){
		if(amend(&(ptr->buy_pq), order_id, trader_id, qty, price)){
			char* message1;
			char* message2;
			asprintf(&message1, "MARKET BUY %s %d %d;", ptr -> product, qty, price);
			message_all_other_traders(trader_id,message1);
			asprintf(&message2, "AMENDED %d;", order_id);
			message_trader(trader_id,message2);
			free(message1);
			free(message2);
			return 1;
		}

		if(amend(&(ptr->sell_pq), order_id, trader_id, qty, price)){
			char* message1;
			char* message2;
			asprintf(&message1, "MARKET SELL %s %d %d;", ptr -> product, qty, price);
			message_all_other_traders(trader_id,message1);
			asprintf(&message2, "AMENDED %d;", order_id);
			message_trader(trader_id,message2);
			free(message1);
			free(message2);
			return 1;
		}

		ptr = ptr -> next;
	}
	message_trader(trader_id, "INVALID;");
	return 0;
}



int invalid_buy_sell(int order_id, char * product, int qty, int price){
    if(order_id < 0 || order_id > 999999){
        return 1;
    }
    else if(qty <= 0 || qty > 999999){
        return 1;
    }

    else if(price <= 0 || price > 999999){
        return 1;
    }
    return 0;
}

int invalid_amend(int order_id, int qty, int price){
    if(order_id < 0 || order_id > 999999){
        return 1;
    }
    else if(qty <= 0 || qty > 999999){
        return 1;
    }

    else if(price <= 0 || price > 999999){
        return 1;
    }
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

int same_price_product_buy(struct product_book* product){
	struct node* back = product->buy_pq;
	if(!back){
		return 0;
	}
	struct node* front = back->next;
	int levels = 1;
	while(front){
		if(front->price == back -> price){
			front = front -> next;
		}
		else{
			levels++;
			back = front;
			front = front -> next;

		}
	}
	return levels;
}

int same_price_product_sell(struct product_book* product){
	struct node* back = product->sell_pq;
	if(!back){
		return 0;
	}
	struct node* front = back->next;
	int levels = 1;
	while(front){
		if(front->price == back -> price){
			front = front -> next;
		}
		else{
			levels++;
			back = front;
			front = front -> next;	
		}
	}
	return levels;
}


void print_orderbook(){
	struct product_book * ptr = product_book;
	printf("[PEX]\t--ORDERBOOK--\n");
	while(ptr){
		ptr->buy_levels = same_price_product_buy(ptr);
		ptr->sell_levels = same_price_product_sell(ptr);
		printf("[PEX]\tProduct: %s; Buy levels: %d; Sell levels: %d\n", ptr->product, ptr->buy_levels, ptr->sell_levels);
		print_buy_sell(ptr,ptr->buy_pq, ptr->sell_pq);
		ptr = ptr -> next;
	}
}

void print_buy_sell(struct product_book* curr, struct node* buy_pq, struct node* sell_pq){
	print_sell(sell_pq);
	print_buy(buy_pq);
}

void print_sell(struct node* sell_pq){
	struct node* back = sell_pq;
	if(!back){
		return;
	}
	struct node* front = back -> next;
	if(!front){
		printf("[PEX]\t\tSELL %d @ $%d (1 order)\n", back->qty, back->price);
	}

	int total_qty = back -> qty;
	int num_orders = 1; 

	while(front){
		if(front->price == back -> price){
			total_qty += front -> qty;
			num_orders += 1;
			front = front->next;
			if(!front){
				if(num_orders == 1){
					printf("[PEX]\t\tSELL %d @ $%d (1 order)\n", total_qty, back->price);
				}
				else{
					printf("[PEX]\t\tSELL %d @ $%d (%d orders)\n", total_qty, back->price, num_orders);
				}
			}
		}

		else{
			//print the val
			if(num_orders == 1){
				printf("[PEX]\t\tSELL %d @ $%d (1 order)\n", total_qty, back->price);
			}
			else{
				printf("[PEX]\t\tSELL %d @ $%d (%d orders)\n", total_qty, back->price, num_orders);
			}
			
			back = front;
			front = front -> next;
			num_orders = 1;
			total_qty = back -> qty;
		}

		if(!(back -> next)){
			printf("[PEX]\t\tSELL %d @ $%d (1 order)\n", total_qty, back->price);
		}
	}
}

void print_buy(struct node* buy_pq){
	struct node* back = buy_pq;
	if(!back){
		return;
	}
	struct node* front = back -> next;

	if(!front){
		printf("[PEX]\t\tBUY %d @ $%d (1 order)\n", back->qty, back->price);
	}
	
	int total_qty = back -> qty;
	int num_orders = 1; 

	while(front){
		if(front->price == back -> price){
			total_qty += front -> qty;
			num_orders += 1;
			front = front->next;
			if(!front){
				if(num_orders == 1){
					printf("[PEX]\t\tBUY %d @ $%d (1 order)\n", total_qty, back->price);
				}
				else{
					printf("[PEX]\t\tBUY %d @ $%d (%d orders)\n", total_qty, back->price, num_orders);
				}
			}
		}

		else{
			//print the val
			if(num_orders == 1){
				printf("[PEX]\t\tBUY %d @ $%d (1 order)\n", total_qty, back->price);
			}
			else{
				printf("[PEX]\t\tBUY %d @ $%d (%d orders)\n", total_qty, back->price, num_orders);
			}
			
			back = front;
			front = front -> next;
			num_orders = 1;
			total_qty = back -> qty;
		}

		if(!(back -> next)){
			printf("[PEX]\t\tBUY %d @ $%d (1 order)\n", total_qty, back->price);
		}
	}
}


void print_positions(){
	printf("[PEX]\t--POSITIONS--\n");
	for(int i = 0; i < num_traders; i++){
		printf("[PEX]\tTrader %d:", i);
		for(int j = 0; j < num_products; j++){
			if(j == num_products - 1){
				printf(" %s %d ($%ld)\n",products[j], traders[i].products[j], traders[i].money_exch[j]);
			}
			else{
				printf(" %s %d ($%ld),",products[j], traders[i].products[j], traders[i].money_exch[j]);
			}
		}
	}
}

void match_orderbook(){
	if(most_rec_op == -1){
		return;
	}

	if(most_rec_op == 0 && most_rec_log -> sell_pq == NULL){
		return;
	}

	if(most_rec_op == 1 && most_rec_log -> buy_pq == NULL){
		return;
	}
	int num;
	for(int i = 0; i < num_products; i++){
		if(strcmp(products[i], most_rec_log -> product) == 0){
			num = i;
		}
	}

	if(most_rec_op == 0){
		reverse(&(most_rec_log -> sell_pq));
		struct node* ptr = most_rec_log -> sell_pq;
		struct node* to_match = most_rec_log -> buy_pq;
		
		while(to_match){
			if(to_match->order_num == most_rec_order){
				break;
			}
			to_match = to_match -> next;
		}	
	
		while(ptr){
			if(ptr -> price <= to_match -> price){
				//print match
				matched = 1;
				if(to_match -> qty >= ptr -> qty){
					to_match -> qty -= ptr -> qty;
					char* message1;
					char* message2;
					long value = ((long) ptr -> qty) * ptr -> price;
					long fee = value * 0.01;
					long check = value % 100 < 50 ? 0 : 1;
					fee += check;
					exchange_fee += fee;
					traders[to_match->trader_id].money_exch[num] = traders[to_match->trader_id].money_exch[num] - value - fee;
					traders[ptr->trader_id].money_exch[num] += value;
					traders[to_match->trader_id].products[num] += ptr->qty;
					traders[ptr->trader_id].products[num] -= ptr->qty;
					printf("[PEX] Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n",
										ptr -> order_id, ptr -> trader_id, to_match -> order_id, to_match -> trader_id, value, fee);
					asprintf(&message1, "FILL %d %d;", to_match->order_id, ptr -> qty);
					message_trader(to_match -> trader_id, message1);
					asprintf(&message2, "FILL %d %d;", ptr->order_id, ptr -> qty);
					message_trader(ptr -> trader_id, message2);
					free(message1);
					free(message2);
					ptr->remove_node = 1;
					if(to_match -> qty == 0){
						to_match -> remove_node = 1;
						break;
					}

					ptr = ptr -> next;
					continue;
				}

				else if(to_match -> qty < ptr -> qty){
					ptr -> qty -= to_match -> qty;
					char* message1;
					char* message2;
					long value = (long) to_match -> qty * ptr -> price;
					long fee = value * 0.01;
					long check = value % 100 < 50 ? 0 : 1;
					fee += check;
					exchange_fee += fee;

					traders[to_match->trader_id].money_exch[num] = traders[to_match->trader_id].money_exch[num] - value - fee;
					traders[ptr->trader_id].money_exch[num] += value;
					traders[to_match->trader_id].products[num] += to_match->qty;
					traders[ptr->trader_id].products[num] -= to_match->qty;

					printf("[PEX] Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n",
										ptr -> order_id, ptr -> trader_id, to_match -> order_id, to_match -> trader_id, value, fee);
					asprintf(&message1, "FILL %d %d;", to_match->order_id, to_match -> qty);
					message_trader(to_match -> trader_id, message1);
					asprintf(&message2, "FILL %d %d;", ptr->order_id, to_match -> qty);
					message_trader(ptr -> trader_id, message2);
					free(message1);
					free(message2);
					to_match -> remove_node = 1;
					break;
				}
			}

			ptr = ptr -> next;
		}
		reverse(&(most_rec_log -> sell_pq));
	}

	else if(most_rec_op == 1){
		struct node* ptr = most_rec_log -> buy_pq;
		struct node* to_match = most_rec_log -> sell_pq;
		
		while(to_match){
			if(to_match->order_num == most_rec_order){
				break;
			}
			to_match = to_match -> next;
		}	
	
		while(ptr){
			if(ptr -> price >= to_match -> price){
				//print match
				matched = 1;
				if(to_match -> qty > ptr -> qty){
					to_match -> qty -= ptr -> qty;
					char* message1;
					char* message2;

					long value = (long) ptr -> qty * ptr -> price;
					long fee = value * 0.01;
					long check = value % 100 < 50 ? 0 : 1;
					fee += check;
					exchange_fee += fee;
					
					traders[to_match->trader_id].money_exch[num] += value - fee;
					traders[ptr->trader_id].money_exch[num] -= value;
					traders[to_match->trader_id].products[num] -= ptr->qty;
					traders[ptr->trader_id].products[num] += ptr->qty;

					printf("[PEX] Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n",
										ptr -> order_id, ptr -> trader_id, to_match -> order_id, to_match -> trader_id, value, fee);
					asprintf(&message1, "FILL %d %d;", to_match->order_id, ptr -> qty);
					message_trader(to_match -> trader_id, message1);
					asprintf(&message2, "FILL %d %d;", ptr->order_id, ptr -> qty);
					message_trader(ptr -> trader_id, message2);
					free(message1);
					free(message2);
					ptr->remove_node = 1;
					if(to_match -> qty == 0){
						to_match -> remove_node = 1;
						break;
					}

					ptr = ptr -> next;
					continue;
				}

				else if(to_match -> qty < ptr -> qty){
					ptr -> qty -= to_match -> qty;
					char* message1;
					char* message2;
					long value = (long) to_match -> qty * ptr -> price;
					long fee = value/100;
					long check = value % 100 < 50 ? 0 : 1;
					fee += check;
					exchange_fee += fee;

					traders[to_match->trader_id].money_exch[num] += value - fee;
					traders[ptr->trader_id].money_exch[num] -= value;
					traders[to_match->trader_id].products[num] -= to_match->qty;
					traders[ptr->trader_id].products[num] += to_match->qty;

					printf("[PEX] Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n",ptr -> order_id, ptr -> trader_id, to_match -> order_id, to_match -> trader_id, value, fee);
					asprintf(&message1, "FILL %d %d;", to_match->order_id, to_match -> qty);
					message_trader(to_match -> trader_id, message1);
					asprintf(&message2, "FILL %d %d;", ptr->order_id, to_match -> qty);
					message_trader(ptr -> trader_id, message2);
					free(message1);
					free(message2);
					to_match -> remove_node = 1;
					break;
				}
			}

			else{
				ptr = ptr -> next;
			}
		}
	}

	most_rec_op = -1;
	most_rec_order = -1;
}

void free_required_nodes(struct product_book* required_log){
	struct node* ptr1 = required_log -> buy_pq;
	struct node* ptr2 = required_log -> sell_pq;
	while(ptr1){
		struct node* curr = ptr1;
		ptr1 = ptr1 -> next;	
		if(curr->remove_node){
			remove_node(&(required_log->buy_pq), curr);
		}
	}

	while(ptr2){
		struct node* curr = ptr2;
		ptr2 = ptr2 -> next;	
		if(curr->remove_node){
			remove_node(&(required_log->sell_pq), curr);
		}
	}
}

//---------------------------------------------------------------------------------------------------------------

void enqueue(struct node** head, int order_num, int order_id,int order_type, int qty, int price, int trader_id){
//The priority queue we maintain is a max priority queue so we store the highest value
//setting up the new node
    struct node* node =  (struct node*) malloc(sizeof(struct node));
    node->order_num = order_num;
    node->order_id = order_id;
    node->qty = qty;
    node->price = price;
    node->trader_id = trader_id;
    node->order_type = order_type;
	node->remove_node = 0;
    node->next = NULL;

    if(*head == NULL){
        *head = node;
        (*head) -> next = NULL;
        return;
    }

    struct node* ptr = *head;
//if the value of the node is less than or equal to the new node we insert the new node before the node being looked at in the list
    while(ptr){
        if(node -> price > ptr -> price){
            insert_before(head, ptr, node);
            return;
        }

        //check for last node 
        if(!(ptr -> next)){
            ptr -> next = node;
            return;
        }
        ptr = ptr -> next;
    }
}

void remove_node_buy(struct product_book* curr, struct node* node){
	struct node* head = curr -> buy_pq;
	remove_node(&head, node);
}

void remove_node_sell(struct product_book* curr, struct node* node){
	struct node* head = curr -> sell_pq;
	remove_node(&head, node);
}

void remove_node(struct node** head, struct node* node){
	if (node == *head) {
        *head = node->next;
        free(node);
        return;
    }

    struct node* before = *head;
    while (before->next != node) {
        before = before->next;
    }

    before->next = node->next;

    free(node);

    return;
    
}

int amend(struct node** head, int order_id, int trader_id, int qty, int price){
    struct node* ptr = *head;
	struct node* prev = ptr;
	int i = 0;
    while(ptr){
        if(ptr-> order_id == order_id && ptr->trader_id == trader_id){
            if(i == 0){
				ptr -> qty = qty;
				ptr -> price = price;
				*head = ptr -> next;
				enqueue(head, ptr -> order_num, ptr -> order_id, ptr -> order_type, qty, price, trader_id);
				ptr -> next = NULL;
				free(ptr);
				return 1;
			}else{
				prev -> next = ptr -> next;
				enqueue(head, ptr -> order_num, ptr -> order_id, ptr -> order_type, qty, price, trader_id);
            	ptr -> next = NULL;
				free(ptr);
				return 1;
			}
        }
		if(i == 0){
			ptr = ptr->next;
		}
		else{
			ptr = ptr -> next;
			prev = prev -> next;
		}
		i++;
    }
    return 0;
}

int cancel(struct node** head, int order_id, int trader_id){
    struct node* ptr = *head;

    while(ptr){
        if(ptr-> order_id == order_id && ptr->trader_id == trader_id){
			remove_node(head, ptr);
            return 1;
        }

		ptr = ptr->next;
    }
    return 0;
}

void insert_before(struct node** head, struct node* node, struct node* to_insert){
    if(node == *head){
        to_insert -> next = node;
        *head = to_insert;
        return;
    }

    struct node* ptr = *head;

    while(ptr){
        if(ptr -> next == node){
            struct node* temp = ptr -> next;
            ptr -> next = to_insert;
            to_insert -> next = temp;
            return;
        }

        ptr = ptr -> next;
    }
    
}


void free_mem(struct node** head) {
    //free whatever is left of the list
    struct node* node = *head;
    while (node != NULL) {
        struct node* last_node = node;
        node = node->next;
        free(last_node);
    }

    *head = NULL;
    return;
}

void add_to_list(struct product_book** head, char* product){
	struct product_book* new_node = (struct product_book*)malloc(sizeof(struct product_book));
	new_node->product = malloc(BUFFER_SIZE*sizeof(char));
	strncpy(new_node->product, product, BUFFER_SIZE);
	new_node->buy_levels = 0;
	new_node->sell_levels = 0;
	new_node->buy_pq = NULL;
	new_node->sell_pq = NULL;
	new_node->next = NULL;

	
	if(*head == NULL){
		*head = new_node; 
		return;
	}

	struct product_book* ptr = *head;
	while(ptr->next){
		ptr = ptr->next;
	}
	ptr -> next = new_node;

}

int check_exists(struct product_book** head, char* product){
	struct product_book* ptr = *head;
	while(ptr){
		if(strcmp(ptr->product, product) == 0){
			return 1;
		}
		ptr = ptr -> next;
	}

	return 0;
}
void free_product_book(struct product_book** head){
    //free whatever is left of the list
    struct product_book* node = *head;
    while (node != NULL) {
        struct product_book* last_node = node;
        node = node->next;
		free(last_node -> product);
		free_mem(&(last_node -> buy_pq));
		free_mem(&(last_node -> sell_pq));
        free(last_node);
    }

    *head = NULL;
    return;
}

void reverse(struct node** head){
	struct node* prev = NULL;
    struct node* current = *head;
    struct node* next = NULL;
    while (current != NULL) {
        // Store next
        next = current->next;
 
        // Reverse current node's pointer
        current->next = prev;
 
        // Move pointers one position ahead.
        prev = current;
        current = next;
    }
    *head = prev;
}
