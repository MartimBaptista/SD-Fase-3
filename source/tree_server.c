#include "network_server.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]){

    if (argc != 3){
        printf("Uso: ./tree_server <porto_servidor> <N>\n");
        printf("Exemplo de uso: ./tree_server 12345 3\n");
        return -1;
    }

    int listening_socket_port = atoi(argv[1]);

    int listening_socket_fd = network_server_init(listening_socket_port);

    if(tree_skel_init(atoi(argv[2])) < 0){
        printf("Error Creating the tree\n");
        return -1;
    }

    network_main_loop(listening_socket_fd);

    return 0;
}