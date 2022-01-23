#include "gettftp.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>

#define BUF_DEFAULT_SIZE 512+4
#define SIZE_ACK 4
#define RCV_ENTETE 4 



int main(int argc, char *argv[])
{
        // Gestion des paramètres d'entrée
        switch(argc){
                case 3:
                        fichier=argv[2];
                        ip = argv[1];
                        port="69"; 
                        blksize = "";
                        break;
                case 4:
                        fichier=argv[3];
                        ip = argv[1];
                        port = argv[2];
                        blksize = "";
                        break;
                case 5:
                        fichier=argv[2];
                        ip = argv[1];
                        port="69"; 
                        if(!strcmp(argv[3], "-b")) blksize = argv[4];
                        else blksize = "";
                        break;
                case 6:
                        fichier=argv[3];
                        ip = argv[1];
                        port= argv[2]; 
                        if(!strcmp(argv[4], "-b")) blksize = argv[5];
                        else blksize = "";
                        break;
                default:
                        printf("Utilisation de la commande:\n");
                        printf("%s ip port fichier\n", argv[0]);
                        printf("%s ip fichier\n", argv[0]);
                        printf("Vous pouvez ajouer à la fin -b xxx pour spécifier l'option blocksize.\n");
                        exit(EXIT_FAILURE);
                        break;

        }




        // Déclaration des variables 
        struct addrinfo hints;
        struct addrinfo *result;

        int sfd, s, monFichier, tailleBuf, tailleLecture;

        ssize_t nread;

        char buf[BUF_DEFAULT_SIZE];
        char *lecture = calloc(BUF_DEFAULT_SIZE, sizeof(char)); // Ce buffer est alloué dynamiquement car on se réserve la possibilité de 
                                                                // l'agrandir en cas d'utilisation de blocksize

        short transmit = 1; // Variable pour le while
        short lastBloc = -1; // Variable contenant le dernier bloc recu

        struct timespec start={0,0};   // Variable pour le chronomètre
        struct timespec stop={0,0};
        // FIN de déclaration des variable


        // Par défaut la taille du buffer lecture est BUF_DEFAULT_SIZE
        tailleLecture = BUF_DEFAULT_SIZE;




        // Ouverture / création du fichier. Nous pouvons aussi utiliser la fonction create
        monFichier = open(fichier, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH); 




        // Initialisation de la structure addrinfo pour récuperer l'ip ainsi que d'autres paramètres
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;              // IPv4
        hints.ai_socktype = SOCK_DGRAM;         // Datagram socket : UDP
        hints.ai_flags = 0;
        hints.ai_protocol = IPPROTO_UDP;        // Protocol UDP



        // Recupération des informations à l'aide de getaddrinfo
        s = getaddrinfo(ip, port, &hints, &result);
        if (s != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
        }
    
        
        // Creation du socket avec les informations récoltées precedemment et verification
        sfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if( sfd == -1){
                perror("Erreur d'ouverture de socket");
                exit(EXIT_FAILURE);
        }



        // Cette partie permet uniquement d'indiquer à l'utilisateur l'ip trouvé
        // Pour cela on une structure sockaddr en sockaddr_in qui dispose d'une fonction de conversion en char
        struct sockaddr_in *addr_in = (struct sockaddr_in *)result->ai_addr;
        char *monip = inet_ntoa(addr_in->sin_addr);

        printf("\nIP address: %s\n", monip);
        printf("Socket reservé: %d\n\n", sfd);



        // On crée la requete de lecture que l'on place dans buf
        tailleBuf = buildRRQ(buf, fichier, blksize);

        // On démmarre le chronomètre
        clock_gettime(CLOCK_MONOTONIC, &start);

        // Puis on l'envoie
        sendto(sfd, buf, tailleBuf, 0,result->ai_addr,(socklen_t) result->ai_addrlen);


        // Nous créons une structure sockaddr_storage qui permettra de récuperer le port utiliser par le serveur pour nous répondre
        struct sockaddr_storage addr;
        socklen_t len = sizeof(struct sockaddr);



        // Si nous avons émis une requete avec une option, nous attendons un OACK
        if(strlen(blksize)>0){ 

                nread = recvfrom(sfd, lecture, BUF_DEFAULT_SIZE, 0, (struct sockaddr*)&addr, &len);
                // Nous utilisons recvfrom pour la reception de la reponse du serveur
                // L'adresse (incluant le port) avec lequel le serveur répond est stocké dans la structure addr

                // Dans le cas d'un OACK (le numéro de l'opération est 6)
                if(getBlocOrOppNum(lecture[0], lecture[1]) == 6){
                        
                        // On extrait la taille répondu par le serveur (pour ne pas commettre d'erreur en cas de réponse différente de la demande)
                        int taille = extractOption(lecture, (int)nread);

                        if (taille != tailleLecture - RCV_ENTETE){ // Si cette taille est différente de la taille originale

                                lecture = (char*) realloc(lecture, sizeof(char)*(taille+RCV_ENTETE)); 
                                // On modifie la taille du buffer de lecture pour recevoir le bloc en entier 
                                // Nous utilisons pour cela la fonction realloc qui permet d'agrandir ou réallouer un bloc de la bonne taille

                                tailleLecture = taille + RCV_ENTETE;

                        }

                        generateACK(buf,0); // On génère un aquittement pour le bloc zero, indiquant que l'on a bien reçu la réponse
                        sendto(sfd, buf, SIZE_ACK, 0,(struct sockaddr*)&addr,len); // on envoie l'aquittement à l'adresse addr

                }
                // Traitement de l'erreur (dans le cas ou le code opération est 5)
                else if (getBlocOrOppNum(lecture[0], lecture[1]) == 5){
                        int errNum = getBlocOrOppNum(lecture[2], lecture[3]);
                        showErr(errNum);
                        write(STDOUT_FILENO, lecture+RCV_ENTETE, nread-RCV_ENTETE);
                        exit(EXIT_FAILURE);
                }

                else{
                        printf("Erreur lors de la demande de transfert.\n");
                        exit(EXIT_FAILURE);
                }

        }



       
        while(transmit){

                // Reception des données
                nread = recvfrom(sfd, lecture, tailleLecture, 0, (struct sockaddr*)&addr, &len);



                if (nread == -1) {
                        perror("read");
                        exit(EXIT_FAILURE);
                }

                // Dans le cas où le nombre d'octets lu ne rempli pas le buffer, alors il s'agissait du dernier paquet, donc fin du while
                else if (nread!= tailleLecture) transmit=0;



                // Traitement des erreurs (ici dans le cas où l'on a pas utilisé d'option)
                if (getBlocOrOppNum(lecture[0], lecture[1]) == 5){
                        int errNum = getBlocOrOppNum(lecture[2], lecture[3]);
                        showErr(errNum);
                        write(STDOUT_FILENO, lecture+RCV_ENTETE, nread-RCV_ENTETE);
                        exit(EXIT_FAILURE);
                }


                // Recuperation du numéro de bloc
                unsigned short blocN = getBlocOrOppNum(lecture[2], lecture[3]);


                //printf("Réception du bloc %u, génération du ACK.\n", blocN); // Cette ligne peut être commentée pour moins d'informations


                // ECRITURE DES DONNES RECU DANS LE FICHIER
                char* debut;
                debut = lecture+RCV_ENTETE; // On place le pointeur sur le 4ème octet pour ne pas écrire l'entete dans le fichier
                if (lastBloc!=blocN){
                        write(monFichier, debut, nread-RCV_ENTETE);
                        lastBloc=blocN;
                }
                // FIN ECRITURE



                generateACK(buf,blocN); // On place l'acquittement pour le dernier bloc recu dans le buffer
                                        // Dans le cas où un paquet se perd, le serveur nous renverra celui-ci tant que l'on envoie pas son ACK
                

                sendto(sfd, buf, SIZE_ACK, 0,(struct sockaddr*)&addr,len); // on envoie l'aquittement

        }


        // On arrete le chronomètre
        clock_gettime(CLOCK_MONOTONIC, &stop);

        // Calcul et affichage des temps de transfert
        double diff = (stop.tv_sec - start.tv_sec) * 1e9;
        diff = (diff + (stop.tv_nsec - start.tv_nsec)) * 1e-9;
        printf("\nTemps de transfert: %f s\n", diff);



        free(lecture);
        freeaddrinfo(result);          // Cette struture n'etant plus utilisé, on peut la liberer
        close(sfd);
        close(monFichier);
        exit(EXIT_SUCCESS);
}



