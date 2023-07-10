# PEX-Product-Exchange
Created a trading platform entirely in C, where multiple traders can communicate amongst themselves and
trade different products.

My exchange has all the products stored in a linked-list, with each node pointing to two priority queues, one for buy operations and one for sell operations. Then to match the orders, the priority queues are used, to match the smallest sell with largest buy and vice-versa. Communication between traders and exchange is done using fifos, followed by a signal.


To make the traders fault tolerant there are a few main things I did. First is I use pause before two places I anticipate a signal The first is before recieving any message, the second is sending a sell request and waiting for an accept. To make sure the exchange does not miss out on any signals, the signals are repeatedly sent with an interval of two seconds. Two seconds was chosen as a result of trial and error.

