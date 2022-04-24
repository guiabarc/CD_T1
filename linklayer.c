#include "linklayer.h"

int count;
int next;
volatile int STOP = FALSE;
// Global data to be user by API
linkData ld;


void ldInitialize() {
  ld.fd = -1;
  ld.R = 0x2;
  ld.S = 0x0;
  ld.Dtt = 0;
  ld.Dtp = 0;
  ld.Dtr = 0;
  ld.NT  = 0;
  ld.NTr = 0;
  ld.NR  = 0;
}

void printStatistics(){
  printf("\n------Statistics------\n");
  printf("--- Sent ---\n");
  printf("Total transmitted data: %d bytes\n", ld.Dtt);
  printf("Total transmitted payload: %d bytes\n",ld.Dtp);
  printf("Total retransmitted data: %d bytes\n",ld.Dtr);
  printf("Efficiency (payload/total data): %.2f%%\n",100*((float)ld.Dtp/(float)ld.Dtt));
  printf("Total number of transmitted packets: %d packets\n",ld.NT);
  printf("Total number of retransmitted packets: %d packets\n",ld.NTr);
  printf("Efficiency (packets): %.2f%%\n",100*(1-(float)ld.NTr/(float)ld.NT));
  printf("--- Received ---\n");
  printf("Total received data: %d bytes\n", ld.Drt);
  printf("Total received payload: %d bytes\n", ld.Drp);
  printf("Efficiency (payload/total data): %.2f%%\n",100*((float)ld.Drp/(float)ld.Drt));
  printf("Total number of received packets: %d packets\n",ld.NR);
  printf("------------------------\n");


}

void retry() {
  count++;
  next = 1;
  if (LL_DEBUG) printf("Retrying...\n");
}

