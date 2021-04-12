#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>


int main(int argc, char* argv[]){
    int sock;
    char* servName = argv[1];
    char* servPort = argv[2];
    char buffer[512];
    struct addrinfo hints, *servInfo;

    if(argc < 3){
        fprintf(stderr, "address and port of server as parameter expected");
        exit(1);
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int returnValue = getaddrinfo(servName, servPort, &hints, &servInfo);
    if(returnValue != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(returnValue));
    }

    //socket
    if ((sock = socket(servInfo->ai_family, servInfo->ai_socktype, 0)) < 0){
        perror("Error: socket creation failed!");
        exit(1);
    }

    //connect
    if(connect(sock, servInfo->ai_addr, servInfo->ai_addrlen) < 0){
        perror("Error: connection failed!");
        exit(1);
    }

    //recv
    int msg_size;
    while((msg_size = recv(sock, buffer, sizeof(buffer), 0)) > 0){
        fwrite(buffer, sizeof(char), msg_size, stdout);
    }

    //close
    close(sock);
    freeaddrinfo(servInfo);

    return 0;

}