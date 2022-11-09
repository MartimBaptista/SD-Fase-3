#include "inet.h"
#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include "sdmessage.pb-c.h"
#include "tree_skel.h"

/* Função para preparar uma socket de receção de pedidos de ligação
 * num determinado porto.
 * Retornar descritor do socket (OK) ou -1 (erro).
 */
int network_server_init(short port){
    int listening_socket;
    struct sockaddr_in server;

    // Cria socket TCP
    if ((listening_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        perror("Erro ao criar socket");
        return -1;
    }

    // Fazer com que possa ser reutilisado
    const int enable = 1;
    if (setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
        perror("Erro no Setsockopt(SO_REUSEADDR):");
        return -1;
    }
    if (setsockopt(listening_socket, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0){
        perror("Erro no Setsockopt(SO_REUSEPORT):");
        return -1;
    }

    // Preenche estrutura server com endereço(s) para associar (bind) à socket 
    server.sin_family = AF_INET;
    server.sin_port = htons(port); // Porta TCP
    server.sin_addr.s_addr = htonl(INADDR_ANY); // Todos os endereços na máquina

    // Faz bind
    if (bind(listening_socket, (struct sockaddr *) &server, sizeof(server)) < 0){
        perror("Erro ao fazer bind");
        close(listening_socket);
        return -1;
    }

    // Esta chamada diz ao SO que esta é uma socket para receber pedidos
    if (listen(listening_socket, 0) < 0){
        perror("Erro ao executar listen");
        close(listening_socket);
        return -1;
    }

    return listening_socket;
}

/* Função para ler toda uma messagem num determinado porto.
 * Retornar o tamanho total lido (OK) ou <0 (erro).
 */
int read_all(int sock, void *buf, int len){
    int bufsize = len;
    while(len>0) {
        int res = read(sock, buf, len);
        if(res<0) {
            if(errno==EINTR) continue;
            perror("read failed:");
            return res;
        }
    buf += res;
    len -= res;
    }
    return bufsize;
}

/* Função para escrever toda uma messagem num determinado porto.
 * Retornar o tamanho total escrito (OK) ou <0 (erro).
 */
int write_all(int sock, void *buf, int len){
    int bufsize = len;
    while(len>0) {
        int res = write(sock, buf, len);
        if(res<0) {
            if(errno==EINTR) continue;
            perror("write failed:");
            return res;
        }
    buf += res;
    len -= res;
    }
    return bufsize;
}


/* Esta função deve:
 * - Ler os bytes da rede, a partir do client_socket indicado;
 * - De-serializar estes bytes e construir a mensagem com o pedido,
 *   reservando a memória necessária para a estrutura message_t.
 */
MessageT *network_receive(int client_socket){
    int size, size_n;

    if(read_all(client_socket, &size_n, sizeof(size_n)) < 0){
		perror("Erro ao receber tamnaho dos dados do cliente");
		close(client_socket);
    }

    size = ntohl(size_n);
    printf("Expecting to receive message of size: %d\n", size);
    uint8_t buf [size];

    if(read_all(client_socket, buf, size) < 0){
		perror("Erro ao receber dados do cliente");
		close(client_socket);
    }

    printf("Received message\n");

    MessageT *res; 
    res = message_t__unpack(NULL, size, buf);

    return res;
}


/* Esta função deve:
 * - Serializar a mensagem de resposta contida em msg;
 * - Libertar a memória ocupada por esta mensagem;
 * - Enviar a mensagem serializada, através do client_socket.
 */
int network_send(int client_socket, MessageT *msg){

    int size = message_t__get_packed_size(msg);
    int size_n = htonl(size);

    uint8_t *buf = malloc(size);
    message_t__pack(msg, buf);

    if(write_all(client_socket, &size_n, sizeof(int)) < 0){
        perror("Erro ao enviar tamanho da resposta ao cliente");
        close(client_socket);
    }

    if(write_all(client_socket, buf, size) < 0){
        perror("Erro ao enviar resposta ao cliente");
    	close(client_socket);
    }

    return 0;
}

/* Esta função deve:
 * - Aceitar uma conexão de um cliente;
 * - Receber uma mensagem usando a função network_receive;
 * - Entregar a mensagem de-serializada ao skeleton para ser processada;
 * - Esperar a resposta do skeleton;
 * - Enviar a resposta ao cliente usando a função network_send.
 */
int network_main_loop(int listening_socket){

    int nfdesc = 5;
    int timeout = 60;

    struct sockaddr_in client;
    socklen_t size_client = sizeof(client);
    

    struct pollfd desc_set[nfdesc]; // numero maximo de cliente concorrentes

    for(int i = 0; i < nfdesc; i++)
        desc_set[i].fd = -1;

    desc_set[0].fd = listening_socket; //adiciona listening_socket a desc_set
    desc_set[0].events = POLLIN; 

    int nfds = 1; //numero de file descriptors

    while(poll(desc_set, nfds, timeout) >= 0){
        /* printf("Server listening...\n"); */
        if ((desc_set[0].revents & POLLIN) && (nfds < nfdesc)) {
            if((desc_set[nfds].fd = accept(desc_set[0].fd,(struct sockaddr *) &client, &size_client)) > 0){
                printf("Client number: %d connected\n", nfds);
                desc_set[nfds].events = POLLIN; //wait for data
                nfds++;
            }
        }

        for(int i = 1; i < nfds; i++){
            if(desc_set[i].revents & POLLIN) {
                MessageT *msg  = network_receive(desc_set[i].fd);

                if(msg->opcode == MESSAGE_T__OPCODE__OP_DISCONNECT) {
                    printf("Client number: %d disconnected\n",i);
                    close(desc_set[i].fd);
                    desc_set[i].fd = -1;
                } else {
                    if(invoke(msg) < 0) {
                        printf("Error on invoke\n");
                        msg->opcode = MESSAGE_T__OPCODE__OP_ERROR;
                        msg->c_type = MESSAGE_T__C_TYPE__CT_NONE;
                    }

                    if(network_send(desc_set[i].fd, msg) == -1) {
                        close(desc_set[i].fd);
                        return -1;
                    }
                }
            }
        }

        for(int i = 1 ; i < nfds ;i++){  //allows new connections when clients disconnect
            if(desc_set[i].fd == -1){
                desc_set[i].fd = desc_set[nfds].fd;
                nfds --;
            }
        }
    }
    close(listening_socket);
    return 0;

    /* TODO
     *                              --Esboço do algoritmo a ser implementado--
     *
     * adiciona listening_socket a desc_set.                            //desc_set corresponde a um conjunto de file descriptors
     *
     * while (poll(desc_set) >= 0) {                                     //Espera por dados nos sockets abertos 
     *   if (listening_socket tem dados para ler) {                      //Verifica se tem novo pedido de conexão
     *     connsockfd = accept(listening_socket); 
     *     adiciona connsockfd a desc_set 
     *   } 
     *   for (all socket s em desc_set, excluindo listening_socket) {    //Verifica restantes sockets 
     *     if (s tem dados para ler) { 
     *       message = network_receive(s); 
     *       if (message é NULL) {                                       //Sinal de que a conexão foi fechada pelo cliente  
     *         close(s); 
     *         remove s de desc_set 
     *       } else { 
     *         invoke(message);                                          //Executa pedido contido em message 
     *         network_send(message);                                    //Envia resposta contida em message 
     *       } 
     *     } 
     *     if (s com erro ou POLLHUP) { 
     *       close(s); 
     *       remove s de desc_set 
     *     } 
     *   } 
     * } 
     */ 
} 

/* A função network_server_close() liberta os recursos alocados por
 * network_server_init().
 */
int network_server_close(){
    printf("\nClosing server.\n");
    return 0;
}
