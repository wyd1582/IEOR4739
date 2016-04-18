#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include "utilities.h"
#include "power.h"

static powerbag **ppbagproxy = NULL;
static int numworkersproxy = 0;
static char *deadstatus = NULL;
static int activeworkers = 0;

int cheap_rank1perturb(int n, double *scratch, double *matcopy, double *matrix, double scale);

void *PWR_wrapper(void *pvoidedbag);
void (*sigset(int sig, void (*disp)(int)))(int);

void handlesigint(int i);


int main(int argc, char *argv[])
{
  int code = 0, i, j, n, initialruns, scheduledjobs;
  powerbag **ppbag = NULL, *pbag;
  double *matcopy = NULL, *scratch = NULL;
  double scale = 1.0;
  int quantity = 1, numworkers = 1, theworker; 
  int eigvaluecount =1;
  char gotone;
  double tempsum;

  pthread_t *pthethread1;
  pthread_mutex_t output;
  pthread_mutex_t *psynchro_array;

  //new variables
  int read_n;
  double* read_matrix;

  if(argc < 2){ 
    printf(" usage: rpower filename [-s scale] [-q quantity]\n");
    code = 1; goto BACK;
  }

  sigset(SIGINT, &handlesigint);

  for(j = 2; j < argc; j++){
    if (0 == strcmp(argv[j],"-s")){
      j += 1;
      scale = atof(argv[j]);
    }
    else if (0 == strcmp(argv[j],"-q")){
      j += 1;
      quantity = atoi(argv[j]);
    }
    else if (0 == strcmp(argv[j],"-w")){
      j += 1;
      numworkers = atoi(argv[j]);
    }
    else if (0 == strcmp(argv[j],"-e")){
      j += 1;
      eigvaluecount = atoi(argv[j]);
    }
    else{
      printf("bad option %s\n", argv[j]); code = 1; goto BACK;
    }
  }

  printf("will use scale %g and quantity %d: %d workers to find %d eigenvalues\n", scale, quantity,
	 numworkers,eigvaluecount);

  if( numworkers > quantity ){
    numworkers = quantity; printf(" --> reset workers to %d\n", numworkers);
  }

  deadstatus = (char *) calloc(numworkers, sizeof(char));

  for (int eig = 0; eig <eigvaluecount; ++eig)
  {
  pthread_mutex_init(&output, NULL); /** common to everybody **/


  psynchro_array = (pthread_mutex_t *)calloc(numworkers, sizeof(pthread_mutex_t));
  if(!psynchro_array){
    printf("could not create mutex array\n"); code = NOMEMORY; goto BACK;
  }

  for(j = 0; j < numworkers; j++)
    pthread_mutex_init(&psynchro_array[j], NULL);

  ppbag = (powerbag **)calloc(numworkers, sizeof(powerbag *));
  if(!ppbag){
    printf("could not create bag array\n"); code = NOMEMORY; goto BACK;
  }
  numworkersproxy = numworkers;
  ppbagproxy = ppbag;

  pthethread1 = (pthread_t *)calloc(numworkers, sizeof(pthread_t));
  if(!pthethread1){
    printf("could not create thread array\n"); code = NOMEMORY; goto BACK;
  }

  code = PWRread(argv[1],&read_n,&read_matrix);   


  //for loop for number of eigenvalues
  for (int eig = 0; eig < eigvaluecount; ++eig)
  {
  printf("***********************************************************************\n");    
  printf("                           EIGENVALUE %d                               \n",eig+1);
  printf("***********************************************************************\n");  
  for(j = 0; j < numworkers; j++){
    //if((code = PWRreadnload_new(argv[1], 0, ppbag + j)))
    /*  goto BACK;*/  /** inefficient: we should read the data once, and then
		      copy **/        
    if(eig==0) code = PWRload(j,&read_n,&read_matrix,ppbag + j);

    pbag = ppbag[j];
    pbag->psynchro = &psynchro_array[j];
    pbag->poutputmutex = &output;
    pbag->command = STANDBY;
    pbag->status = PREANYTHING;
    pbag->ID = j;

    n = pbag->n;

  /** now, allocate an extra matrix and a vector to use in 
      perturbation**/
    /** should really do it in the power code since we are putting them in
	the bag **/
    pbag->matcopy = (double *)calloc(n*n, sizeof(double));
    pbag->scratch = (double *)calloc(n, sizeof(double));
    if((!pbag->matcopy) || (!pbag->scratch)){
      printf("no memory for matrices at %d\n", j); code = NOMEMORY; goto BACK;
    }
  /** and copy matrix **/
    for(i = 0; i < n*n; i++)
      pbag->matcopy[i] = pbag->matrix[i];

    printf("about to launch thread for worker %d\n", j);

    pthread_create(&pthethread1[j], NULL, &PWR_wrapper, (void *) pbag);
  }

  initialruns = numworkers;
  if (initialruns > quantity) initialruns = quantity;

  for(theworker = 0; theworker < initialruns; theworker++){
    pbag = ppbag[theworker];
    /*    if((code = cheap_rank1perturb(n, pbag->scratch, pbag->matcopy, pbag->matrix, scale)))
	  goto BACK;*/

    pthread_mutex_lock(&output);
    printf("*****master:  worker %d will run experiment %d\n", theworker, j);
    pthread_mutex_unlock(&output);

    /** tell the worker to work **/
    pthread_mutex_lock(&psynchro_array[theworker]);
    pbag->command = WORK;
    pbag->status = WORKING;
    pbag->jobnumber = theworker;
    pbag->itercount = 0;
    pthread_mutex_unlock(&psynchro_array[theworker]);

  }
  scheduledjobs = activeworkers = initialruns;
  scheduledjobs =activeworkers * eigvaluecount;
  while(activeworkers > 0){
    /** check the workers' status **/
    gotone = 0;
    for(theworker = 0; theworker < numworkers; theworker++){

      pthread_mutex_lock(&psynchro_array[theworker]);
      pbag = ppbag[theworker];
      if(pbag->status == DONEWITHWORK){

	pthread_mutex_lock(&output);
	printf("master:  worker %d is done with job %d\n", pbag->ID, pbag->jobnumber);
	printf(" eigenvalue estimate (%d): %.12e\n",eig,pbag->topeigvalue);
	pthread_mutex_unlock(&output);

	if(scheduledjobs >= quantity){
	  /** tell worker to quit **/
	  pthread_mutex_lock(&output);
	  printf("master: telling worker %d to quit\n", pbag->ID);
	  pthread_mutex_unlock(&output);
	  pbag->command = QUIT;
	  pbag->status = QUIT;
	  --activeworkers;
	}
	else {
	  gotone = 1;
	}
      }
      else if(pbag->status == PREANYTHING){
	pthread_mutex_lock(&output);
	printf("master:  worker %d is available\n", theworker);
	pthread_mutex_unlock(&output);
	gotone = 1;
      }
      else if( (pbag->status == WORKING) && (pbag->itercount > 100000)){
	pbag->command = INTERRUPT;
	pthread_mutex_lock(&output);
	printf("master: telling worker %d to interrupt\n", pbag->ID);
	pthread_mutex_unlock(&output);
      }
      else if(deadstatus[theworker]){
	pthread_mutex_lock(&output);
	printf("master: telling worker %d to quit\n", pbag->ID);
	pthread_mutex_unlock(&output);
	pbag->command = QUIT;
	pbag->status = QUIT;
	--activeworkers;
	/** and let's make sure we don't do it again **/
	deadstatus[theworker] = 0;
      }
      pthread_mutex_unlock(&psynchro_array[theworker]);
      if(gotone) break;
      usleep(10000);
      
    }
    /** at this point we have run through all workers **/

    if(gotone){
    /** if we are here, "theworker" can work **/
      pbag = ppbag[theworker];
      if((code = cheap_rank1perturb(n, pbag->scratch, pbag->matcopy, pbag->matrix, scale)))
	goto BACK;

      pthread_mutex_lock(&output);
      printf("master:  worker %d will run experiment %d\n", theworker, scheduledjobs);
      pthread_mutex_unlock(&output);


      /** tell the worker to work **/
      pthread_mutex_lock(&psynchro_array[theworker]);
      pbag->command = WORK;
      pbag->status = WORKING;
      pbag->itercount = 0;
      pbag->jobnumber = scheduledjobs;
      tempsum=0;
    if (eig>0)
    {    
    //Q'
    for(int cnt1 = 0; cnt1 < n; cnt1++)
      for(int cnt2 = 0; cnt2 < n; cnt2++){
        pbag->matrix[cnt1*n + cnt2]+= - pbag->topeigvalue * pbag->vector[cnt1]*pbag->vector[cnt2];
        //first part of w'(0). Set vector to w(0)
        pbag->newvector[cnt2]=pbag->vector[cnt2];
      }
    //w'(0)
      for(int cnt2 = 0; cnt2 < n; cnt2++)
          tempsum+=pbag->newvector[cnt2] * pbag->W0[cnt2];

      for(int cnt2 = 0; cnt2 < n; cnt2++)
       pbag->vector[cnt2] =  pbag->W0[cnt2] - tempsum * pbag->newvector[cnt2];
    }
  

      pthread_mutex_unlock(&psynchro_array[theworker]);

      ++scheduledjobs;
    }
  }



  /*  pthread_mutex_lock(&psynchro_array[theworker]);
  pbag->command = QUIT;
  pthread_mutex_unlock(&psynchro_array[theworker]);*/

  pthread_mutex_lock(&output);
  printf("master:  done with loop\n");
  pthread_mutex_unlock(&output);

  /** actually this is bad -- should wait for the threads to be done --
      but how **/
  for(j = 0; j < numworkers; j++){
    pthread_join(pthethread1[j], NULL);
    pthread_mutex_lock(&output);
    printf("master: joined with thread %d\n", j);
    pthread_mutex_unlock(&output);
    //pbag = ppbag[j];
    //PWRfreespace(&pbag);
  }
  //free(ppbag);
}	 
BACK:
  if(matcopy) free(matcopy);
  if(scratch) free(scratch);
  return code;
}
for(j = 0; j < numworkers; j++){
  pbag = ppbag[j];
  PWRfreespace(&pbag);
}
free(ppbag);  
}



