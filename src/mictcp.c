#include <mictcp.h>
#include <api/mictcp_core.h>
#include <stdlib.h>
#define MAX_NB_SOCKET 100
#define MAX_ATTEMPT 10
#define TIMEOUT 100
#define NB_WATCHED_LOSSES 10
#define ACCEPTABLE_LOSSRATE 0.8

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

mic_tcp_sock mainSocket;
int num_seq =0;

int loss_table[NB_WATCHED_LOSSES] = {0};
int loss_tab_index = 0;
int nb_losses = 0;

void printLosses(){
    printf("%s{", KYEL);
    for(int i=0; i<NB_WATCHED_LOSSES; i++){
        printf("%d, ", loss_table[i]);
    }
    printf("}\n%s", KWHT);
}

// connection
pthread_cond_t flag = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
   int result = -1;
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   result = initialize_components(sm); /* Appel obligatoire */
   set_loss_rate(20);

   mainSocket.fd = 1;
   mainSocket.state = IDLE;
   return 1;
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   mainSocket.local_addr = addr;
   return 1;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

    // SYN-ACK pdu
    mic_tcp_pdu syn_ack_pdu;
    syn_ack_pdu.header.seq_num = num_seq;
    syn_ack_pdu.header.source_port = mainSocket.local_addr.port;
    syn_ack_pdu.header.dest_port = mainSocket.remote_addr.port;
    syn_ack_pdu.header.syn = 1;
    syn_ack_pdu.header.ack = 1;
    syn_ack_pdu.payload.size = 0;

    int sent_size;

    // Pending SYN
    printf("%s WAITING SYN\n", KYEL);
    pthread_mutex_lock(&mutex);
    while(mainSocket.state != SYN_RECEIVED){
        pthread_cond_wait(&flag, &mutex);
    }
    pthread_mutex_unlock(&mutex);
    printf("%s SYN RECEIVED\n", KGRN);

    // Sending SYN-ACK
    if((sent_size = IP_send(syn_ack_pdu, mainSocket.remote_addr.ip_addr))<0){
        return -1;
    }
    printf("%s SYN-ACK SENT\n", KGRN);

    // Pending ACK
    printf("%s WAITING ACK\n", KYEL);
    pthread_mutex_lock(&mutex);
    while(mainSocket.state != ESTABLISHED){
        pthread_cond_wait(&flag, &mutex);
    }
    pthread_mutex_unlock(&mutex);
    printf("%s ACK RECEIVED/n", KGRN);

    return 0;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    mainSocket.remote_addr = addr;

    // SYN pdu
    mic_tcp_pdu syn_pdu;
    syn_pdu.header.seq_num = num_seq;
    syn_pdu.header.source_port = mainSocket.local_addr.port;
    syn_pdu.header.dest_port = mainSocket.remote_addr.port;
    syn_pdu.header.syn = 1;
    syn_pdu.payload.size = 0;

    // ACK pdu
    mic_tcp_pdu ack_pdu;
    ack_pdu.header.seq_num = num_seq;
    ack_pdu.header.source_port = mainSocket.local_addr.port;
    ack_pdu.header.dest_port = mainSocket.remote_addr.port;
    ack_pdu.header.ack = 1;
    ack_pdu.header.ack_num = num_seq;
    ack_pdu.payload.size = 0;


    int sent_size;

    // Sending SYN
    if((sent_size = IP_send(syn_pdu, addr.ip_addr)) < 0){
        return -1;
    }
    mainSocket.state = SYN_SENT;
    printf("%s SYN SENT\n", KGRN);

    // Receiving SYN-ACK
    printf("%s WAITING SYN-ACK\n", KYEL);
    pthread_mutex_lock(&mutex);
    while(mainSocket.state != SYN_RECEIVED){
        pthread_cond_wait(&flag, &mutex);
    }
    pthread_mutex_unlock(&mutex);
    printf("%s SYN-ACK RECEIVED\n", KGRN);

    // Sending ACK
    if((sent_size  = IP_send(ack_pdu, addr.ip_addr)) < 0){
        return -1;
    }
    printf("%s ACK SENT\n", KGRN);

    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
