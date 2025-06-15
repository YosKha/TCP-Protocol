MOUSSAID Mohamed Amine et MARGUIER Yohann

- Pas de changement pour la compilation et pour l'exécution

Ce qui marche :
- v1
- v2
- v3
- phase établissement de connexion TCP (sans pertes ou avec peu de pertes : setlossrate(x) x < 20 environ)
- asynchronisme
- négociation de pertes

Ce qui ne marque pas :
- phase établissement de connexion TCP (Pour un pourcentage de pertes élevé)
-> Par conséquent pour imposer la réussite de la connexion et pouvoir tester la fiabilité partielle,
nous avons défini le LOSSRATE=0 avant la connexion et LOSSRATE=20 une fois connecté.
Pour changer la valeur "20", il faut modifier #define LOSSRATE 20 (en pourcentage)

Choix d'implémentation :
-> Stop and Wait :
Utilisation d'une "do-while" pour imposer un premier envoie du pdu

-> Négociation pertes :
C'est le client qui impose/fixe le taux de pertes accepté.
ON peut le modifier via #define ACCEPTABLE_LOSSRATE 20 (en pourcentage)
On a rajouté un attribut "acceptable_lossrate" mic_tcp_header pour partager le taux de pertes
accepté avec le serveur. Cette valeur est donnée dans le SYN de connexion client/serveur.
Par ailleurs, on a ajouté le même attribut à mic_tcp_socket pour y avoir accès à tout instant.
Il sera utiliser avec notre socket : "mainSocket".

-> Asynchronisme :
on utilise :
pthread_cond_t syn_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t ack_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

pour bloquer le serveur dans "mic_tcp_accept". Et on débloque le thread via "process_received_pdu"