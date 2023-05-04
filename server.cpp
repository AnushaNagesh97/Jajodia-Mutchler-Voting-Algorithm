#include "include/serversocket.hpp"
#include "include/clientsocket.hpp"
#include "include/socketexception.hpp"
#include "commons.h"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <unistd.h>
#include <poll.h>


auto phase_itr = phases.begin();

struct RequestingServerDeets {
    int to_idx, from_idx; 
};

class Server {
public:
    int m_designation;
    ServerSocket m_controllerSocket;
    ServerSocket m_serverSocket;
    ServerSocket m_peerFromSockets[NUM_SERVERS];
    ClientSocket m_peerToSockets[NUM_SERVERS];
    std::vector<int> m_peers;
    ObjectX SendInfo;
    std::vector<ObjectX> SetP;
    int is_Distinguished_flag = 0;
    int M = 0;
    int N;

    Server(int port, int designation): m_serverSocket(port) {
        ServerSocket new_socket;
        m_designation = designation;
        m_serverSocket.accept(new_socket);
        std::cout << "Connected to controller" << std::endl;
        m_controllerSocket = new_socket;
        SendInfo.server_id = designation;
        SendInfo.VN = 1;
        SendInfo.RU = NUM_SERVERS;
        SendInfo.DS = 0;
    }

    void ControllerCommand(){
        int endphase_flag = 0;
        while (!endphase_flag) {
            int* controllerMsg = new int;
            int bytes_read;
            do {
                bytes_read = m_controllerSocket.recv(controllerMsg, sizeof(int));
            } while(bytes_read==0);

            /*
            *  IF we get a None Start phase message, some "undefined-as-of-now" behaviour
            *  If we get a UPDATE Message,
            * */
            
            switch(*controllerMsg) {
                case NONE: {
                    std::cout << "Received None\n";
                    none();
                }
                break;
                case START_PHASE: {
                    std::cout << "Recieved PHASE\n";
                    phase();
                }
                break;
                case UPDATE: {
                    std::cout << "Received UPDATE on object X\n";
                    update();
                }
                break;
                case END_PHASE: {
                    endphase_flag = 1;
                    std::cout << "Received END_PHASE\n";
                }
                break;
            }
        }
        
    }

    void phase() {
        // Cut all ties...
        for (ServerSocket peer_socket: m_peerFromSockets) {
            if (peer_socket.is_valid()) {
                peer_socket.close();
            }
        }
        for (ClientSocket peer_socket: m_peerToSockets) {
            if (peer_socket.is_valid()) {
                peer_socket.close();
            }
        }

        // ...And form new bonds. (very poetic)
        std::string current_phase = *phase_itr;
        std::string separator = ":";
        std::size_t partition_end;
        std::size_t partition_start = 0;
        do {
            partition_end = current_phase.find(separator, partition_start);
            std::size_t nchars = partition_end - partition_start;
            std::string partition = current_phase.substr(partition_start, nchars);
            std::cout << "Partition is " << partition << std::endl;
            partition_start = partition_end + 1;

            // Am I in this partition?
            auto designation = std::to_string(m_designation);
            if (partition.find(designation) != std::string::npos) {
                std::cout << "Found the partition\n";
                // Who are my neighbors?
                for (int peer: partition) {
                    peer -= '0';
                    if (peer != m_designation) {
                        m_peers.push_back(peer);
                    }
                }
                std::cout << "My peers are:";
                for (int peer: m_peers)
                    std::cout << " " << peer;
                std::cout << std::endl;


                // Hi-Diddly-Ho, Neighborino!
                // Two parts that have to be done simultaneously...
                // 1. Listen for connections from other servers
                auto connFromUtil = [=]() {
                    for (int i = 0; i < m_peers.size(); i++) {
                        ServerSocket new_socket;
                        m_serverSocket.accept(new_socket);
                        std::cout << "Found a connection...\n";
                        m_peerFromSockets[i] = new_socket;
                    }
                };
                std::thread connFromThread = std::thread(connFromUtil);

                sleep(1);
                // 2. Connect to other servers
                auto connToUtil = [=]() {
                    for (int peer: m_peers) {
                        m_peerToSockets[peer] = ClientSocket();
                        std::string ip = servers[peer].ip;
                        int port = servers[peer].port;
                        std::cout << "IP: " << ip << "\tPort: " << port << std::endl;
                        m_peerToSockets[peer].connect(ip, port);
                        std::cout << "Connected to the server\n";
                    }
                };
                std::thread connToThread = std::thread(connToUtil);

                connToThread.join();
                connFromThread.join();
                break;
            }
        } while(partition_end != std::string::npos);
        phase_itr++;

        sleep(1);
        // Inform the controller that the partitioning is complete.
        int* success_msg_buffer = new int;
        *success_msg_buffer = 1;
        m_controllerSocket.send(success_msg_buffer, sizeof(int));
        std::cout << "Sent the partitioning confirmation to the controller.\n";
    }

