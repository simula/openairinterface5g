/*
* Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
* contributor license agreements.  See the NOTICE file distributed with
* this work for additional information regarding copyright ownership.
* The OpenAirInterface Software Alliance licenses this file to You under
* the OAI Public License, Version 1.1  (the "License"); you may not use this file
* except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.openairinterface.org/?page_id=698
*
* Author and copyright: Laurent Thomas, open-cells.com
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*-------------------------------------------------------------------------------
* For more information about the OpenAirInterface (OAI) Software Alliance:
*      contact@openairinterface.org
*/


#define _GNU_SOURCE
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <threadPool/thread-pool.h>


static inline  notifiedFIFO_elt_t *pullNotifiedFifoRemember( notifiedFIFO_t *nf, struct one_thread *thr) {
  mutexlock(nf->lockF);

  while(!nf->outF && !thr->terminate)
    condwait(nf->notifF, nf->lockF);

  if (thr->terminate) {
    mutexunlock(nf->lockF);
    return NULL;
  }

  notifiedFIFO_elt_t *ret=nf->outF;
  nf->outF=nf->outF->next;

  if (nf->outF==NULL)
    nf->inF=NULL;

  // For abort feature
  thr->runningOnKey=ret->key;
  thr->dropJob = false;
  mutexunlock(nf->lockF);
  return ret;
}

void *one_thread(void *arg) {
  struct  one_thread *myThread=(struct  one_thread *) arg;
  struct  thread_pool *tp=myThread->pool;

  // Infinite loop to process requests
  do {
    notifiedFIFO_elt_t *elt=pullNotifiedFifoRemember(&tp->incomingFifo, myThread);
    if (elt == NULL) {
      AssertFatal(myThread->terminate, "pullNotifiedFifoRemember() returned NULL although thread not aborted\n");
      break;
    }

    if (tp->measurePerf) elt->startProcessingTime=rdtsc_oai();

    elt->processingFunc(NotifiedFifoData(elt));

    if (tp->measurePerf) elt->endProcessingTime=rdtsc_oai();

    if (elt->reponseFifo) {
      // Check if the job is still alive, else it has been aborted
      mutexlock(tp->incomingFifo.lockF);

      if (myThread->dropJob)
        delNotifiedFIFO_elt(elt);
      else
        pushNotifiedFIFO(elt->reponseFifo, elt);
      myThread->runningOnKey=-1;
      mutexunlock(tp->incomingFifo.lockF);
    }
    else
      delNotifiedFIFO_elt(elt);
  } while (!myThread->terminate);
  return NULL;
}

void initNamedTpool(char *params,tpool_t *pool, bool performanceMeas, char *name) {
  memset(pool,0,sizeof(*pool));
  char *measr=getenv("OAI_THREADPOOLMEASUREMENTS");
  pool->measurePerf=performanceMeas;
  // force measurement if the output is defined
  pool->measurePerf |= measr!=NULL;

  if (measr) {
    mkfifo(measr,0666);
    AssertFatal(-1 != (pool->dummyKeepReadingTraceFd=
                         open(measr, O_RDONLY| O_NONBLOCK)),"");
    AssertFatal(-1 != (pool->traceFd=
                         open(measr, O_WRONLY|O_APPEND|O_NOATIME|O_NONBLOCK)),"");
  } else
    pool->traceFd=-1;

  pool->activated=true;
  initNotifiedFIFO(&pool->incomingFifo);
  char *saveptr, * curptr;
  char *parms_cpy=strdup(params);
  pool->nbThreads=0;
  curptr=strtok_r(parms_cpy,",",&saveptr);
  struct one_thread * ptr;
  char *tname = (name == NULL ? "Tpool" : name);
  while ( curptr!=NULL ) {
    int c=toupper(curptr[0]);

    switch (c) {

      case 'N':
        pool->activated=false;
        break;

      default:
        ptr=pool->allthreads;
        pool->allthreads=(struct one_thread *)malloc(sizeof(struct one_thread));
        pool->allthreads->next=ptr;
        pool->allthreads->coreID=atoi(curptr);
        pool->allthreads->id=pool->nbThreads;
        pool->allthreads->pool=pool;
        pool->allthreads->dropJob = false;
        pool->allthreads->terminate = false;
        //Configure the thread scheduler policy for Linux
        // set the thread name for debugging
        sprintf(pool->allthreads->name,"%s%d_%d",tname,pool->nbThreads,pool->allthreads->coreID);
        // we set the maximum priority for thread pool threads (which is close
        // but not equal to Linux maximum). See also the corresponding commit
        // message; initially introduced for O-RAN 7.2 fronthaul split
        threadCreate(&pool->allthreads->threadID, one_thread, (void *)pool->allthreads,
                     pool->allthreads->name, pool->allthreads->coreID, OAI_PRIORITY_RT_MAX);
        pool->nbThreads++;
    }

    curptr=strtok_r(NULL,",",&saveptr);
  }
  free(parms_cpy);
  if (pool->activated && pool->nbThreads==0) {
    printf("No servers created in the thread pool, exit\n");
    exit(1);
  }
}

void initFloatingCoresTpool(int nbThreads,tpool_t *pool, bool performanceMeas, char *name) {
  char threads[1024] = "n";
  if (nbThreads) {
    strcpy(threads,"-1");
    for (int i=1; i < nbThreads; i++)
      strncat(threads,",-1", sizeof(threads)-1);
  }
  threads[sizeof(threads)-1]=0;
  initNamedTpool(threads, pool, performanceMeas, name);
}
