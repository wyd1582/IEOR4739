#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "gsock.h"

int masteralgorithm(baggedsock *pbsock);

int main(int argc, char* argv[])
{
  int retcode = 0, wcode = 0, readcode;
  int portnumber;
  baggedsock *pbsock = NULL;
  
  if(argc < 2){
    printf("\nUsage: master portnumber\n");
    return 0;
  }
  
  portnumber = atoi(argv[1]);
  
  if((retcode = creategsock(&pbsock, 0))) goto BACK;
  
  printf("\nStarting server");
  
  printf("\ncreating socket with portnumber %d\n", portnumber);
  
  retcode = startupsock(pbsock);
  
  if((retcode = gotrybind(pbsock, portnumber))) goto BACK;
  
  retcode = finishopenstuff(pbsock);
  
  printf("\n Now waiting for a connection on socket id %d\n", pbsock->id);
  
  retcode = startconnection(pbsock);
  if(retcode) goto BACK;
  
  strcpy(pbsock->pBuffer,"ping");
  printf("\nSending \"%s\" to client %d\n",pbsock->pBuffer, pbsock->id);
  write2socket(pbsock, &wcode);
  
  readcode = just_read(pbsock);
  if(readcode){
    printf(" ==> could not read ???\n");
  }
  
  printf("\nReceived \"%s\" from client\n",pbsock->pBuffer);
  fflush(stdout);
  
  retcode = masteralgorithm(pbsock);
  
  printf("\n<closing socket.>\n"); 
  /* close socket */
  if(close(pbsock->hSocket) == SOCKET_ERROR){
    printf("\nCould not close socket\n");
    retcode = 1;
  }
  
  
  usleep(1000);
  
 BACK:
  return retcode;
}

int masteralgorithm(baggedsock *pbsock)
{
  int retcode = 0, n, wcode, readcode;
  double x;

  printf("\n master now running algorithm\n"); fflush(stdout);

  n = 2;

  sprintf(pbsock->pBuffer,"%d",n);
  write2socket(pbsock, &wcode);



  x = 100*rand()/((double) RAND_MAX);
  sprintf(pbsock->pBuffer,"%g",x);
  write2socket(pbsock, &wcode);

  printf("1. just sent %s\n", pbsock->pBuffer);

  readcode = just_read(pbsock);
  printf("got %s from worker\n", pbsock->pBuffer);


  x = 100*rand()/((double) RAND_MAX);
  sprintf(pbsock->pBuffer,"%g",x);
  write2socket(pbsock, &wcode);

  printf("2. just sent %s\n", pbsock->pBuffer);

  readcode = just_read(pbsock);
  printf("got %s from worker\n", pbsock->pBuffer);

  /** say bye **/

  sprintf(pbsock->pBuffer,"bye");
  write2socket(pbsock, &wcode);

  printf("3. just sent %s\n", pbsock->pBuffer);

  /** here we should get 'bye' from worker **/

    readcode = just_read(pbsock);
    if(readcode){
      printf(" ==> could not read?\n");
    }

    printf("\nReceived \"%s\" from worker\n",pbsock->pBuffer);
    fflush(stdout);



  return retcode;
}