int buildRRQ(char* buffer, char* file, char* option){

        // Dans cette fonction nous utilisons des for pour copier nos chaines de caractères dans le buffer et non strcpy
        // pour conserver la maitrise de cette copie et manipuler plus facilement les données

        int sizeofBuffer = 2;
        
        buffer[0]=0x00;
        buffer[1]=0x01; // Le code pour une requete de lecture est 1

        // On copie ensuite le nom du fichier
        for(int k = 0; k<(int)strlen(file);k++) buffer[k+sizeofBuffer]=file[k];
        sizeofBuffer += strlen(file);

        buffer[sizeofBuffer]=0x00;
        sizeofBuffer++;

        // On ecrit ensuite le mode (ici mode octet)
        for(int k = 0; k<5;k++) buffer[k+sizeofBuffer]= "octet"[k];

        sizeofBuffer+=5;
        
        buffer[sizeofBuffer]=0x00;
        sizeofBuffer++;


        // Dans le cas où nous avons l'option blocksize, on l'ajoute à la fin
        if(strlen(option)>0){
                
                for(int k = 0; k<7;k++) buffer[k+sizeofBuffer]="blksize"[k];
                sizeofBuffer += 7;


                buffer[sizeofBuffer]=0x00;
                sizeofBuffer++;

                // On ecrit ci dessous la taille du bloc
                for(int k = 0; k<(int)strlen(option);k++) buffer[k+sizeofBuffer]=option[k];
                sizeofBuffer += strlen(option);
                

                buffer[sizeofBuffer]=0x00;
                sizeofBuffer++;
        }

        // Enfin la fonction renvoie la taille du buffer qu'il faut envoyer
        return sizeofBuffer;

}

