# Jajodia-Mutchler
The final project for the "Advanced Operating Systems" course (CS6378) (taught by Ravi Prakash), in which we implement the Jajodia-Mutchler algorithm.

## How to compile

### Controller

> g++ --std=c++11 -pthread controller.cpp -o control


### Servers
> g++ -pthread --std=c++11 include/serversocket.cpp include/clientsocket.cpp include/socket.cpp server.cpp -o server

## How to run
Activate the servers before the controller.

### Servers
`server_designation` is an integer between `[0, 7]`. Update the ip address and ports in the [`servers` object in the `commons.h` file](https://github.com/kraftpunk97/Jajodia-Mutchler/blob/f51f7e9d7de6386133a8275319e8387c55aa7ebe/commons.h#L19).
> ./server <server_port> <server_designation>


### Controller

> ./control



---
PS. If you take this course in the future and you get this exact same assignment, then I gotchu covered fam.
---