#include "sdmessage.pb-c.h"
#include "tree.h"
#include "tree_skel.h"
#include "request_t-private.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include<unistd.h>


struct tree_t *tree;
int last_assigned;
struct request_t *queue_head;

pthread_t **threads;
size_t threads_amount = 0;

int CLOSE_PROGRAM = 0;

/* Inicia o skeleton da árvore. 
 * O main() do servidor deve chamar esta função antes de poder usar a 
 * função invoke().  
 * A função deve lançar N threads secundárias responsáveis por atender  
 * pedidos de escrita na árvore. 
 * Retorna 0 (OK) ou -1 (erro, por exemplo OUT OF MEMORY) 
 */
int tree_skel_init(int N){
    tree = tree_create();

    if(tree == NULL)
        return -1;

    threads_amount = N;

    if (threads_amount < 1)
    {
        return -1;
    }

    // printf("Vou iniciar as %ld threads\n", threads_amount);

    threads = malloc(sizeof(pthread_t *) * threads_amount);    

    for (int i = 0; i < threads_amount; i++) {
        threads[i] = malloc(sizeof(pthread_t));
        int id = i + 1;
        pthread_create(threads[i], NULL, process_request, (void *) (intptr_t) id);
    }

    return 0;
}

/* Função da thread secundária que vai processar pedidos de escrita. 
*/ 
void * process_request (void *params){
    int id = (intptr_t) params;
    //TODO - threads function

    while (1)
    {
        sleep(1);

        if (CLOSE_PROGRAM)
        {
            // printf("  Thread %d closing\n", id);
            pthread_exit(NULL);
        }
    }

    return 0;
}

/* Liberta toda a memória e recursos alocados pela função tree_skel_init.
 */
void tree_skel_destroy(){
    CLOSE_PROGRAM = 1;

    tree_destroy(tree);
    
    for (size_t i = 0; i < threads_amount; i++)
    {   
        pthread_join(*threads[i], NULL);
        free(threads[i]);
    }

    free(threads);
}

/* Executa uma operação na árvore (indicada pelo opcode contido em msg)
 * e utiliza a mesma estrutura message_t para devolver o resultado.
 * Retorna 0 (OK) ou -1 (erro, por exemplo, árvore nao incializada)
*/
int invoke(MessageT *msg) {
    MessageT__Opcode op = msg->opcode;

    char * key;
    struct data_t* data;


    switch(op) {
        case MESSAGE_T__OPCODE__OP_SIZE: ;
            printf("Requested: size\n");

            msg->opcode = MESSAGE_T__OPCODE__OP_SIZE + 1;
            msg->c_type = MESSAGE_T__C_TYPE__CT_RESULT;
            msg->result = tree_size(tree);
            return 0;

            break;

        case MESSAGE_T__OPCODE__OP_HEIGHT: ;
            printf("Requested: height\n");

            msg->result = tree_height(tree);
            msg->opcode = MESSAGE_T__OPCODE__OP_HEIGHT + 1;
            msg->c_type = MESSAGE_T__C_TYPE__CT_RESULT;
            return 0;
            break;

        case MESSAGE_T__OPCODE__OP_DEL: ;

            //TODO - assincronous del

            printf("Requested: del %s\n", msg->entry->key);

            //executa tree_del
            int del = tree_del(tree, msg->entry->key);

            //caso de erro em tree_del
            if(del == -1){
                printf("Error on Delete\n");
                return -1;
            }

            msg->opcode = MESSAGE_T__OPCODE__OP_DEL + 1;
            msg->c_type = MESSAGE_T__C_TYPE__CT_NONE;
            return 0;
            break;

        case MESSAGE_T__OPCODE__OP_GET: ;
            printf("Requested: get %s\n", msg->entry->key);

            data = tree_get(tree, msg->entry->key);

            //caso a key nao esteja presente
            if(data == NULL){
                msg->opcode = MESSAGE_T__OPCODE__OP_GET + 1;
                msg->c_type = MESSAGE_T__C_TYPE__CT_NONE;
                msg->entry->data.data = NULL;
                msg->entry->data.len = 0;
                return 0;
            }

            msg->opcode = MESSAGE_T__OPCODE__OP_GET + 1;
            msg->c_type = MESSAGE_T__C_TYPE__CT_VALUE;
            msg->entry->data.data = data->data;
            msg->entry->data.len = data->datasize;
            return 0;
            break;

        case MESSAGE_T__OPCODE__OP_PUT: ;

            //TODO - assincronous put

            printf("Requested: put %s %s\n", msg->entry->key, (char*)msg->entry->data.data);

            //cria data para tree_put
            key = malloc(strlen(msg->entry->key) + 1);
            strcpy(key, msg->entry->key);
            void * buf = malloc(msg->entry->data.len);
            memcpy(buf, msg->entry->data.data, msg->entry->data.len);
            data = data_create2(msg->entry->data.len, buf);

            //caso de erro em tree_put
            if(tree_put(tree,key,data) == -1 ){
                printf("Error on Put\n");
                return -1;
            }

            msg->opcode = MESSAGE_T__OPCODE__OP_PUT + 1;
            msg->c_type = MESSAGE_T__C_TYPE__CT_NONE;
            return 0;
            break;

        case MESSAGE_T__OPCODE__OP_GETKEYS: ;
            printf("Requested: getkeys\n");

            char** keys = tree_get_keys(tree);

            //caso arvore vazia
            if(keys == NULL){
                msg->opcode = MESSAGE_T__OPCODE__OP_BAD;
                msg->c_type = MESSAGE_T__C_TYPE__CT_BAD;
                return 0;
            }

            msg->opcode = MESSAGE_T__OPCODE__OP_GETKEYS + 1;
            msg->c_type = MESSAGE_T__C_TYPE__CT_KEYS;
            msg->n_keys = tree_size(tree);
            msg->keys = keys;

            return 0;
            break;

        case MESSAGE_T__OPCODE__OP_GETVALUES: ;
            printf("Requested: getvalues\n");

            void** datas = tree_get_values(tree);

            //caso arvore vazia
            if(datas == NULL){
                msg->opcode = MESSAGE_T__OPCODE__OP_BAD;
                msg->c_type = MESSAGE_T__C_TYPE__CT_BAD;
                return 0;
            }

            msg->n_values = tree_size(tree);
            msg->values = malloc(tree_size(tree) * sizeof(ProtobufCBinaryData*));

            int i = 0;
            while(datas[i] != NULL){
                ProtobufCBinaryData data_temp;
                data = (struct data_t*)datas[i];
                data_temp.len = data->datasize;
                data_temp.data = malloc(data->datasize);
                memcpy(data_temp.data, data->data, data->datasize);
                msg->values[i] = data_temp;
                i++;
            }

            msg->opcode = MESSAGE_T__OPCODE__OP_GETVALUES + 1;
            msg->c_type = MESSAGE_T__C_TYPE__CT_VALUES;

            return 0;
            break;

        case MESSAGE_T__OPCODE__OP_ERROR: ;
            printf("Received message signaling error.\n");
            return -1;
        default: ;
            printf("Request not recognised.\n");
        return -1;
    }

    return -1;
};

/* Verifica se a operação identificada por op_n foi executada. 
 */ 
int verify(int op_n){
    //TODO - verify
    return 0;
}