;

    // PDU message à envoyé
    mic_tcp_pdu pdu;
    pdu.header.source_port = 9000;
    pdu.header.dest_port = mainSocket.remote_addr.port;
    pdu.header.seq_num = num_seq;
    pdu.header.ack = 0;
    pdu.header.ack_num = 0;

    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;

    // Info pour le ACK
    mic_tcp_pdu ack_pdu;
    ack_pdu.payload.size = 0;
    mic_tcp_ip_addr local;
    mic_tcp_ip_addr remote;
    remote.addr_size = INET_ADDRSTRLEN;
    remote.addr = malloc(INET_ADDRSTRLEN);

    int lg_envoyee;
    float loss_ratio;
    int cond = 0;
    // Envoie du PDU et en attente du PDU ACK en retour
    do{
        lg_envoyee  = IP_send(pdu, mainSocket.remote_addr.ip_addr);
        if(lg_envoyee < 0) return -1;
        int received = IP_recv(&ack_pdu, &local, &remote, TIMEOUT);

        // Le ack a bien été reçu
        if(received >= 0 && ack_pdu.header.ack ==1 && ack_pdu.header.ack_num == num_seq){
            num_seq = (num_seq + 1)%2;
            printf("%sACK has been received\n", KGRN);
            if(loss_table[loss_tab_index] == 1){
                nb_losses --;
                loss_table[loss_tab_index] = 0;
            }

        // on a pas reçu de ack attendu
        }else {
            printf("%sACK has not been received\n", KRED);
            if(loss_table[loss_tab_index] == 0 && ! cond){
                nb_losses ++;
                loss_table[loss_tab_index] = 1;
            }
        }

        loss_ratio = (nb_losses / (float)NB_WATCHED_LOSSES);
        cond = (loss_ratio > (float) ACCEPTABLE_LOSSRATE);
        printf("%sInfo :\nnb_losses = %d\nratio = %f\ncond=%d\n", KBLU, nb_losses, loss_ratio, cond);
        printLosses();
    }while(cond);
    printf("%s\n", KWHT);
    loss_tab_index++;
    if(loss_tab_index == NB_WATCHED_LOSSES){
        loss_tab_index = 0;
    }

    return lg_envoyee;
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    
    mic_tcp_payload payload;
    
    payload.data = mesg;
    payload.size = max_mesg_size;

    int effective_data_size = app_buffer_get(payload);
    return effective_data_size;
}

/*
 * Permet de récl
 * amer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    mainSocket.state = IDLE;
    return 0;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    if(pdu.header.syn == 1){
        printf("sdfsdfqs\n");
        pthread_cond_broadcast(&flag);
        mainSocket.state = SYN_RECEIVED;
    }

    if(pdu.header.ack == 1){
        pthread_cond_broadcast(&flag);
        mainSocket.state = ESTABLISHED;
    }

    if(pdu.header.syn == 1 && pdu.header.ack == 1){
        pthread_cond_broadcast(&flag);
        mainSocket.state = SYN_RECEIVED;
    }

    if(pdu.header.ack == 0){

        if(pdu.header.seq_num == num_seq){
            app_buffer_put(pdu.payload);
            num_seq = (num_seq+1)%2;

        }
        mic_tcp_pdu ack;
        ack.header.source_port = mainSocket.local_addr.port;
        ack.header.dest_port = mainSocket.remote_addr.port;
        ack.header.seq_num = 0;
        ack.header.ack = 1;
        ack.header.ack_num = pdu.header.seq_num;
        ack.payload.data = NULL;
        ack.payload.size = 0;

        IP_send(ack, remote_addr);
    }

    app_buffer_put(pdu.payload);
}
