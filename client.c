//  RDT 2.0 Client
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
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

void ClientSend(int sockfd, const struct sockaddr *address, socklen_t addrlen, Packet packet) {
    while (1) {
//      Send the packet
        printf("Sending packet\n");
        logPacket(packet);
        sendto(sockfd, &packet, sizeof(packet), 0, address, sizeof(struct sockaddr));
        printf("Sent packet\n");
//      Receive an ACK from the server
        Packet recvpacket;
        printf("Waiting to receive ACK\n");
        recvfrom(sockfd, &recvpacket, 1024, 0, address, sizeof(struct sockaddr));
//      Log what was received
        printf("Received ACK %d, checksum %d - ", recvpacket.header.seq_ack, recvpacket.header.cksum);

//      Validate the ACK
        int recv_cksum = getChecksum(recvpacket);
        if (recvpacket.header.cksum != recv_cksum) {
//          Bad checksum, resend packet
            printf("Bad checksum, expected checksum was: %d\n", recv_cksum);
        } else if (recvpacket.header.seq_ack != seq) {
//          Incorrect sequence number, resend packet
            printf("Bad seqnum, expected sequence number was: %d\n", seq);
        } else {
//          Good ACK, we're done
            printf("Good ACK\n");
            seq = !seq;
            break;
        }
    }

    printf("\n");
}

int main(int argc, char *argv[]) {
//  Check arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ip> <port> <infile>\n", argv[0]);
        return 1;
    }

//  Initialize socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Failed to setup an endpoint socket");
        exit(1);
    }

//  Convert domain names to numerical IP address with DNS
    struct hostent *host;
    host = (struct hostent *)gethostbyname(argv[1]);

//  Initialize the server address structure
    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    socklen_t addrlen = sizeof(struct sockaddr);

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(atoi(argv[2]));
    servAddr.sin_addr = *((struct in_addr *)host->h_addr);

//  Open file
    int fdin, in;
    fdin = open(argv[3], O_RDWR);

//  Create a packet
	Packet p;

//  Send file contents to server
    while ((in = read(fdin, p.data, sizeof(p.data))) != 0)
    {
//  Prepare packet
        p.header.seq_ack = seq;
        p.header.len = in;
        p.header.cksum = getChecksum(p);
//  Send the packet
        ClientSend(sockfd, &servAddr, sizeof(struct sockaddr), p);
    }


//  Send zero-length packet to server to end connection
    p.header.len = 0;
    p.header.seq_ack = !p.header.seq_ack;
    p.header.cksum = getChecksum(p);
    ClientSend(sockfd, &servAddr, sizeof(struct sockaddr), p);

//  Clean up
    close(fdin);
    close(sockfd);

    return 0;
}
