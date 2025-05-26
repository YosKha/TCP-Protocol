#include <mictcp.h>
#include <api/mictcp_core.h>
#define MAX_NB_SOCKET 100
#define MAX_ATTEMPT 10
#define TIMEOUT 100
#define NB_WATCHED_LOSSES 10
#define ACCEPTABLE_LOSSRATE 0.2

mic_tcp_sock mainSocket;
int num_seq =0;

int* loss_table = malloc(NB_WATCHED_LOSSES*sizeof(int));
int loss_tab_index = 0;
int nb_losses = 0;
for(int i=0; i<NB_WATCHED_LOSSES; i++){
    loss_table[i] = 0;
}


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


    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

    // PDU message à envoyé
    mic_tcp_pdu pdu;
    pdu.header.source_port = 9000;
    pdu.header.dest_port = mainSocket.remote_addr.port;
    pdu.header.seq_num = num_seq;
    pdu.header.ack = 0;
    pdu.header.ack_num = 0;

    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;

    int lg_envoyee = IP_send(pdu, mainSocket.remote_addr.ip_addr);

    // Attente du PDU ACK en retour
    int counter = 0;
    int isRepeated = 0;
    do{
        int sent = IP_send(pdu, mainSocket.remote_addr.ip_addr);
        if(sent < 0) return 0;


        mic_tcp_pdu ack_pdu;
        ack_pdu.payload.size = 0;
        mic_tcp_ip_addr local;
        mic_tcp_ip_addr remote;
        remote.addr_size = INET_ADDRSTRLEN;
        remote.addr = malloc(INET_ADDRSTRLEN);
        int received = IP_recv(&ack_pdu, &local, &remote, TIMEOUT);

        """if(isRepeated){
            loss_tab_index --;
        }"""

        // It has received the good ack
        if(received >= 0 && ack_pdu.header.ack ==1 && ack_pdu.header.ack_num == num_seq){
            num_seq = (num_seq + 1)%2;
            
            if(loss_table[loss_tab_index] == 1){
                nb_losses --;
            }
            loss_table[loss_tab_index] = 0;
            loss_tab_index ++;
            if(loss_tab_index == NB_WATCHED_LOSSES){
                loss_tab_index = 0;
            }
        // It has had a loss
        }else if(!isRepeated){
            if(loss_table[loss_tab_index] != 1){
                loss_table[loss_tab_index] = 1;
                nb_losses ++;
            }
            loss_tab_index ++;
            if(loss_tab_index == NB_WATCHED_LOSSES){
                loss_tab_index = 0;
            }
        }
        
        isRepeated = 1;
        counter ++;
    }while((nb_losses/NB_WATCHED_LOSSES) > ACCEPTABLE_LOSSRATE)

    if(counter == MAX_ATTEMPT){
        return -1;
    }else{
        return lg_envoyee;
    }

    
    
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
 * Permet de réclamer la destruction d’un socket.
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
