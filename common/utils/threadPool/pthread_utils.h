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
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#ifndef PTHREAD_UTILS_H
#define PTHREAD_UTILS_H

#include <pthread.h>
#define mutexinit(mutex)   {int ret=pthread_mutex_init(&mutex,NULL); \
                            AssertFatal(ret==0,"ret=%d\n",ret);}
#define condinit(signal)   {int ret=pthread_cond_init(&signal,NULL); \
                            AssertFatal(ret==0,"ret=%d\n",ret);}
#define mutexlock(mutex)   {int ret=pthread_mutex_lock(&mutex); \
                            AssertFatal(ret==0,"ret=%d\n",ret);}
#define mutextrylock(mutex)   pthread_mutex_trylock(&mutex)
#define mutexunlock(mutex) {int ret=pthread_mutex_unlock(&mutex); \
                            AssertFatal(ret==0,"ret=%d\n",ret);}
#define condwait(condition, mutex) {int ret=pthread_cond_wait(&condition, &mutex); \
                                    AssertFatal(ret==0,"ret=%d\n",ret);}
#define condbroadcast(signal) {int ret=pthread_cond_broadcast(&signal); \
                               AssertFatal(ret==0,"ret=%d\n",ret);}
#define condsignal(signal)    {int ret=pthread_cond_signal(&signal); \
                               AssertFatal(ret==0,"ret=%d\n",ret);}
#define mutexdestroy(mutex) { int ret = pthread_mutex_destroy(&mutex);\
                              AssertFatal(ret==0,"ret=%d\n",ret);}
#define conddestroy(condition) { int ret = pthread_cond_destroy(&condition);\
                                 AssertFatal(ret==0,"ret=%d\n",ret);}
#endif