int llopen(linkLayer connectionParameters) {
  //INICIALIZACAO
  ldInitialize();
  //-----------------------

  // input Verifications

  char comp[10];
  strncpy(comp, connectionParameters.serialPort, 9);

  comp[9] = '\0';

  if (strcmp(comp, "/dev/ttyS") != 0) {
    if (LL_DEBUG) printf("ERROR: Invalid port (llopen)\n");
    return -1;
  }

  if (connectionParameters.role != TRANSMITTER &&
      connectionParameters.role != RECEIVER) {
    if (LL_DEBUG) printf("ERROR: Not a transmiter nor a receiver (llopen)\n");
    return -1;
  }

  if (connectionParameters.baudRate <= 0) {
    if (LL_DEBUG) printf("ERROR: Invalid baudRate (llopen)\n");
    return -1;
  }
  if (connectionParameters.numTries <= 0) {
    if (LL_DEBUG) printf("ERROR: Invalid numTries (llopen)\n");
    return -1;
  }
  if (connectionParameters.timeOut <= 0) {
    if (LL_DEBUG) printf("ERROR: Invalid TimeOut (llopen)\n");
    return -1;
  }

  if ((ld.fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) {
    if(LL_DEBUG) perror(connectionParameters.serialPort);
    return -1;
  }

  if (tcgetattr(ld.fd, &ld.oldtio) == -1) { /* save current port settings */
    if (LL_DEBUG) perror("ERROR: (llopen) tcgetattr");
    return -1;
  }

  bzero(&ld.oldtio, sizeof(ld.newtio));
  ld.newtio.c_cflag = BAUDRATE_DEFAULT | CS8 | CLOCAL | CREAD;
  ld.newtio.c_iflag = IGNPAR;
  ld.newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  ld.newtio.c_lflag = 0;

  ld.newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
  ld.newtio.c_cc[VMIN] = 0;  /* blocking read until 5 chars received */

  tcflush(ld.fd, TCIOFLUSH);

  if (tcsetattr(ld.fd, TCSANOW, &ld.newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  if (LL_DEBUG) printf("New termios structure set\n");

  // Variable set
  int res;
  int state;
  char buf[5];
  unsigned char FLAG; // Flag
  unsigned char TA;   // Transmitter address
  unsigned char RA;   // Receiver address
  unsigned char TC;   // Transmitter control
  unsigned char RC;   // Receiver control

  FLAG = 0x7e;
  TA = 0x3;
  RA = 0x1;
  TC = 0x3;
  RC = 0x7;

  // Set function for alarm
  signal(SIGALRM, retry);

  ld.timeOut = connectionParameters.timeOut;
  ld.role = connectionParameters.role;
  ld.numTries = connectionParameters.numTries;
  // Transmit and listen
  if (connectionParameters.role == TRANSMITTER) {
    STOP = FALSE;
    count = 0;
    next = 0;

    while (count < connectionParameters.numTries && STOP == FALSE) {

      buf[0] = FLAG;
      buf[1] = TA;
      buf[2] = TC;
      buf[3] = TA ^ TC;
      buf[4] = FLAG;

      if ((res = write(ld.fd, buf, 5)) < 0) {
        if (LL_DEBUG) printf("ERROR: could not write data (llopen)\n");
        return -1;
      }

      ld.NT++;
      ld.Dtt += res;
      if (next == 1) {
        ld.Dtr += res;
        ld.NTr++;
      }

      next = 0;

      alarm(connectionParameters.timeOut);

      if(LL_DEBUG) printf("%d bytes written (llopen)\n", res);
      if(LL_DEBUG) printf("Transmitted: waiting for response! (llopen)\n");

      while (STOP == FALSE && next == 0) {

        if ((res = read(ld.fd, buf, 1)) < 0) {
          if (LL_DEBUG) printf("ERROR: could not read data (llopen)\n");
          return -1;
        }

        switch (state) {
        case 0:
          if (buf[0] == FLAG)
            state = 1;
          break;

        case 1:
          if (buf[0] == RA)
            state = 2;
          else if (buf[0] == FLAG)
            state = 1;
          else
            state = 0;
          break;

        case 2:
          if (buf[0] == RC)
            state = 3;
          else if (buf[0] == FLAG)
            state = 1;
          else
            state = 0;
          break;

        case 3:
          if (buf[0] == (RA ^ RC))
            state = 4;
          else if (buf[0] == FLAG)
            state = 1;
          else
            state = 0;
          break;

        case 4:
          if (buf[0] == FLAG)
            STOP = TRUE;
          else
            state = 0;
          break;

        default:
          state = 0;
        }
      }
    }

    alarm(0);
    if (STOP == TRUE) {
      if (LL_DEBUG) printf("Connection established! (llopen)\n");
      ld.NR++;
      ld.Drt += 5;
      return ld.fd;
    }
    else {
      if (LL_DEBUG) printf("Could not establish connection! (llopen)\n");
      return -1;
    }

  }

  // Listen and answer
  else {
    STOP = FALSE;
    while (STOP == FALSE) {
      if ((res = read(ld.fd, buf, 1)) < 0) {
        if (LL_DEBUG) printf("ERROR: could not read data (llopen)\n");
        return -1;
      }

      switch (state) {
      case 0:
        if (buf[0] == FLAG)
          state = 1;
        break;

      case 1:
        if (buf[0] == TA)
          state = 2;
        else if (buf[0] == FLAG)
          state = 1;
        else
          state = 0;
        break;

      case 2:
        if (buf[0] == TC)
          state = 3;
        else if (buf[0] == FLAG)
          state = 1;
        else
          state = 0;
        break;

      case 3:
        if (buf[0] == (TA ^ TC))
          state = 4;
        else if (buf[0] == FLAG)
          state = 1;
        else
          state = 0;
        break;

      case 4:
        if (buf[0] == FLAG)
          STOP = TRUE;
        else
          state = 0;
        break;

      default:
        state = 0;
      }
    }


    if (STOP == FALSE) {
      if (LL_DEBUG) printf("Could not establish connection! (llopen)\n");
      return -1;
    }
    else if (STOP == TRUE) {

      ld.Drt += 5;
      ld.NR++;

      buf[0] = FLAG;
      buf[1] = RA;
      buf[2] = RC;
      buf[3] = RA ^ RC;
      buf[4] = FLAG;

      if ((res = write(ld.fd, buf, 5)) < 0) {
        if (LL_DEBUG) printf("ERROR: could not write data (llopen)\n");
        return -1;
      }
      ld.Dtt += res;
      ld.NT++;
      if (LL_DEBUG) printf("Connection established! (llopen)\n");
      return ld.fd;
    }


  }

  if (LL_DEBUG) printf("ERROR: No return (llopen)\n");
  return -1;

}


int llwrite(char *buf, int bufSize) {

  if (buf == NULL) {
    if (LL_DEBUG) printf("ERROR: Invalid buffer (llwrite)\n");
    return -1;
  }
  if (bufSize <= 0 || bufSize > MAX_PAYLOAD_SIZE) {
    if (LL_DEBUG) printf("ERROR: Invalid length: %d (llwrite)\n", bufSize);
    return -1;
  }
  if (ld.fd < 0) {
    if (LL_DEBUG) printf("ERROR: No opened connection (llwrite)\n");
    return -1;
  }




  //--------------------
  //--- Check for UA ---
  //--------------------

  int res;
  int state;
  int i;
  char packet[2 * MAX_PAYLOAD_SIZE + 7];
  char FLAG; // Flag
  char TA;   // Transmitter address
  char RA;   // Receiver address
  char BCC2; // BCC2


  FLAG = 0x7e;
  TA = 0x3;
  RA = 0x1;

  BCC2 = buf[0];

  for (i = 1; i < bufSize; i++) {
    BCC2 = BCC2 ^ buf[i];
  }

  // Header
  packet[0] = FLAG;
  packet[1] = TA;          // A
  packet[2] = ld.S;        // C
  packet[3] = TA ^ ld.S;   // BCC1

  // Data with stuffing
  int pos = 0; // Buf position
  i = 4;       // first data in packet position

  while (pos < bufSize) {

    if (buf[pos] == FLAG) { // Write FLAG - stuffing
      packet[i] = 0x7d;
      i++;
      packet[i] = 0x5e;
    }
    else if (buf[pos] == 0x7d) { // Write ESC - stuffing
      packet[i] = 0x7d;
      i++;
      packet[i] = 0x5d;
    }
    else { // Any other byte
      packet[i] = buf[pos];
    }

    i++;
    pos++;
  }

  // i-1 = last written position
  if (BCC2 == FLAG) { // BCC stuffing
    packet[i] = 0x7d;
    i++;
    packet[i] = 0x5e;
  }
  else if (BCC2 == 0x7d) { // BCC stuffing
    packet[i] = 0x7d;
    i++;
    packet[i] = 0x5d;
  }
  else { // Any other byte
    packet[i] = BCC2;
  }
  packet[i + 1] = FLAG;

  // Transmits ld.numTries times
  count = 0;
  char RR_REJ = 0;
  STOP = FALSE;
  next = 0;

  while (count < ld.numTries && STOP == FALSE) {
    // Transmit
    if ((res = write(ld.fd, packet, i + 3)) < 0) {
      if (LL_DEBUG) printf("ERROR: could not write data (llwrite)\n");
      return -1;
    }
    if (LL_DEBUG) printf("%d bytes written with a load of %d (llwrite)\n", res, bufSize);

    ld.NT++;
    ld.Dtt += res;
    ld.Dtp += bufSize;
    if (next == 1) {
      ld.Dtr += res;
      ld.NTr++;
    }

    alarm(ld.timeOut);
    next = 0;
    state = 0;
    RR_REJ = 0;

    // Listen
    while (STOP == FALSE && next == 0 && RR_REJ == 0) {
      if ((res = read(ld.fd, buf, 1)) < 0) {
        if (LL_DEBUG) printf("ERROR: could not read data (llwrite)\n");
        return -1;
      }

      switch (state) {
      case 0:
        if (buf[0] == FLAG)
          state = 1;
        break;

      case 1:
        if (buf[0] == RA)
          state = 2;
        else if (buf[0] == FLAG)
          state = 1;
        else
          state = 0;
        break;

      case 2:
        if (buf[0] == (0x1 | (ld.R << 4)))
          state = 3;
        else if (buf[0] == (0x5 | (ld.R << 4)))
          state = 4;
        else if (buf[0] == FLAG)
          state = 1;
        else
          state = 0;
        break;

      // RR
      case 3:
        if (buf[0] == (RA ^ (0x1 | (ld.R << 4))))
          state = 5;
        else if (buf[0] == FLAG)
          state = 1;
        else
          state = 0;
        break;

      // REJ
      case 4:
        if (buf[0] == (RA ^ (0x5 | (ld.R << 4))))
          state = 6;
        else if (buf[0] == FLAG)
          state = 1;
        else
          state = 0;
        break;

      // RR - FLAG
      case 5:
        if (buf[0] == FLAG) {
          STOP = TRUE;
          RR_REJ = 0;
        }
        else
          state = 0;
        break;

      // REJ - FLAG
      case 6:
        if (buf[0] == FLAG) {
          RR_REJ = 1;
          if (LL_DEBUG) printf("Warning: Recieved REJ (llwrite)\n");
          ld.Drt += 5;
          ld.NR++;
          count++;
          alarm(0);
        }
        else
          state = 0;
        break;

      default:
        state = 0;
      }
    }

    alarm(0);
    if (STOP == TRUE && (RR_REJ == 0)) {
      if (LL_DEBUG) printf("Recieved RR (llwrite)\n");

      ld.Drt += 5;
      ld.NR++;

      ld.R = ld.R ^ 0x2;
      ld.S = ld.S ^ 0x2;


      return res;
    }
  }
  if (STOP == FALSE && (RR_REJ == 0)) {
    if (LL_DEBUG) printf("ERROR: No response (llwrite)\n");
    return -1;
  }
  else {
    if (LL_DEBUG) printf("ERROR: Recieved multiple REJs (llwrite)\n");
    return -1;
  }
}


int llread(char *packet) {
  if (packet == NULL) {
    if (LL_DEBUG) printf("ERROR: No packet (llread)\n");
    return -1;
  }
  if (ld.fd < 0) {
    if (LL_DEBUG) printf("ERROR: No opened connection (llread)\n");
    return -1;
  }




  int res;
  int i = 0;
  int state;
  int pos;

  char FLAG; // Flag
  char TA;   // Transmitter address
  char RA;   // Receiver address
  char BCC2; // BCC2
  char ESC;
  char buf[5];
  char SET;

  FLAG = 0x7e;
  TA = 0x3;
  RA = 0x1;
  ESC = 0x7d;
  SET = 0;


  // state machine
  state = 0;
  STOP = FALSE;
  count = 0;
  next = 0;
  signal(SIGALRM, retry);
  char RR_REJ = 0;

  reject:
  if (RR_REJ == 1) {
    STOP = FALSE;
    i=0;
    RR_REJ = 0;
    count++;
  }

  while(STOP == FALSE && count < ld.numTries) {
    if(i>=2*MAX_PAYLOAD_SIZE) break;
    next = 0;
    state = 0;
    alarm(ld.timeOut);

    while (STOP == FALSE && next == 0 && i < 2*MAX_PAYLOAD_SIZE) {
      for(int k = 0 ; k < 5 ; k++) buf[k] = 0;
      res = read(ld.fd, buf, 1);
      if (res < 0){
        if (LL_DEBUG) printf("ERROR: could not read data (llread)\n");
        return -1;
      }
      else if (res == 0) continue;



      switch (state) {
      case 0:
        if (buf[0] == FLAG){
          packet[0] = buf[0];
          state = 1;
        }
        break;

      case 1:
        if (buf[0] == TA){
          packet[1] = buf[0];
          state = 2;
        }
        else if (buf[0] == FLAG)
          state = 1;
        else
          state = 0;
        break;

      case 2:
        if (buf[0] == ld.S){
          packet[2] = buf[0];
          state = 3;
        }
        else if(buf[0] == FLAG)
          state=1;
        else
          state = 0;
        break;

      case 3:
        if (buf[0] == (TA^ld.S)) {
          packet[3] = buf[0];
          state = 4;
          i = 4;
        }
        else if(buf[0] == FLAG)
          state=1;
        else
          state = 0;
        break;

      case 4:
        if (buf[0] == FLAG){
          packet[i] = buf[0];

          STOP = TRUE;
        }
        else { //Dado ordinario
          packet[i] = buf[0];
          i++;
        }
        break;

        case 5:
        if (buf[0] == 0x3)
          state = 6;
        else if (buf[0] == FLAG)
          state = 1;
        else
          state = 0;
        break;

      case 6:
        if (buf[0] == (TA ^ 0x3)){
          state = 4;
          SET = 1;
        }
        else if (buf[0] == FLAG)
          state = 1;
        else
          state = 0;
        break;

        default:
          state=0;
      }
    }
    if (SET == 1 && STOP == TRUE) {
      ld.Drt += 5;
      ld.NR++;

      buf[0] = FLAG;
      buf[1] = RA;
      buf[2] = 0x7;
      buf[3] = RA ^ 0x7;
      buf[4] = FLAG;

      if ((res = write(ld.fd, buf, 5)) < 0) {
        if (LL_DEBUG) printf("ERROR: could not write data UA (llread)\n");
        return -1;
      }
      ld.Dtt += res;
      ld.NT++;
      if (LL_DEBUG) printf("Connection established! (llread)\n");

      STOP = FALSE;
      SET = 0;
    }

  }
  alarm(0);

  if (STOP == FALSE) {
    if (LL_DEBUG) printf("ERROR: Did not recieve packet (llread)\n");
    return -1;
  }

  // Destuffing
  i = 4;   // position in packet
  pos = 0; // position in buffer



  while (i < (2 * MAX_PAYLOAD_SIZE + 7)) {

    if (packet[i] == FLAG) break;

    else if (packet[i] == 0x7d && packet[i + 1] == 0x5e) { // FLAG
      packet[pos] = FLAG;
      i++;
    }
    else if (packet[i] == 0x7d && packet[i + 1] == 0x5d) { // ESC
      packet[pos] = ESC;
      i++;
    }
    else { // Any other byte
      packet[pos] = packet[i];
    }

    i++;
    pos++;
  }

  ld.NR++;
  ld.Drt += pos + 2;
  ld.Drp += pos - 4;


  //BCC2
  BCC2 = packet[0];

  for (i = 1; i < (pos-1) ; i++) {
    BCC2 = BCC2 ^ packet[i];
  }


  if (BCC2 != packet[pos-1]) {

    //REJ
    buf[0] = FLAG;
    buf[1] = RA;
    buf[2] = (0x5|(ld.R<<4));
    buf[3] = RA^buf[2];
    buf[4] = FLAG;

    if ((res = write(ld.fd, buf, 5)) < 0) {
      if (LL_DEBUG) printf("ERROR: could not send REJ (llread)\n");
      return -1;
    }
    ld.Dtt += res;
    ld.NT++;

    if (LL_DEBUG) printf("ERROR: Invalid BCC2 (llread)\n");
    if (LL_DEBUG) printf("ERROR: Sending REJ... (llread)\n");
    RR_REJ = 1;
    goto reject;
  }
  else {
    if (LL_DEBUG) printf("Sending RR (llread)\n");
    //RR
    buf[0] = FLAG;
    buf[1] = RA;
    buf[2] = (0x1|(ld.R<<4));
    buf[3] = RA^buf[2];
    buf[4] = FLAG;

    ld.R = ld.R ^ 0x2;

    ld.S = ld.S ^ 0x2;


    if ((res = write(ld.fd, buf, 5)) < 0) {
      if (LL_DEBUG) printf("ERROR: could not send RR (llread)\n");
      return -1;
    }
    ld.NT++;
    ld.Dtt += res;

    return pos-1;
  }


}


int llclose(int showStatistics) {

  unsigned char FLAG;
  unsigned char TA;   // Transmitter address
  unsigned char RA;   // Receiver address
  unsigned char DISC; // Disconect control
  unsigned char UA;   // Unnumbered acknowledgment
  unsigned char buf[5];
  int res;
  int state;

  FLAG = 0x7e;
  TA = 0x3;
  RA = 0x1;
  DISC = 0xB;
  UA = 0x7;

  //Set signal
  signal(SIGALRM, retry);
  count = 0;

  // Transmitter sends DISC
  if (ld.role == TRANSMITTER) {
    STOP = FALSE;
    next = 0;

    while (count < ld.numTries && STOP == FALSE) {


      buf[0] = FLAG;
      buf[1] = TA;
      buf[2] = DISC;
      buf[3] = TA ^ DISC;
      buf[4] = FLAG;


      if ((res = write(ld.fd, buf, 5)) < 0) {
        if (LL_DEBUG) printf("ERROR: could not write data  / DISC (llclose)\n");
        return -1;
      }
      ld.Dtt += res;
      ld.NT++;
      if (next == 1) {
        ld.Dtr += res;
        ld.NTr++;
      }

      alarm(ld.timeOut);
      state = 0;
      next = 0;

      // Transmitter receives DISC
      while (STOP == FALSE && next == 0) {
        if ((res = read(ld.fd, buf, 1)) < 0) {
          if (LL_DEBUG) printf("ERROR: could not read data/DISC (llclose)\n");
          return -1;
        }
        switch (state) {
        case 0:
          if (buf[0] == FLAG)
            state = 1;
          break;

        case 1:
          if (buf[0] == RA)
            state = 2;
          else if (buf[0] == FLAG)
            state = 1;
          else
            state = 0;
          break;

        case 2:
          if (buf[0] == DISC)
            state = 3;
          else if (buf[0] == FLAG)
            state = 1;
          else
            state = 0;
          break;

        case 3:
          if (buf[0] == (RA ^ DISC))
            state = 4;
          else if (buf[0] == FLAG)
            state = 1;
          else
            state = 0;
          break;

        case 4:
          if (buf[0] == FLAG)
            STOP = TRUE;
          else
            state = 0;
          break;

        default:
          state = 0;
        }
      }
    }

    alarm(0);

    if(STOP==FALSE){
      if (LL_DEBUG) printf("ERROR: did not receive DISC (llclose)\n");
      return -1;
    }

    ld.NR++;
    ld.Drt += 5;

    // Sends UA

    buf[0] = FLAG;
    buf[1] = TA;
    buf[2] = UA;
    buf[3] = TA ^ UA;
    buf[4] = FLAG;

    if ((res = write(ld.fd, buf, 5)) < 0) {
      if (LL_DEBUG) printf("ERROR: could not write data  / UA (llclose)\n");
      return -1;
     }
    ld.Dtt += res;
    ld.NT++;
  }


    else if (ld.role == RECEIVER) {
      STOP = FALSE;

      while(STOP == FALSE && count < ld.numTries) {
        next = 0;
        alarm(ld.timeOut);
        STOP = FALSE;

        while (STOP == FALSE && next == 0) {
          buf[0] = 0;
          if ((res = read(ld.fd, buf, 1)) < 0) {
            if (LL_DEBUG) printf("ERROR: could not read data (llclose)\n");
            return -1;
          }
          switch (state) {
          case 0:
            if (buf[0] == FLAG)
              state = 1;
            break;
          case 1:
            if (buf[0] == TA)
              state = 2;
            else if (buf[0] == FLAG)
              state = 1;
            else
              state = 0;
            break;

          case 2:
            if (buf[0] == DISC)
              state = 3;
            else if (buf[0] == FLAG)
              state = 1;
            else
              state = 0;
            break;

          case 3:
            if (buf[0] == (TA ^ DISC))
              state = 4;
            else if (buf[0] == FLAG)
              state = 1;
            else
              state = 0;
            break;

          case 4:
            if (buf[0] == FLAG)
              STOP = TRUE;
            else
              state = 0;
            break;

          default:
            state = 0;
          }
        }
      }

      alarm(0);

      if (STOP == FALSE) {


      }

      count = 0;
      STOP = FALSE;
      next = 0;

      while (count < ld.numTries && STOP == FALSE) {


        state = 0;
        // Receiver sends DISC
        buf[0] = FLAG;
        buf[1] = RA;
        buf[2] = DISC;
        buf[3] = RA ^ DISC;
        buf[4] = FLAG;

        if ((res = write(ld.fd, buf, 5)) < 0) {
          if (LL_DEBUG) printf("ERROR: could not write data (llclose)\n");
          return -1;
        }

        ld.Dtt += res;
        ld.NT++;
        if (next == 1) {
          ld.Dtr += res;
          ld.NTr++;
        }

        next = 0;
        alarm(ld.timeOut);

        // Receiver receives UA
        while (STOP == FALSE && next == 0) {
          if ((res = read(ld.fd, buf, 1)) < 0) {
            if (LL_DEBUG) printf("ERROR: could not read data (llclose)\n");
            return -1;
          }
          switch (state) {
          case 0:
            if (buf[0] == FLAG)
              state = 1;
            break;

          case 1:
            if (buf[0] == TA)
              state = 2;
            else if (buf[0] == FLAG)
              state = 1;
            else
              state = 0;
            break;

          case 2:
            if (buf[0] == UA)
              state = 3;
            else if (buf[0] == FLAG)
              state = 1;
            else
              state = 0;
            break;

          case 3:
            if (buf[0] == (TA ^ UA))
              state = 4;
            else if (buf[0] == FLAG)
              state = 1;
            else
              state = 0;
            break;

          case 4:
            if (buf[0] == FLAG)
              STOP = TRUE;
            else
              state = 0;
            break;

          default:
            state = 0;
          }
        }
        alarm(0);
      }
    }

    sleep(1);

    if (tcsetattr(ld.fd, TCSANOW, &ld.oldtio) == -1) {
      perror("ERROR: tcsetattr(llclose)");
      return -1;
    }

    if (STOP == TRUE){
      if (LL_DEBUG) printf("Connection finished! (llclose)\n");
      ld.Drt += 5;
      ld.NR++;
    }
    else {
      if (LL_DEBUG) printf("Could not finish connection! (llclose)\n");
      return -1;
    }

    if (showStatistics) printStatistics();
    close(ld.fd);
    return 1;
    }
