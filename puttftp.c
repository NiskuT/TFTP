#include "puttftp.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h> // pour open

#define BUF_DEFAULT_SIZE 512+4
#define SIZE_ACK 4
#define ENTETE 4


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



        // Test de l'existence du fichier
        if( !testFile(fichier)) { 
                printf("fichier: %s\n", fichier);
                printf("ip: %s\n", ip);
        }
        else{
                perror("Erreur de fichier");
                exit(EXIT_FAILURE);
        }




        // Déclaration des variables 
        struct addrinfo hints;
        struct addrinfo *result;

        int sfd, s, monFichier, tailleBuf, tailleLecture, ret;

        ssize_t nread;

        char buf[BUF_DEFAULT_SIZE];
        char *lecture = calloc(BUF_DEFAULT_SIZE, sizeof(char)); // Ce buffer est alloué dynamiquement car on se réserve la possibilité de 
                                                                // l'agrandir en cas d'utilisation de blocksize

        short transmit = 1;  // variable pour le while
        unsigned short blocToSend = 0;   // Le numéro du bloc à envoyer
        unsigned short lastBlocSend = 0; // Le numéro du dernier bloc envoyé
        // FIN de déclaration des varable



        // Par défaut la taille du buffer lecture est BUF_DEFAULT_SIZE
        tailleLecture = BUF_DEFAULT_SIZE;



        // Ouverture du fichier à envoyer
        monFichier= open(fichier, O_RDONLY);



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
        printf("Socket reservé: %d\n", sfd);





        // On crée la requete d'écriture que l'on place dans buf
        tailleBuf = buildWRQ(buf, fichier, blksize );


        // Puis on l'envoie
        sendto(sfd, buf, tailleBuf, 0,result->ai_addr,(socklen_t) result->ai_addrlen);



        // Nous créons une structure sockaddr_storage qui permettra de récuperer le port utiliser par le serveur pour nous répondre
        struct sockaddr_storage addr;
        socklen_t len = sizeof(struct sockaddr);





        // reception de l'OACK ou de l'ACK correspondant au bloc 0
        nread = recvfrom(sfd, buf, BUF_DEFAULT_SIZE, 0, (struct sockaddr*)&addr, &len); 

        // En cas d'erreur: 
        if (nread == -1) {
                perror("read");
                exit(EXIT_FAILURE);
        }

        // Dans le cas de l'acquittement d'un bloc zero:
        else if(getBlocOrOppNum(buf[0], buf[1]) == 4 && getBlocOrOppNum(buf[2], buf[3]) == 0 ) blocToSend = 1;

        // Dans le cas d'un OACK (acquittement pour l'option)
        else if (getBlocOrOppNum(buf[0], buf[1]) == 6){

                // On récupère la valeur de l'option renvoyée dans l'OACK
                int taille = extractOption(buf, (int)nread);

                if (taille != tailleLecture - ENTETE){ // Si cette taille est différente de la taille originale

                        lecture = (char*) realloc(lecture, sizeof(char)*(taille+ ENTETE)); 
                        // On modifie la taille du buffer de lecture pour recevoir le bloc en entier 
                        // Nous utilisons pour cela la fonction realloc qui permet d'agrandir ou réallouer un bloc de la bonne taille

                        tailleLecture = taille + ENTETE;
                }

                blocToSend = 1;  // On place le numéro de bloc à envoyer à 1
        }

        // Traitement des erreurs (code d'opération 5)
        else if (getBlocOrOppNum(buf[0], buf[1]) == 5){
                int errNum = getBlocOrOppNum(buf[2], buf[3]);
                showErr(errNum);
                write(STDOUT_FILENO, buf+ENTETE, nread-ENTETE);
                exit(EXIT_FAILURE);
        }

        else{
                printf("Erreur lors de la demande de transfert.\n");
                exit(EXIT_FAILURE);
        }


       
        while(transmit){

                // Dans le cas où l'on a bien recu l'ACK du bloc précedent, on en lit un autre
                if(lastBlocSend!=blocToSend) { 
                        ret = read(monFichier, lecture+ENTETE, tailleLecture - ENTETE);

                        // On utilise generateENTETE pour generer l'entete TFTP du bloc
                        generateENTETE(lecture, blocToSend);
                        ret+=4;
                        lastBlocSend=blocToSend;
                }


                // On envoie le paquet du dernier bloc non acquitté
                sendto(sfd, lecture, ret, 0,(struct sockaddr*)&addr,len);


                // On attend la reception d'un acquittement
                nread = recvfrom(sfd, buf, BUF_DEFAULT_SIZE, 0, (struct sockaddr*)&addr, &len);  // reception de l'ACK


                // On récupère sur la réponse le code d'opération ainsi que la valeur (numéro du bloc)
                unsigned short operation = getBlocOrOppNum(buf[0], buf[1]);
                unsigned short blocN = getBlocOrOppNum(buf[2], buf[3]);

                // Dans le cas où la taille du buffer préparé pour l'envoie n'est pas égale à sa taille max, alors c'est le dernier
                // Si il s'agit d'un acquittement sur le dernier bloc envoyé, et qu'il s'agissait du dernier bloc alors on met fin au while
                if(operation==4 && blocN == lastBlocSend && ret!=tailleLecture) transmit=0;
                // Sinon dans le cas où il s'agit d'un ACK sur le dernier bloc envoyé, on passe au bloc suivant
                else if (operation==4 && blocN == lastBlocSend) blocToSend++; 
                // Sinon on renvoie le meme bloc
                

        }



        free(lecture);
        freeaddrinfo(result);
        close(sfd);
        close(monFichier);
        exit(EXIT_SUCCESS);
}


