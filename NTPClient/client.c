#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <limits.h>

#define NTP_PORT "123"
#define UNIX_OFFSET 2208988800L

#define MAX_BYTES 48

// li = 0 vn = 4 mode = 3 00 100 011
#define FLAG 0x23
#define SHIFT_MASK_64 ((uint64_t) 1 << 32)
#define SHIFT_MASK_32 ((uint32_t) 1 << 16)


typedef struct NTPInfo{
    char* host;
    int n;
    float rootDispersion;
    long double delay;
    long double offset;
    long double rtt;
}NTPInfo;

void max_min_rtt(long double* max, long double* min, const long double* list, int n){
    long double tmp_max = list[0], tmp_min = list[0];

    for (int i = 0; i < n+1; i++) {
        if (list[i] > tmp_max){
            tmp_max = list[i];
        }
        if (list[i] < tmp_min){
            tmp_min = list[i];
        }
    }

    *max = tmp_max;
    *min = tmp_min;
}

unsigned char* new_packet(){
    unsigned char* buf = (unsigned char*) calloc(MAX_BYTES, 1);

    *buf = FLAG;

    return buf;
}

void netToHost(void* dest, void* src, unsigned int length){
    unsigned char* tmp_src = (unsigned char*) src;
    unsigned char* tmp_dest = (unsigned char*) dest;

    for (unsigned int i = 0; i < length; i++) {
        tmp_dest[i] = tmp_src[length-1-i];
    }
}

void decode_package(unsigned char* rsp, long double* t2, long double* t3, float* rootDispersion){
    uint64_t tmp_t2, tmp_t3;
    uint32_t tmp_rootDisp;
    netToHost(&tmp_rootDisp, rsp+8, 4);
    netToHost(&tmp_t2, rsp+32, 8);
    netToHost(&tmp_t3, rsp+40, 8);

    // convert from utp to unix time
    //tmp_t2 += (uint64_t) UNIX_OFFSET << 32;
    //tmp_t3 += (uint64_t) UNIX_OFFSET << 32;
    // convert fixed-point to double (hopefully with enough precision)
    *t2 = ((long double) tmp_t2 / (long double) SHIFT_MASK_64);
    *t3 = ((long double) tmp_t3 / (long double) SHIFT_MASK_64);

    *rootDispersion = ((float) tmp_rootDisp / (float) SHIFT_MASK_32);

}

void getData(NTPInfo *data, long double* delays){
    delays[data->n] = data->rtt;

    long double max, min;
    max_min_rtt(&max, &min, delays, data->n);

    //double dispersion = max_delay - min_delay;
    long double dispersion = max - min;

    printf("%s;%d;%lf;%Lf;%Lf;%Lf\n", data->host, data->n, data->rootDispersion, dispersion, data->delay, data->offset);
}

void every8sec(int sockfd, struct addrinfo *p, NTPInfo *data){
    struct timespec start, end;
    int numbytes;

    // get the time when request is sent
    clock_gettime(CLOCK_REALTIME, &start);
    //double t1 = (double) start.tv_sec + (( (double) start.tv_nsec) / pow((double)10, (double) -8));
    double t1 = (double) start.tv_sec + ((double) start.tv_nsec / (double)1000000000);

    // create request
    unsigned char* request = new_packet();

    // send request
    numbytes = sendto(sockfd, request, MAX_BYTES, 0, p->ai_addr, p->ai_addrlen);
    if (numbytes == -1){
        perror("send problem");
        exit(1);
    }
    free(request);

    // receive response
    unsigned char* response = (unsigned char*) calloc(MAX_BYTES, 1);

    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;


    numbytes = recvfrom(sockfd, response, MAX_BYTES, 0, (struct sockaddr *) &their_addr, &addr_len);
    if (numbytes == -1) {
        perror("recvfrom");
        exit(1);
    }

    if (numbytes == 0){
        numbytes = recvfrom(sockfd, response, MAX_BYTES, 0, (struct sockaddr *) &their_addr, &addr_len);
        if (numbytes == -1) {
            perror("recvfrom");
            exit(1);
        }
    }

    // get the time when response is received from the server
    clock_gettime(CLOCK_REALTIME, &end);
    //double t4 = (double) end.tv_sec + (( (double) end.tv_nsec) / pow((double)10, (double) -9));
    double t4 = (double) end.tv_sec + ((double) end.tv_nsec / (double)1000000000);

    // decode the response
    long double t2, t3;
    float rootDispersion;
    decode_package(response, &t2, &t3, &rootDispersion);

    //t2 += UNIX_OFFSET;
    //t3 += UNIX_OFFSET;
    long double delay = 0.5 * ((t4 - t1) - (t3 - t2));
    data->rtt = (t4 - t1) - (t3 - t2);

    t2 -= UNIX_OFFSET;
    t3 -= UNIX_OFFSET;
    long double offset = ((t2 - t1) + (t3 - t4)) / 2;


    data->rootDispersion = rootDispersion;
    data->delay = delay;
    data->offset = offset;

    free(response);

}

int main(int argc, char** argv) {
    if (argc < 1){
        fprintf(stderr, "You have to, at least, state the number of server you want to communicate to. "
                        "If the number is higher than 0, then you have to state the hostname and ip address of the server\n");
        exit(0);
    }
    int numberRequest = atoi(argv[1]);
    long double delays[numberRequest];

    int numberServer = argc - 2;
    if (numberServer == 0){
        fprintf(stderr, "You did not state a server to connect to\n");
        exit(0);
    }

    char* server[numberServer];
    for (int i = 0; i < numberServer; i++) {
        server[i] = argv[i+2];
    }

    for (int i = 0; i < numberServer; i++) {
        int sockfd;
        struct addrinfo hints, *servinfo, *p;
        int rv;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        if ((rv = getaddrinfo(server[i], NTP_PORT, &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            exit(0);
        }

        for (p = servinfo; p != NULL; p = servinfo->ai_next) {
            sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sockfd == -1) {
                perror("socket problem");
                continue;
            }
            break;
        }

        if (p == NULL) {
            fprintf(stderr, "Failed to create socket\n");
            exit(2);
        }

        // send n requests to the server
        for (int j = 0; j < numberRequest; j++) {
            NTPInfo* data = (NTPInfo*) malloc(sizeof(NTPInfo));
            data->host = server[i];
            data->n = j;

            every8sec(sockfd, p, data);

            getData(data, delays);

            free(data);

            sleep(8);
        }
        close(sockfd);
        freeaddrinfo(servinfo);
    }

    return 0;
}
