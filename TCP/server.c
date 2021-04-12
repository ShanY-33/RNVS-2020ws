//
// Created by tkn on 12/1/20.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <time.h>


int count_lines_in_file(FILE* fd);
int get_random_line(int line);

int main(int argc, char* argv[]){
    int sock, connect_sock;
    char* port = argv[1];
    char* file = argv[2];

    struct addrinfo hints, *servInfo, *p;
    struct sockaddr_storage their_addr;
    int returnValue;
    int yes = 1;

    FILE* theFile = fopen(file, "r");
    if(theFile == NULL){
        printf("Not able to open the file.\n");
        return 1;
    }
    int length = count_lines_in_file(theFile);
    if(length == 0){
        fprintf(stderr, "The file is empty\n");
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((returnValue = getaddrinfo(NULL, port, &hints, &servInfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(returnValue));
        return 1;
    }

    for(p = servInfo; p != NULL; p = p -> ai_next){
        if((sock = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1){
            perror("server: socket");
            exit(1);
        }

        if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            perror("server: socket");
            exit(1);
        }

        if(bind(sock, p -> ai_addr, p -> ai_addrlen) == -1){
            close(sock);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servInfo);

    if(p == NULL){
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if(listen(sock, 7) == -1){
        perror("listen");
        exit(1);
    }

    printf("server: waiting for connections \n");

    while(1) {
        socklen_t sin_size = sizeof(their_addr);
        connect_sock = accept(sock, (struct sockaddr *) &their_addr, &sin_size);
        if (connect_sock == -1) {
            perror("accept");
            continue;
        }
        printf("Connected with client\n");

        char* buffer;
        int randNum = get_random_line(length);

        rewind(theFile);
        int counter = 0;
        size_t len = 0;
        while((len = getline(&buffer, &len, theFile)) != -1) {
            if (counter == randNum) {
                break;
            } else {
                counter++;
            }
        }

        if (send(connect_sock, buffer, len-1, 0) == -1) {
            perror("send");
            exit(1);
        }
        printf("Message sent to client\n");

        close(connect_sock);
        printf("server is closing\n");
    }

    fclose(theFile);
    return 0;

}

int count_lines_in_file(FILE* fd){
    rewind(fd);
    int counter = 0;
    char ch;
    while((ch = fgetc(fd)) != EOF){
        if(ch == '\n'){
            counter++;
        }
    }
    return counter;
}

int get_random_line(int line){
    srand((unsigned) time(NULL));
    return((int)rand() % line);
}


