#include <iostream>
#include <iomanip>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <random>
#include <sstream>
#include <boost/program_options.hpp>
#include <map>

#define FIXED 0
#define RANDOM_EXP 1
#define RANDOM_FIXED_PLUS_EXP 2

#define SEQNR_TYPE long int

#define MAXBUF 70000

namespace po = boost::program_options;

int openSocket()
{
    int fd = socket(AF_INET,SOCK_DGRAM,0);
    if(fd<0){
        perror("Cannot open socket");
    }
    return fd;
}


bool udpSend(int fd, sockaddr_in serveraddr, const char *msg, int size)
{
    if (sendto(fd, msg, size, 0,
               (sockaddr*)&serveraddr, sizeof(serveraddr)) < 0){
        perror("Cannot send message");
        return false;
    }
    return true;
}

int main(int argc, char *argv[])
{
    const char * host_name;
    std::string tmp_host_name;
    int port, num_packets, packet_size, mode;
    float ratio, interval;
    long startTime;
    SEQNR_TYPE packetID = 0;
    std::map<SEQNR_TYPE, timespec> sendMap, receiveMap;

    try
    {
        po::options_description desc("Allowed options");
        desc.add_options()
        ("help,h", "Display this help screen")
        ("address,a", po::value<std::string>(&tmp_host_name), "Destination host IP address")
        ("port,p", po::value<int>(&port)->default_value(1234), "Destination port number (default 1234)")
        ("num-packets,n", po::value<int>(&num_packets)->default_value(5000), "Number of UDP packets to transmit (default 5000)")
        ("packet-size,s", po::value<int>(&packet_size)->default_value(50), "Size, in Byte, of each UDP packet (default 50 Byte, conains random ASCII characters)")
        ("interval,i", po::value<float>(&interval)->default_value(20), "Interval, in milliseconds, between sending packets (default 20.0 milliseconds, mean value if randomness is used)")
        ("mode,m", po::value<int>(&mode)->default_value(0), "Distribution of interval between packets: 0: Constant, 1: Exponetially distributed, 2: Constant with ratio r, adding exponentially distributed random component with ratio 1 - r")
        ("ratio,r", po::value<float>(&ratio)->default_value(0.9), "Ratio of constant component for Mode 2 (default 0.9)");

        po::positional_options_description p;
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::cout << "Usage: options_description [options]\n";
            std::cout << desc;
            return 0;
        }

        if (vm.count("address"))
        {
            host_name = tmp_host_name.c_str();
        }
        else
        {
            perror("Host IP address is required!\n");
            return -1;
        }

        if(mode == FIXED)
            std::cout << "Sending interval between packets: Constant\n";
        else if(mode == RANDOM_EXP)
            std::cout << "Sending interval between packets: Exponetially distributed\nWARNING: This may result in small spaces between some packets, causing self-queueing that could be falsely interpreted as network delay. Consider using Mode 2 instead.";
        else if(mode == RANDOM_FIXED_PLUS_EXP)
        {
            if( (ratio < 0.0) || (ratio > 1.0) )
            {
                perror("Ratio must be between 0 and 1!\n");
                return -1;
            }
            else
                std::cout << "Sending interval between packets: Constant with ratio " << ratio << ", adding exponentially distributed random component with ratio " << 1 - ratio << "\n";
        }
        else
        {
            perror("Mode must be '0', '1', or '2':  0: Constant, 1: Exponetially distributed, 2: Constant with ratio r, adding exponentially distributed random component with ratio 1 - r\n");
            return -1;
        }
        if(packet_size < sizeof(SEQNR_TYPE))
        {
            std::cout << "Minimum packet size is " << sizeof(SEQNR_TYPE) << " to fit sequence number\n";
            return -1;
        }

        std::cout << "Destination host IP address: " << host_name << "\n";
        std::cout << "Destination port number: " << port << "\n";
        std::cout << "Number of UDP packets to transmit: " << num_packets << "\n";
        std::cout << "Size of each UDP packet: " << packet_size << " Byte\n";
        std::string strInt = (mode != FIXED)?"Mean interval":"Interval";
        std::cout << strInt << " between sending packets: " << interval << " ms\n\n";
    }
    catch(std::exception& e)
    {
        std::cout << e.what() << "\n";
        return 1;
    }


    struct timespec start, now, tim;
    tim.tv_sec = 0;
    tim.tv_nsec = interval * 1E6;
    float rate = 1.0;

    std::default_random_engine generator;
    if(mode == RANDOM_EXP)
        rate = 1.0/float(interval);
    if(mode == RANDOM_FIXED_PLUS_EXP)
        rate = 1.0/(float(interval) * (1.0 - ratio));

    std::exponential_distribution<double> distribution(rate);

    sockaddr_in servaddr;
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(host_name);
    servaddr.sin_port = htons(port);

    char *msg = (char*)malloc(sizeof(char) * (packet_size + 1));
    for(int i = 0; i < packet_size; i++)
        msg[i] = rand() % 57 + 65; // Printable ASCII chars for easier debug

    std::cout << "Opening socket\n";
    int fd = openSocket();
    if(fd < 0)
        return -1;

    std::cout << "Socket is open\n";
    std::cout << std::fixed << std::fixed << std::setprecision(1) << "Starting to send at " 
              << 1E-3 / interval * packet_size * 8 << " Mbit/s\n";
    std::cout << "Estimated experiment duration: " 
              << float(num_packets) / (1000.0 / interval) << " seconds\n";

    clock_gettime(CLOCK_REALTIME, &start);
    startTime = start.tv_nsec + 1E9 * start.tv_sec;

    for(SEQNR_TYPE i = 0; i < num_packets; i++)
    {
        clock_gettime(CLOCK_REALTIME, &start);

        memcpy(msg, &i, sizeof(SEQNR_TYPE)); // Copy the sequence number to the beginning of the message
        udpSend(fd, servaddr, msg, packet_size);

        if(mode == RANDOM_EXP)
            tim.tv_nsec = distribution(generator) * 1E6;
        if(mode == RANDOM_FIXED_PLUS_EXP)
            tim.tv_nsec = distribution(generator) * 1E6 + (1E6 * float(interval) * ratio);

        do
        {
            clock_gettime(CLOCK_REALTIME, &now);
        }while((now.tv_nsec + now.tv_sec * 1E9) - (start.tv_nsec + start.tv_sec * 1E9) < (tim.tv_nsec + tim.tv_sec * 1E9));
        //std::cout << std::fixed << (now.tv_nsec + now.tv_sec * 1E9) - (start.tv_nsec + start.tv_sec * 1E9) << "\n";
    }
    float duration = float(now.tv_nsec + now.tv_sec * 1E9 - startTime) / 1E9;
    long totalBit = long(num_packets) * long(packet_size) * 8.0;

    std::cout << "Experiment took " << duration << " seconds.\n";
    std::cout <<  std::fixed << std::setprecision(3) 
              << "Average sending throughput: " << float(totalBit) / 1E6 
              << " MBit / " << duration << " s = " << float(totalBit) / duration / 1E6 
              << " Mbit/s. (If lower than what was configured, client was unable to send faster)\n";
    std::cout << "Client finished. Press Ctrl+C on server to print result.\n";
    free(msg);
    close(fd);
    return 0;
}

