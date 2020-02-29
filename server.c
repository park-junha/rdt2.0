//  RDT 2.0 Server
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include "rdt2.h"

int seq = 0;

int getChecksum(Packet packet) {
    packet.header.cksum = 0;

    int checksum = 0;
    char *ptr = (char *)&packet;
    char *end = ptr + sizeof(Header) + packet.header.len;
    
    while (ptr < end) {
        checksum ^= *ptr++;
    }

    return checksum;
}

void logPacket(Packet packet) {
    printf("Packet{ header: { seq_ack: %d, len: %d, cksum: %d }, data: \"",
        packet.header.seq_ack,
        packet.header.len,
        packet.header.cksum);
    fwrite(packet.data, (size_t)packet.header.len, 1, stdout);
    printf("\" }\n");
}

void ServerSend(int sockfd, const struct sockaddr *address, socklen_t addrlen, int seqnum) {
    Packet packet;
//  Prepare the packet headers
    packet.header.seq_ack = seqnum;
    packet.header.len = 0;
    packet.header.cksum = getChecksum(packet);
//  Send the packet
    sendto(sockfd, &packet, sizeof(packet), 0, address, addrlen);
    printf("Sent ACK %d, checksum %d\n", packet.header.seq_ack, packet.header.cksum);
}

Packet ServerReceive(int sockfd, struct sockaddr *address, socklen_t *addrlen, int seqnum) {
    Packet packet;

    while (1) {
        printf("Waiting to receive packet\n");
//      Receive packets from the client
        recvfrom(sockfd, &packet, sizeof(packet), 0, address, addrlen);

//      Log what was received
        printf("Received: ");
        logPacket(packet);

        if (packet.header.cksum != getChecksum(packet)) {
            printf("Bad checksum, expected checksum was: %d\n", getChecksum(packet));
            ServerSend(sockfd, address, *addrlen, !seqnum);
            sleep(10);
        } else if (packet.header.seq_ack != seqnum) {
            printf("Bad seqnum, expected sequence number was:%d\n", seqnum);
            ServerSend(sockfd, address, *addrlen, !seqnum);
            sleep(10);
        } else {
            printf("Good packet\n");
            ServerSend(sockfd, address, *addrlen, seqnum);
            break;
        }
    }

    printf("\n");
    return packet;
}


int main(int argc, char *argv[]) {
//  Check arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ip> <port> <outfile>\n", argv[0]);
        return EXIT_FAILURE;
    }

//  Create a socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("Failed to setup an endpoint socket");
        exit(1);
    }

//  Convert domain names to numerical IP address with DNS
    struct hostent *host;
    host = (struct hostent *)gethostbyname(argv[1]);

//  Initialize the server address structure 
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(struct sockaddr);

    address.sin_family = AF_INET;
    address.sin_port = htons(atoi(argv[2]));
    address.sin_addr = *((struct in_addr *)host->h_addr);

//  Bind the socket
    if ((bind(sockfd, (struct sockaddr *)&address, sizeof(struct sockaddr))) < 0)
    {
        perror("Failure to bind address to endpoint socket");
        exit(1);
    }

//  Open file 
    int fdout;
    fdout = open(argv[3], O_RDWR);

//  Get file contents from client
    Packet packet;
    struct sockaddr_in clientaddr;
    socklen_t clientaddr_len = sizeof(clientaddr);
    do {
        packet = ServerReceive(sockfd, (struct sockaddr*)&address, &addrlen, seq);
        write(fdout, packet.data, packet.header.len);
        seq = !seq;
    } while (packet.header.len != 0);

//  Clean up
    close(fdout);
    close(sockfd);
    return 0;
}
