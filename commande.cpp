#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <csignal>

ssize_t send_message(int socket, const char * message, char* buffer, size_t bufferSize){
    write(socket, message, strlen(message));
    //read(socket, buffer, bufferSize-1);
    ssize_t n = read(socket, buffer, bufferSize-1);
    if (n >= 0) buffer[n] = '\0';
    return n;
}



int main(int argc, char* argv[]){
    signal(SIGPIPE, SIG_IGN);
    if (argc < 2){
        fprintf(stderr, "Usage: %s <commande> [arguments...]\n", argv[0]);
        fprintf(stderr, "Exemple: %s menu\n", argv[0]);
        return 1;
    }

    std::string request;
    request = "GET";
    for (int i = 1; i < argc; i++){
        request += " ";
        request += argv[i];
    }
    request += "\n";

    //On crée notre socket et on le parametre (port , adress, paramètres)
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        perror("socket");
        return 1;
    }


    struct sockaddr_in servr_addr;
    servr_addr.sin_family = AF_INET;
    servr_addr.sin_port = htons(3333);

    //On converti l'adress ip de string en binaire et on l'attribue a sin_addr
    inet_pton(AF_INET, "127.0.0.1", &servr_addr.sin_addr);

    //On se connecte
    if (connect(sock, (struct sockaddr*)&servr_addr, sizeof(servr_addr)) < 0){
        perror("connect");
        close(sock);
        return 1;
    }

    char buffer[256];

    ssize_t n = send_message(sock, request.c_str(), buffer, sizeof(buffer));
    if (n > 0){
    std::cout << buffer << std::endl;
    }
    else{
        std::cout << "ERREUR INCONNUE : VEUILLEZ REESAYER" << std::endl;
    }

    close(sock);


    return 0;
}