int cheap_rank1perturb(int n, double *scratch, double *matcopy, double *matrix, double scale)
{
  int retcode = 0, j, i;
  double sum2, invnorm;

  /** first, create a random vector **/
  for(j = 0; j < n; j++)
    scratch[j] = ((double) rand())/((double) RAND_MAX);

  /** next, convert to norm 1 **/
  sum2 = 0;
  for(j = 0; j < n; j++)
    sum2 += scratch[j]*scratch[j];

  invnorm = 1/sqrt(sum2);

  /** rescale **/
  for(j = 0; j < n; j++)
    scratch[j] *= scale*invnorm;


  printf("scale for random perturbation: %g\n", scale);

  for(i = 0; i < n; i++)
    for(j = 0; j < n; j++)
      matrix[i*n + j] = scratch[i]*scratch[j] + matcopy[i*n + j];

  return retcode;
}

void *PWR_wrapper(void *pvoidedbag)
{
  powerbag *pbag = (powerbag *) pvoidedbag;

  PWRpoweralg_new(pbag);

  return (void *) &pbag->ID;
}



void handlesigint(int signal)
{
  int j;
  printf("yo, what's happening\n");
  for(j = 0; j < numworkersproxy; j++){
    deadstatus[j] = 1;
  }
  /** brutal **/
}

















