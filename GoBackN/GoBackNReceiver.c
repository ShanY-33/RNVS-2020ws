/***************************************************************************
 *
 * Copyright:   (C)2004-2021 Telecommunication Networks Group (TKN) at
 *              Technische Universitaet Berlin, Germany.
 *
 * Authors:     Lars Westerhoff, Guenter Schaefer, Daniel Happ,
 *              Sebastian Bräuer
 *
 **************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <inttypes.h>

#include <sys/socket.h>

#include "GoBackNMessageStruct.h"
#include "SocketConnection.h"

#define DEBUG
#ifdef DEBUG
#define DEBUGOUT(x, ...) fprintf(stderr, x, __VA_ARGS__)
#else
#define DEBUGOUT(x, ...)
#endif

#define DEFAULT_LOCAL_PORT "12105"
#define DEFAULT_PAYLOAD_SIZE 1024

char *localPort;
char *fileName;

long lastReceivedSeqNo;
size_t goodBytes, totalBytes;
struct sockaddr *cliaddr;
socklen_t len;

void help(int exitCode) {
    fprintf(stderr, "GoBackNReceiver [--local|-l port] file\n");
    exit(exitCode);
}

void initialize(int argc, char **argv) {
    localPort = DEFAULT_LOCAL_PORT;

    while (1) {
        static struct option long_options[] = {
                {"local", 1, NULL, 'l'},
                {"help",  0, NULL, 'h'},
                {0,       0, 0,    0}};

        int c = getopt_long(argc, argv, "l:h", long_options, NULL);
        if (c == -1) break;

        switch (c) {
            case 'l':
                localPort = optarg;
                break;

            case 'h':
                help(0);
                break;

            case '?':
                help(1);
                break;

            default:
                printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

    if (argc < optind + 1) help(1);

    fileName = argv[optind];

    lastReceivedSeqNo = -1;
    goodBytes = totalBytes = 0;
}

void writeBuffer(FILE *file, GoBackNMessageStruct *packet) {
    size_t count = packet->size - sizeof(*packet);
    size_t retval;
    if ((retval = fwrite(packet->data, 1, count, file)) < count) {
        if (ferror(file)) {
            perror("fread");
            exit(1);
        } else {
            fprintf(stderr, "WARNING: Could not write complete packet to file!\n");
        }
    }
    DEBUGOUT("FILE: %zu bytes written\n", retval);
}

void sendAck(int s, GoBackNMessageStruct *packet, long expected) {
    GoBackNMessageStruct *ack = allocateGoBackNMessageStruct(0);
    ack->seqNo = -1;
    ack->seqNoExpected = expected;
    ack->size = sizeof(*ack);
    ack->crcSum = 0;
    ack->crcSum = crcGoBackNMessageStruct(ack);

    int retval;
    if ((retval = sendto(s, ack, ack->size, 0, cliaddr, len)) < 0) {
        perror("send");
        exit(1);
    }
    DEBUGOUT("SOCKET: %d bytes sent\n", retval);
    freeGoBackNMessageStruct(ack);
}

int main(int argc, char **argv) {
    socklen_t addrlen;

    initialize(argc, argv);

    // open file
    FILE *output = fopen(fileName, "wb");
    if (output == NULL) {
        perror("fopen");
        exit(1);
    }

    // prepare channel to receiver
    int s = udp_server(NULL, localPort, &addrlen);
    if (s < 0) {
        exit(1);
    }
    cliaddr = malloc(addrlen);
    len = addrlen;

    ssize_t bytesRead = 0;
    while (1) {
        bool crcValid = false;
        uint32_t tmpCRC = 0;

        GoBackNMessageStruct *data =
                allocateGoBackNMessageStruct(DEFAULT_PAYLOAD_SIZE);
        if (recvfrom(s, data, sizeof(*data), MSG_PEEK, cliaddr, &len) < 0) {
            perror("recv(MSG_PEEK)");
            exit(1);
        }

        if (data->size > (sizeof(GoBackNMessageStruct) + DEFAULT_PAYLOAD_SIZE)) {
            data->size = sizeof(GoBackNMessageStruct) + DEFAULT_PAYLOAD_SIZE;
        }

        bytesRead = recvfrom(s, data, data->size, 0, cliaddr, &len);
        if (bytesRead < 0) {
            perror("recv");
            exit(1);
        }
        if (bytesRead < data->size) {
            fprintf(stderr, "WARNING: Truncated read\n");
            data->size = bytesRead;
        }

        DEBUGOUT("SOCKET: %zd bytes received.\n", bytesRead);

        if (bytesRead == 0) {
            freeGoBackNMessageStruct(data);
            break;
        }

        data->size = bytesRead;
        totalBytes += bytesRead - sizeof(*data);

        // check if CRC is valid
        tmpCRC = data->crcSum;
        data->crcSum = 0;
        crcValid = true;
        crcValid = (tmpCRC == crcGoBackNMessageStruct(data));

        DEBUGOUT("#%d, size: %u, CRC: %u\n", data->seqNo, data->size, tmpCRC);

        /* YOUR TASK: (done) */
        if (crcValid == true && data->seqNo == lastReceivedSeqNo + 1){
          sendAck(s, data, lastReceivedSeqNo + 2);
          lastReceivedSeqNo++;
          goodBytes += data->size - sizeof(*data);

          // Wenn folgender Fall eintritt, wurde die Datei
          // komplett uebertragen und wir koennen das Programm
          // beenden
          if (data->size == sizeof(*data)){
            fclose(output);
            printf("Total bytes: %zu\nGood bytes: %zu\n\n", totalBytes, goodBytes);
            freeGoBackNMessageStruct(data);
            close(s);
            free(cliaddr);
            exit(0);
          }
          writeBuffer(output, data);
        } else {
          sendAck(s, data, lastReceivedSeqNo + 1);
        }
        /* END YOUR TASK (done) */

        freeGoBackNMessageStruct(data);
    }
    free(cliaddr);
}
