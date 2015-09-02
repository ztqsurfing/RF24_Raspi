# RF24_Raspi
Using RF24 antenna on Raspi to facilitate heterogeneous wireless network

# dsdv.cpp
- contain a main func, running simple dsdv protocol to setup route table
- nxt node is changed when DV broadcast message is loss for a time threshold
- route table is saved to file "route.dat"

# gps-based.cpp
- contain a main func, running our gps-based routing protocol to setup route table
- nxt node is changed when distance is beyond some threshold
- route table is saved to file "route.dat"

# RF24Network_dsdv.cpp & RF24Network_gps.cpp
- basic network function based on RF24 and dsdv(gps) routing protocol
- Specifically, enqueue()/logtophysicaladdress() function is modified, and available_dsdv() (available_gps()) function is added

# RF24Network_dsdv.h & RF24Network_gps.h
- header file for RF24Network_dsdv.cpp & RF24Network_gps.cpp
- annouce the prototype of above function
- define map type: route_table; queue type: dsdv_queue/gps_queue;

# log.h & log.cpp
- log system to print log msg in self-defined format
- [yy-mm-dd hh-mm-ss] [LEVEL]- msg
