# RF24_Raspi
Using RF24 antenna on Raspi to facilitate heterogeneous wireless network

* dsdv.cpp

>1. contain a main func, running simple dsdv protocol to setup route table
>2. nxt node is changed when DV broadcast message is loss for a time threshold
>3. route table is saved to file "route.dat"

* gps-based.cpp
 
>1. contain a main func, running our gps-based routing protocol to setup route table
>2. nxt node is changed when distance is beyond some threshold
>3. route table is saved to file "route.dat"

* RF24Network_dsdv.cpp & RF24Network_gps.cpp
 
>1. basic network function based on RF24 and dsdv(gps) routing protocol
>2. Specifically, enqueue()/logtophysicaladdress() function is modified, and available_dsdv() (available_gps()) function is added

* RF24Network_dsdv.h & RF24Network_gps.h
 
>1. header file for RF24Network_dsdv.cpp & RF24Network_gps.cpp
>2. annouce the prototype of above function
>3. define map type: route_table; queue type: dsdv_queue/gps_queue;

* log.h & log.cpp
 
>1. log system to print log msg in self-defined format
>2. [yy-mm-dd hh-mm-ss] [LEVEL]- msg
