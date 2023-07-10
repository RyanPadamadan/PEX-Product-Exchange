#ifndef PRIORITY_QUEUE
#define PRIORITY_QUEUE_H

#include "pe_common.h"

struct node{
    int order_num;
    int trader_id;
    int order_id;
    int qty;
    int price;
    int order_type;
    int remove_node;
    struct node * next;
    //1 for buy
    //2 for sell
    //3 for amend
    //4 for cancel
};

struct product_book{
    char* product;
    int buy_levels;
    int sell_levels;
    struct product_book* next;
    struct node* buy_pq;
    struct node* sell_pq;
    int product_id;
};

//add to list that starts from empty
//free memory
//check_exists

void enqueue(struct node** head, int order_num, int order_id,int order_type, int qty, int price, int trader_id);
void remove_node(struct node** head, struct node* node);
int amend(struct node** head, int order_id, int trader_id, int qty, int price);
void insert_before(struct node** head, struct node* node, struct node* to_insert);
void free_mem(struct node** head);
void add_to_list(struct product_book** head, char* product);
int check_exists(struct product_book** head, char* product);
void free_product_book(struct product_book** head);
int cancel(struct node** head, int order_id, int trader_id);
void reverse(struct node** head);

//void free_product_book(struct product_book** head); 
#endif