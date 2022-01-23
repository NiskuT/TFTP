
#ifndef GETTFTP_H
#define GETTFTP_H

int testFile(char* nom);
int buildWRQ(char* buffer,  char* file, char* option);

void generateENTETE(char* buffer, unsigned short blocNum);
unsigned short getBlocOrOppNum(char msb, char lsb);
int extractOption(char* buf, int max);
void showErr(int erreur);

char* fichier;
char* ip;
char* port;
char* blksize;


#endif