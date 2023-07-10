#ifndef ORDER_OPERATIONS
#define ORDER_OPERATIONS_H
#include "pe_common.h"


int parse_message(int trader_id, int order_num);
int handle_buy_sell(int trader_id, char* operation, int order_id, char* product, int qty, int price, int order_num);
int invalid_buy_sell(int order_id, char * product, int qty, int price);
int invalid_amend(int order_id, int qty, int price);
int get_price(char* string);
int cancel_item(int trader_id, int order_id);
void print_orderbook();
void print_buy_sell(struct product_book * product_book,struct node* buy_pq, struct node* sell_pq);
int same_price_product_buy(struct product_book* product);
int same_price_product_sell(struct product_book* product);
void print_positions();
void print_sell(struct node* curr);
void print_buy(struct node* curr);
void remove_node_buy(struct product_book* curr, struct node* node);
void remove_node_sell(struct product_book* curr, struct node* node);
void match_orderbook();
void free_required_nodes(struct product_book* required_log);
int amend_val(int trader_id,int order_id,int qty,int price);
int product_exists(char* product);
#endif