int testFile(char* nom){
        // Cette fonction utilise la fonction lstat ainsi qu'une structure stat pour tester le fichier

        struct stat sb;

        if (lstat(nom, &sb) == -1) {
                //Le fichier n'existe pas.
                return 1;
        }
        else if ((sb.st_mode & S_IFMT) == S_IFREG){
                //Fichier normal.
                return 0;
        }
        else{
                //Fichier non ordianire.
                return 2;
        }
}


int buildWRQ(char* buffer, char* file, char* option){
        // Dans cette fonction nous utilisons des for pour copier nos chaines de caractères dans le buffer et non strcpy
        // pour conserver la maitrise de cette copie et manipuler plus facilement les données

        int sizeofBuffer = 2;
        
        buffer[0]=0x00;
        buffer[1]=0x02;  // Code pour l'écriture

        // On écrit le nom du fichier
        for(int k = 0; k<(int)strlen(file);k++) buffer[k+sizeofBuffer]=file[k];
        sizeofBuffer += strlen(file);

        buffer[sizeofBuffer]=0x00;
        sizeofBuffer++;

        // On écrit le mode
        for(int k = 0; k<5;k++) buffer[k+sizeofBuffer]= "octet"[k];

        sizeofBuffer+=5;
        
        buffer[sizeofBuffer]=0x00;
        sizeofBuffer++;



        if(strlen(option)>0){ // Dans le cas d'une option on l'écrit à la suite
                
                for(int k = 0; k<7;k++) buffer[k+sizeofBuffer]="blksize"[k];
                sizeofBuffer += 7;


                buffer[sizeofBuffer]=0x00;
                sizeofBuffer++;


                for(int k = 0; k<(int)strlen(option);k++) buffer[k+sizeofBuffer]=option[k];
                sizeofBuffer += strlen(option);
                

                buffer[sizeofBuffer]=0x00;
                sizeofBuffer++;
        }

        return sizeofBuffer;

}

void generateENTETE(char* buffer, unsigned short blocNum){
        // Cette fonction permet de génerer une entete de tyoe DATA avec comme numéro de bloc, blocNum
        buffer[0]=0x00;
        buffer[1]=0x03;

        unsigned short a = 0xFF00 & blocNum;
        a=a>>8;
        // On convertit ici l'entie non signé sur 16 bit en 2 char (sur 8 bits)
        // Après avoir décaler de 8bits sur la droite les 8 bits de poids forts, on efectue un cast en char

        buffer[2]= (char) a;
        buffer[3]= (char) (0xFF & blocNum); // Et on cast les 8 bits de poids faibles

}

unsigned short getBlocOrOppNum(char msb, char lsb){
        // Cette fonction permet d'obtenir un entier non signé sur 16 bits à partir de deux char
        unsigned short a = (unsigned short) msb & 0xFF;
        a=a<<8;        
        // tout d'abord on cast msb en unsigned short et on décale de 8 bits à droite
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

        // On récupère la valeur de l'option
        int idx2 = idx;
        while(buf[idx2]!=0x00 && idx2 < max) {
                value[idx2-idx]=buf[idx2];
                idx2++;
        }

        int x = atoi(value); // enfin on utilise atoi pour convertir la valeur de l'option en int
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