    void close() {
        /*
         * Because we're fucking decent human beings...
         * */
        sleep(1);
        for (ServerSocket peer_socket: m_peerFromSockets) {
            if (peer_socket.is_valid()) {
                try {
                    peer_socket.close();
                } catch (SocketException& e) {}
            }
        }
        for (ClientSocket peer_socket: m_peerToSockets) {
            if (peer_socket.is_valid()) {
                try {
                    peer_socket.close();
                } catch (SocketException& e) {}
            }
        }
        try {
            m_controllerSocket.close();
        } catch (SocketException& e) {}

        try {
            m_serverSocket.close();
        } catch (SocketException& e) {}
    }

    void none() {
        RequestingServerDeets request_deets = listenForVoteReq();
        sendVote(request_deets.to_idx);
        listenForUpdate(request_deets.from_idx);
    }

    void update() {
        sendVoteReq();
        getVotes();
         /*TODO: Store the votes in an array. (Set P)
         *      M = Find the max VN
         *      Store all the votes with max VN (set I)
         *      Find N = setI[0].RU
         *      The three cases = if in majority, equal, or minority (is_distinguised())
         *      majority = setP.size() > N/2  -> do_update(); distinguished_flag = true (?);
         *      even = setP.size() == N/2 -> setP.in(setI[0].DS) == true -> do_update();
         *                                                       == false -> abort();
         *      minority = abort();
         *      catch_up() -> if self.VN < M -> (?)
         *      do_update() -> self.VN = M+1
         *      catch_up() -> for every server in setP, send (M-setP[i].VN) updates to setP[i] (?)
         *      The servers need to send a confirmation to the controller after the update is performed.
         */

        int ret_dist = isDistinguished();
        if(ret_dist) {
            catch_up();
            do_update();
        }
        else
            abort();
    }

    void listenForUpdate(int requesting_socket) {
        ObjectX* update_buffer = new ObjectX;
        m_peerFromSockets[requesting_socket].recv(update_buffer, sizeof(ObjectX));
        SendInfo = *update_buffer;
        if (update_buffer->VN == -1)
            std::cout << "Update was aborted" << std::endl;
        else {
            std::cout << "Someone did an update . Values updated to VN:" << SendInfo.VN << "\tRU:" << SendInfo.RU << "\tDS:" << SendInfo.DS << std::endl; 
        }
    }

    void sendVoteReq() {
        for (int i=0; i < m_peers.size(); i++) {
            int peer_idx = m_peers[i];
            m_peerToSockets[peer_idx].send(&SendInfo, sizeof(ObjectX));
            std::cout << "Sent Vote Request to peers\n";
        }
    }

    void getVotes() {
        for (int i=0; i<m_peers.size(); i++) {
            auto* recv_buffer = new ObjectX;
            try {
                m_peerFromSockets[i].recv(recv_buffer, sizeof(ObjectX));
            } catch (SocketException&) { std::perror("getVotes"); }
            std::cout << "Server:" << recv_buffer->server_id <<
                         " VN:" << recv_buffer->VN <<
                         " DS:" << recv_buffer->DS <<
                         " RU:" << recv_buffer-> RU << std::endl;
            //Store the votes in an array
            SetP.push_back(*recv_buffer);
        }
        SetP.push_back(SendInfo);
    }