void generateACK(char* buffer, unsigned short blocNum){

        //Cette fonction très simple permet de generer l'acquittement
        buffer[0]=0x00;
        buffer[1]=0x04; // Code pour ACK

        unsigned short a = 0xFF00 & blocNum;
        a=a>>8; // On réalise ensuite un ET logique entre le numéro du bloc (qui est un entier non signé sur 16 bits)
                // On décale ensuite de 8 bits à gauche et on le converti en char pour le placer dans le buffer

        buffer[2]= (char) a;
        buffer[3]= (char) (0xFF & blocNum); // On réalise l'opération analogue pour les 8 bits de poids faible


}

unsigned short getBlocOrOppNum(char msb, char lsb){
        // Cette fonction permet d'obtenir un entier non signé sur 16 bits à partir de deux char
        unsigned short a = (unsigned short) msb & 0xFF;
        a=a<<8; // tout d'abord on cast msb en unsigned short et on décale de 8 bits à droite
        // Il nous suffit ensuite d'y additionner le lsb
        a+=(unsigned short) lsb & 0xFF;
        return a;
}

int extractOption(char* buf, int max){
        // Cette fonction permet de rechercher la valeur pour une option
        char opt[50];    // chaine de caractère qui va contenir le nom de l'option
        char value[50];  // chaine de caractère qui va contenir la valeur de l'option

        int idx = 2; // on se place après le code d'opération

        // On récupère le nom de l'option
        while(buf[idx]!=0x00 && idx < max) {
                opt[idx-2]=buf[idx];
                idx++;
        }

        // On passe l'octet 0x00
        idx++;

        int idx2 = idx;
        // On récupère la valeur de l'option
        while(buf[idx2]!=0x00 && idx2 < max) {
                value[idx2-idx]=buf[idx2];
                idx2++;
        }

        int x = atoi(value);  // enfin on utilise atoi pour convertir la valeur de l'option en int
        printf("Option %s detecté: %d \n", opt, x);
        return x;
}


void showErr(int erreur){
        // Cette fonction basée sur la RCF 1350 permet d'afficher le texte correspondant à un code d'erreur
        switch(erreur){
                case 1 : 
                        printf("Fichier non trouvé.\n");
                        break;
                case 2 : 
                        printf("Violation d'accès.\n");
                        break;
                case 3 : 
                        printf("Disque plein ou excès d'allocation.\n");
                        break;
                case 4 : 
                        printf("Operation TFTP illegale.\n");
                        break;
                case 5 : 
                        printf("ID tranfert inconnu.\n");
                        break;
                case 6 : 
                        printf("Le fichier existe déjà.\n");
                        break;
                case 7 : 
                        printf("Utilisateur inconnu.\n");
                        break;
                default:
                        printf("Erreur inconnue.\n");
                        break;
        }
}