#include "sdmessage.pb-c.h"
#include "tree.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct request_t { 
    int op_n;               //o número da operação 
    int op;                 //a operação a executar. op=0 se for um delete, op=1 se for um put 
    char* key;              //a chave a remover ou adicionar 
    struct data_t* data;    //os dados a adicionar em caso de put, ou NULL em caso de delete 
    struct request_t* next; //o proximo request (linked list)
    //adicionar campo(s) necessário(s) para implementar fila do tipo produtor/consumidor 
};

struct op_proc_t {
    int max_proc;
    int *in_progress;
};

struct tree_t *tree;

int last_assigned;
struct request_t *queue_head;
struct op_proc_t op_proc;

pthread_mutex_t queue_lock;
pthread_mutex_t tree_lock;
pthread_mutex_t op_proc_lock;
pthread_cond_t queue_not_empty;


/* Inicia o skeleton da árvore. 
 * O main() do servidor deve chamar esta função antes de poder usar a 
 * função invoke().  
 * A função deve lançar N threads secundárias responsáveis por atender  
 * pedidos de escrita na árvore. 
 * Retorna 0 (OK) ou -1 (erro, por exemplo OUT OF MEMORY) 
 */
int tree_skel_init(int N){
    //Creating tree
    tree = tree_create();

    //Reserving in_progress array and setting last_assigned and max_proc
    last_assigned = 1;
    op_proc.max_proc = 0;
    op_proc.in_progress = malloc(sizeof(int) * N);

    //Initialising locks
    pthread_mutex_init(queue_lock, NULL);
    pthread_mutex_init(tree_lock, NULL);
    pthread_mutex_init(op_proc_lock, NULL);
    pthread_cond_init(&queue_not_empty, NULL);

    //TODO: crete threads

    if(tree == NULL)
        return -1;
    return 0;
}

void queue_add_task(struct request_t *request){
    pthread_mutex_lock(&queue_lock);
    if (queue_head == NULL){ /* Adiciona na cabeça da fila */
        queue_head = request;
        request->next = NULL;
    }
    else{ /* Adiciona no fim da fila */
        struct request_t *tptr = queue_head;
        while (tptr->next != NULL)
            tptr = tptr->next;
        tptr->next = request;
        request->next = NULL;
    }
    pthread_cond_signal(&queue_not_empty); /* Avisa um bloqueado nessa condição */
    pthread_mutex_unlock(&queue_lock);
}

struct request_t *queue_get_task(){
    pthread_mutex_lock(&queue_lock);
    while (queue_head == NULL)
        pthread_cond_wait(&queue_not_empty, &queue_lock); /* Espera haver algo */
    struct request_t *task = queue_head;
    queue_head = task->next;
    pthread_mutex_unlock(&queue_lock);
    return task;
}

/* Função da thread secundária que vai processar pedidos de escrita. 
*/ 
void * process_request (void *params){
    //TODO: use pthread_cond_broadcast when locked after trying to increassing proc_max when porc_max is not op_n - 1 (waiting for previous)

    return 0;
}

/* Liberta toda a memória e recursos alocados pela função tree_skel_init.
 */
void tree_skel_destroy(){
    //destroying tree
    tree_destroy(tree);

    //freeing in_progress array
    free(op_proc.in_progress);

    //destroying locks
    pthread_mutex_destroy(queue_lock);
    pthread_mutex_destroy(tree_lock);
    pthread_mutex_destroy(op_proc_lock);
    pthread_cond_destroy(queue_not_empty);

    //TODO: destroys threads???
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

    case MESSAGE_T__OPCODE__OP_HEIGHT: ;
        printf("Requested: height\n");

        msg->result = tree_height(tree);
        msg->opcode = MESSAGE_T__OPCODE__OP_HEIGHT + 1;
        msg->c_type = MESSAGE_T__C_TYPE__CT_RESULT;
        return 0;

    case MESSAGE_T__OPCODE__OP_DEL: ;
        printf("Requested: del %s\n", msg->entry->key);

        //creating request
        struct request_t* new_request = malloc(sizeof(struct request_t));
        //inserting key
        new_request->key = malloc(strlen(msg->entry->key) + 1);
        strcpy(new_request->key, msg->entry->key);
        //setting op and op_n
        new_request->op = 0;
        new_request->op_n = last_assigned;
        last_assigned++;
        
        queue_add_task(new_request);

        //creating answer msg
        msg->opcode = MESSAGE_T__OPCODE__OP_DEL + 1;
        msg->c_type = MESSAGE_T__C_TYPE__CT_RESULT;
        msg->op_n = new_request->op_n;

        return 0;

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

    case MESSAGE_T__OPCODE__OP_PUT: ;
        printf("Requested: put %s %s\n", msg->entry->key, (char*)msg->entry->data.data);

        //creating request
        struct request_t* new_request = malloc(sizeof(struct request_t));
        //inserting key
        new_request->key = malloc(strlen(msg->entry->key) + 1);
        strcpy(new_request->key, msg->entry->key);
        //inserting data
        new_request->data = data_create(msg->entry->data.len);
        memcpy(new_request->data->data, msg->entry->data.data, new_request->data->datasize);
        //setting op and op_n
        new_request->op = 1;
        new_request->op_n = last_assigned;
        last_assigned++;
        
        queue_add_task(new_request);

        //creating answer msg
        msg->opcode = MESSAGE_T__OPCODE__OP_PUT + 1;
        msg->c_type = MESSAGE_T__C_TYPE__CT_RESULT;
        msg->op_n = new_request->op_n;

        return 0;

        //TDOO: Remove old put (leave for reference)

        // //cria data para tree_put
        // key = malloc(strlen(msg->entry->key) + 1);
        // strcpy(key, msg->entry->key);
        // void * buf = malloc(msg->entry->data.len);
        // memcpy(buf, msg->entry->data.data, msg->entry->data.len);
        // data = data_create2(msg->entry->data.len, buf);

        // //caso de erro em tree_put
        // if(tree_put(tree,key,data) == -1 ){
        //     printf("Error on Put\n");
        //     return -1;
        // }

        // msg->opcode = MESSAGE_T__OPCODE__OP_PUT + 1;
        // msg->c_type = MESSAGE_T__C_TYPE__CT_NONE;
        // return 0;

    case MESSAGE_T__OPCODE__OP_VERIFY: ;
        printf("Requested: verify %d\n", msg->op_n);

        //creating answer msg
        msg->opcode = MESSAGE_T__OPCODE__OP_VERIFY + 1;
        msg->c_type = MESSAGE_T__C_TYPE__CT_RESULT;
        msg->result = verify(msg->op_n);

        return 0;

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
    int ret = 0;
    pthread_mutex_lock(op_proc_lock);
    if(op_n <= op_proc.max_proc)
        ret = 1;
    else        //TODO: perguntar ao stor se isto é necessario ou sequer feito assim
        ret = 1;
        for (size_t i = 0; i < sizof(op_proc.in_progress) / sizeof(int); i++){
            if(op_n == op_proc.in_progress[i]){
                ret = 0;
                break;
            }
        }
    pthread_mutex_unlock(op_proc_lock);
    return ret;
}