    RequestingServerDeets listenForVoteReq() {
        struct pollfd pfds[m_peers.size()];

        for(int i = 0; i < m_peers.size(); i++) {
            pfds[i].fd = m_peerFromSockets[i].get_file_desc();
            pfds[i].events = POLLIN;
        }

        int ret = 0;
        int recv_socket_idx = -1;
        RequestingServerDeets request_deets;
        do {
            ret = poll(pfds, m_peers.size(), 100);
            if (ret) {
                for (int j = 0; j < m_peers.size(); j++) {
                    if (pfds[j].revents & POLLIN) {
                        ObjectX *ListenToReqbuf = new ObjectX;
                        int bytes_read = m_peerFromSockets[j].recv((ObjectX *) ListenToReqbuf, sizeof(ObjectX));
                        if (bytes_read < 0)
                            std::cerr << "Error receiving Vote Request from server " << m_peers[j] << std::endl;
                        else {
                            if (bytes_read > 0) {
                                recv_socket_idx = ListenToReqbuf->server_id;
                                request_deets.from_idx = j;
                                request_deets.to_idx = recv_socket_idx;
                                std::cout << "Received a vote request from " << recv_socket_idx << std::endl;
                                break;
                            }
                        }
                    }
                }
            }
        } while( recv_socket_idx == -1);
        return request_deets;
    }

    void sendVote(int requesting_socket) {
        m_peerToSockets[requesting_socket].send(&SendInfo, sizeof(ObjectX));
        std::cout << "Sent Vote to peer " << requesting_socket << std::endl;
    }

    int isDistinguished() {
        for(int i=0; i<= m_peers.size(); i++) {
            if (SetP[i].VN > M)
                M = SetP[i].VN;
        }
        std::cout << "Highest version number is " << M << std::endl;

        std::vector<ObjectX> setI;
        for(int i=0; i<=SetP.size(); i++) {
            if(SetP[i].VN == M) {
                setI.push_back(SetP[i]);
            }
        }
        //Find N
        N = setI[0].RU;
        int cardI = setI.size();
        std::cout << cardI << " " << N << std::endl;
        //Check if server is part of distinguished partition
        if(cardI > (N/2)) {
            is_Distinguished_flag = 1;
        }
        else if (N%2==0 && cardI==N/2) {
            for(int i=0; i < m_peers.size(); i++) {
                if(SetP[i].DS == SendInfo.DS) {
                    is_Distinguished_flag = 1;
                    break;
                }
                else
                    abort();
            }
        }
        else
            abort();
        return is_Distinguished_flag;
    }

    void catch_up() {
        if(SendInfo.VN < M) {
            std::cout << "Server has outdated version of X" << std::endl;
            SendInfo.VN = M;
        }
        else
            std::cout << "Server has latest version of X" << std::endl;
    }

    void do_update() {
        SendInfo.VN = M+1;
        SendInfo.RU = SetP.size(); // Including the server that wants to perform the update.
        int min_server_id = SendInfo.server_id;
        for(int i=0; i<m_peers.size(); i++) {
            min_server_id = min_server_id < SetP[i].server_id ? min_server_id : SetP[i].server_id;
        }
        SendInfo.DS = min_server_id;
        for (auto peer_id: m_peers) {
            m_peerToSockets[peer_id].send(&SendInfo, sizeof(ObjectX));
        }
    }

    void abort() {
        std::cout << "Killing this fetus..." << std::endl;
        ObjectX* abort_info = new ObjectX;
        abort_info->VN = -1;
        abort_info->DS = -1;
        abort_info->RU = -1;
        abort_info->server_id = m_designation;
        for (auto peer_id: m_peers) {
            m_peerToSockets[peer_id].send(abort_info, sizeof(ObjectX));
        }
    }
};



int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Args: <server_designation> <server_port>\n";
        return 1;
    }

    int port, designation;
    port = std::atoi(argv[1]);
    designation = std::atoi(argv[2]);
    Server server(port, designation);
    server.ControllerCommand();
    server.close();
    return 0;
}