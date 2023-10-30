// Server side implementation of UDP client-server model
#include <bits/stdc++.h>
#include <stdlib.h>
#include <map>
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>

#define PORT 1234
#define MAXBUF 70000    // Allowing jumb frames of ~65 kByte
#define SEQNR_TYPE long int

std::map<SEQNR_TYPE, std::pair<timespec, int>> received;

void printResult()
{
    std::map<SEQNR_TYPE, std::pair<timespec, int>>::iterator it;
    std::cout << "\n";
    for(it = received.begin(); it != received.end(); it++)
    {
       std::cout << std::fixed << std::setprecision(0) << it->first << "; " << it->second.first.tv_sec * 1E9 + it->second.first.tv_nsec << "; " << it->second.second << "\n";
    }
}

void printPerSecondResult()
{
    std::map<SEQNR_TYPE, std::pair<timespec, int>>::iterator it;
    std::cout << "\n";
    long lastTime, thisTime, passedTime, timeSum, bitSum, lost, startTime, allBit;
    SEQNR_TYPE lastID, thisID, count;

    // Init with values from first entry
    it = received.begin();
    lastTime = it->second.first.tv_sec * 1E9 + it->second.first.tv_nsec;
    bitSum = it->second.second;
    lastID = it->first;
    timeSum = 0;
    lost = 0;
    count = 1;
    it++;
    startTime = lastTime;
    allBit = 0;

    for( ; it != received.end(); it++)
    {
       thisID = it->first;
       thisTime = it->second.first.tv_sec * 1E9 + it->second.first.tv_nsec;
       passedTime = thisTime - lastTime;
       timeSum += passedTime;
       lastTime = thisTime;

       if(timeSum >= 1E9)
       {
           std::cout << std::fixed << std::setprecision(3) << float(bitSum) / 1E6 << " Mbit/s " << count << " packets\n";
           
           lastTime = thisTime;
           bitSum = it->second.second;
           timeSum -= 1E9;
           count = 1;
       }
       else
       { 
           bitSum += it->second.second;
           count++;
       }
       lost += (thisID - lastID - 1);
       lastID = thisID;
       allBit += it->second.second;
    }
    std::cout << std::fixed << std::setprecision(3) 
              << lost << " of " << received.size() + lost << " (" 
              << 100.0 * float(lost)/float(received.size() + lost)  
              << "\%) packets lost. Experiment took " << float(lastTime - startTime) / 1E9  
              << " seconds.\n"
              << "Average receiving throughput: " << float(allBit) / 1E6 / (float(lastTime - startTime) / 1E9) << " Mbit/s\n";
}

void
inthand(int signum)
{
    if(received.size())
        printPerSecondResult();
    exit(0);
}

int main(int argc, char *argv[])
{
    int port, packet_size;
    long int count = 0;
    struct timespec now;
    SEQNR_TYPE seqnr;

    if(argc > 2)
    {
        std::cout << "\nToo many input parameters\n\n";
        std::cout << "How to use udpServer:\n";
        std::cout << argv[0] << "\t";
        std::cout << "[PORT]\nDefault port number is " << PORT << "\n";
        std::cout << "Terminating the program\n\n";

        return -1;
    }
    if(argc == 2)
        std::istringstream ss1(argv[1]);
    if(argc == 1)
        port = PORT;

    std::cout << "Port: " << port << "\n";

    int sockfd;
    char buffer[MAXBUF];
    struct sockaddr_in servaddr, cliaddr;

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) 
    {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
    }

    // Filling server information
    servaddr.sin_family    = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    // Bind the socket with the server address
    if(bind(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("Binding to socket failed");
        exit(EXIT_FAILURE);
    }

    socklen_t len;
    int n = 0;

    // Setting Ctrl+C handler for exit
    signal(SIGINT, inthand);

    clock_gettime(CLOCK_REALTIME, &now);
    long start = now.tv_nsec + now.tv_sec * 1E9;

    while(1)
    {
        len = sizeof(cliaddr);

        n = recvfrom(sockfd, (char*)buffer, MAXBUF, 0, ( struct sockaddr*) &cliaddr, &len);

        memcpy(&seqnr, buffer, sizeof(SEQNR_TYPE));
	clock_gettime(CLOCK_REALTIME, &now);
	//std::cout << std::fixed << std::setprecision(0) << seqnr << "; " << now.tv_nsec + now.tv_sec * 1E9 - start << "; " << n * 8 << "\n";
        received[seqnr].first = now;
        received[seqnr].second = n * 8;
    }
    return 0;
}

