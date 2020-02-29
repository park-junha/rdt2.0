//  Header structure
typedef struct {
    int seq_ack;
    int len;
    int cksum;
} Header;

//  Packet structure
typedef struct {
    Header header;
    char data[10];
} Packet;
