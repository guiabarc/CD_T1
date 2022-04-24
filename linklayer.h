
#ifndef LINKLAYER
#define LINKLAYER

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

//Data used by all functions on the API
typedef struct linkData {
  int fd;
  int role;
  struct termios oldtio;
  struct termios newtio;
  int timeOut;
  int numTries;
  char R;
  char S;

  //Statistics
  int Dtt; //Datos transmitidos totais
  int Dtp; //Dados transmitidos de payload
  int Dtr; //Dados retrasmitidos
  int Drt; //Dados recebidos totais
  int Drp; //Dados recebidos de payload
  int NT;  //Numero de pacotes transmitidos
  int NTr; //Numero de pacotes restransmitidos
  int NR;  //Numero de pacotes recebidos
} linkData;



typedef struct linkLayer {
  char serialPort[50];
  int role; // defines the role of the program: 0==Transmitter, 1=Receiver
  int baudRate;
  int numTries;
  int timeOut;
} linkLayer;

// ROLE
#define NOT_DEFINED -1
#define TRANSMITTER 0
#define RECEIVER 1

// SIZE of maximum acceptable payload; maximum number of bytes that application
// layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// CONNECTION deafault values
#define BAUDRATE_DEFAULT B38400
#define MAX_RETRANSMISSIONS_DEFAULT 3
#define TIMEOUT_DEFAULT 4
#define _POSIX_SOURCE 1 /* POSIX compliant source */

// MISC
#define FALSE 0
#define TRUE 1

//DEBUG (1/0)==(y/n)
#define LL_DEBUG 1

// Opens a conection using the "port" parameters defined in struct linkLayer,
// returns "-1" on error and "1" on sucess
int llopen(linkLayer connectionParameters);
// Sends data in buf with size bufSize
int llwrite(char *buf, int bufSize);
// Receive data in packet
int llread(char *packet);
// Closes previously opened connection; if showStatistics==TRUE, link layer
// should print statistics in the console on close
int llclose(int showStatistics);

#endif
