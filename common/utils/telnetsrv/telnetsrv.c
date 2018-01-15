/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
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

/*! \file common/utils/telnetsrv/telnetsrv.c
 * \brief: implementation of a telnet server
 * \author Francois TABURET
 * \date 2017
 * \version 0.1
 * \company NOKIA BellLabs France
 * \email: francois.taburet@nokia-bell-labs.com
 * \note
 * \warning
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <telnetsrv.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "common/config/config_userapi.h"


#include "telnetsrv_phycmd.h"
#include "telnetsrv_proccmd.h"	
static char* telnet_defstatmod[] = {"softmodem","phy"}; 
static telnetsrv_params_t telnetparams;
#define TELNETSRV_LISTENADDR 0
#define TELNETSRV_LISTENPORT 1
#define TELNETSRV_PRIORITY   2
#define TELNETSRV_DEBUG      3
#define TELNETSRV_STATICMOD  7
#define TELNETSRV_SHRMOD     8
paramdef_t telnetoptions[] = {
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            configuration parameters for telnet utility                                                                             */
/*   optname                     helpstr                paramflags           XXXptr                               defXXXval               type                 numelt */
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
	{"listenaddr",    "<listen ip address>",         0,                 uptr:&telnetparams.listenaddr,        defstrval:"0.0.0.0",            TYPE_IPV4ADDR,  0 },
	{"listenport",    "<local port>",                0,                 uptr:&(telnetparams.listenport),      defuintval:9090,                TYPE_UINT,      0 },
        {"priority",      "<scheduling policy (0-99)",   0,                 uptr:&telnetparams.priority,          defuintval:0,                   TYPE_INT,       0 }, 
	{"debug",         "<debug level>",               0,                 uptr:NULL,                            defuintval:0,                   TYPE_UINT,      0 },
	{"loopcount",     "<loop command iterations>",   0,                 uptr:&(telnetparams.loopcount),       defuintval:10,                  TYPE_UINT,      0 },
	{"loopdelay",     "<loop command delay (ms)>",   0,                 uptr:&(telnetparams.loopdelay),       defuintval:5000,                TYPE_UINT,      0 },
	{"phypbsize",     "<phy dump buff size (bytes)>",0,                 uptr:&(telnetparams.phyprntbuff_size),defuintval:65000,               TYPE_UINT,      0 },
        {"staticmod",     "<static modules selection>",  0,                 NULL,                                 defstrlistval:telnet_defstatmod,TYPE_STRINGLIST,1},
        {"shrmod",        "<static modules selection>",  0,                 NULL,                                 NULL,TYPE_STRINGLIST,0 },
};

int get_phybsize() {return telnetparams.phyprntbuff_size; };
int add_telnetcmd(char *modulename,telnetshell_vardef_t *var, telnetshell_cmddef_t *cmd );
int setoutput(char *buff, int debug, telnet_printfunc_t prnt);
int setparam(char *buff, int debug, telnet_printfunc_t prnt);

telnetshell_vardef_t telnet_vardef[] = {
{"debug",TELNET_VARTYPE_INT32,&telnetparams.telnetdbg},
{"prio",TELNET_VARTYPE_INT32,&telnetparams.priority},
{"loopc",TELNET_VARTYPE_INT32,&telnetparams.loopcount},
{"loopd",TELNET_VARTYPE_INT32,&telnetparams.loopdelay},
{"phypb",TELNET_VARTYPE_INT32,&telnetparams.phyprntbuff_size},
{"",0,NULL}
};

telnetshell_cmddef_t  telnet_cmdarray[] = {
   {"redirlog","[here,file,off]",setoutput},
   {"param","[prio]",setparam},
   {"","",NULL},
};


void client_printf(const char *message, ...)
{
    va_list va_args;
    
    va_start(va_args, message);
    if (telnetparams.new_socket > 0)
       {
       vsnprintf(telnetparams.msgbuff,sizeof(telnetparams.msgbuff)-1,message, va_args);
       send(telnetparams.new_socket,telnetparams.msgbuff , strlen(telnetparams.msgbuff), MSG_NOSIGNAL);
       }
    else
       {
       vprintf(message, va_args);
       }
    va_end(va_args);
    return ;
}

#define NICE_MAX 19
#define NICE_MIN -20
void set_sched(pthread_t tid, int pid, int priority)
{
int rt; 
struct sched_param schedp;
int policy;
char strpolicy[10];


//sched_get_priority_max(SCHED_FIFO)
if (priority < NICE_MIN)
   {
   policy=SCHED_FIFO;
   sprintf(strpolicy,"%s","fifo");
   schedp.sched_priority= NICE_MIN - priority ;   
   }
else if (priority > NICE_MAX)
   {
   policy=SCHED_IDLE;
   sprintf(strpolicy,"%s","idle");
   schedp.sched_priority=0;   
   } 
else 
   {
   policy=SCHED_OTHER;
   sprintf(strpolicy,"%s","other");
   schedp.sched_priority=0;   
   } 
if( tid != 0)
  {  
  rt = pthread_setschedparam(tid, policy, &schedp);
  }
else if(pid > 0)
  {
  rt = sched_setscheduler( pid, policy,&schedp);
  }
if (rt != 0)
    {
    client_printf("Error %i: %s modifying sched param to %s:%i, \n",
                  errno,strerror(errno),strpolicy,schedp.sched_priority); 
    }
else
    {
    client_printf("policy set to %s, priority %i\n",strpolicy,schedp.sched_priority);
    }



if ( policy == SCHED_OTHER)
   {
   if ( tid > 0 && tid != pthread_self())
     {
     client_printf("setting nice value using a thread id not implemented....\n");    
     }
   else if (pid > 0)
     {
     errno=0;
     rt = setpriority(PRIO_PROCESS,pid,priority);
     if (rt != 0)
        {
        client_printf("Error %i: %s calling setpriority, \n",errno,strerror(errno)); 
        }
     else
        {
        client_printf("nice value set to %i\n",priority);
        }
     }
   }
}

void set_affinity(pthread_t tid, int pid, int coreid)
{
cpu_set_t cpuset;
int rt;

  CPU_ZERO(&cpuset);
  CPU_SET(coreid, &cpuset);
  if (tid > 0)
     {
     rt = pthread_setaffinity_np((pthread_t)tid, sizeof(cpu_set_t), &cpuset);
     }
  else if (pid > 0)
     {
     rt = sched_setaffinity((pid_t)pid, sizeof(cpu_set_t), &cpuset);
     }
  if (rt != 0)
      {
      client_printf("Error %i: %s calling , xxx_setaffinity...\n",errno,strerror(errno)); 
      }
  else
      {
      client_printf("thread %i affinity set to %i\n",(pid==0)?(int)tid:pid,coreid);
      }  
}
/*------------------------------------------------------------------------------------*/
/*
function implementing telnet server specific commands, parameters of the
telnet_cmdarray table
*/

void redirstd(char *newfname,telnet_printfunc_t prnt )
{
FILE *fd;
   fd=freopen(newfname, "w", stdout);
   if (fd == NULL)
      {
      prnt("ERROR: stdout redir to %s error %s",strerror(errno));
      }
   fd=freopen(newfname, "w", stderr);
   if (fd == NULL)
      {
      prnt("ERROR: stderr redir to %s error %s",strerror(errno));
      }
}
int setoutput(char *buff, int debug, telnet_printfunc_t prnt)
{

char cmds[TELNET_MAX_MSGLENGTH/TELNET_CMD_MAXSIZE][TELNET_CMD_MAXSIZE];
char *logfname;
char stdout_str[64];


#define LOGFILE "logfile.log"
memset(cmds,0,sizeof(cmds));
sscanf(buff,"%9s %32s %9s %9s %9s", cmds[0],cmds[1],cmds[2],cmds[3],cmds[4]  );
if (strncasecmp(cmds[0],"here",4) == 0)
   {
   fflush(stdout);
   sprintf(stdout_str,"/proc/%i/fd/%i",getpid(),telnetparams.new_socket);
   dup2(telnetparams.new_socket,fileno(stdout));
//   freopen(stdout_str, "w", stdout);
//   freopen(stdout_str, "w", stderr);
   dup2(telnetparams.new_socket,fileno(stderr));
   prnt("Log output redirected to this terminal (%s)\n",stdout_str);
   }
if (strncasecmp(cmds[0],"file",4) == 0)
   {
   if (cmds[1][0] == 0)
      logfname=LOGFILE;
   else
      logfname=cmds[1];   
   fflush(stdout);
   redirstd(logfname,prnt);

   }   
if (strncasecmp(cmds[0],"off",3) == 0)
   {
   fflush(stdout);
   redirstd("/dev/tty",prnt); 
   } 

return CMDSTATUS_FOUND;   
} /* setoutput */

int setparam(char *buff, int debug, telnet_printfunc_t prnt)
{
char cmds[TELNET_MAX_MSGLENGTH/TELNET_CMD_MAXSIZE][TELNET_CMD_MAXSIZE];


memset(cmds,0,sizeof(cmds));
sscanf(buff,"%9s %9s %9s %9s %9s", cmds[0],cmds[1],cmds[2],cmds[3],cmds[4]  );
if (strncasecmp(cmds[0],"prio",4) == 0)
   {
   pthread_attr_t attr;
   int prio;
   prio=(int)strtol(cmds[1],NULL,0);
   if (errno == ERANGE)
       return CMDSTATUS_VARNOTFOUND;
   telnetparams.priority = prio;
   set_sched(pthread_self(),0,prio);
   return CMDSTATUS_FOUND; 
   }
if (strncasecmp(cmds[0],"aff",3) == 0)
   {
   int aff;
   aff=(int)strtol(cmds[1],NULL,0);
   if (errno == ERANGE)
       return CMDSTATUS_VARNOTFOUND;
   set_affinity(pthread_self(),0,aff);
   return CMDSTATUS_FOUND; 
   }

return CMDSTATUS_NOTFOUND;   
} /* setparam */
/*-------------------------------------------------------------------------------------------------------*/
/*
generic commands available for all modules loaded by the server
*/

int setgetvar(int moduleindex,char getorset,char *params)
{
int n,i;
char varname[TELNET_CMD_MAXSIZE];
char varval[TELNET_CMD_MAXSIZE];

   memset(varname,0,sizeof(varname));
   memset(varval,0,sizeof(varval));
   n = sscanf(params,"%s %s",varname,varval);
   for ( i=0 ; telnetparams.CmdParsers[moduleindex].var[i].varvalptr != NULL ; i++)
      {
      if ( strncasecmp(telnetparams.CmdParsers[moduleindex].var[i].varname,varname,strlen(telnetparams.CmdParsers[moduleindex].var[i].varname)) == 0)
         {
	 if (n > 0 && (getorset == 'g' || getorset == 'G'))
	    {
	    client_printf("%s, %s = ", telnetparams.CmdParsers[moduleindex].module,
	               telnetparams.CmdParsers[moduleindex].var[i].varname );
	    switch(telnetparams.CmdParsers[moduleindex].var[i].vartype)
	        {
		case TELNET_VARTYPE_INT32:
	             client_printf("%i\n",*(int *)(telnetparams.CmdParsers[moduleindex].var[i].varvalptr));
		break;
		case TELNET_VARTYPE_INT16:
	             client_printf("%hi\n",*(short *)(telnetparams.CmdParsers[moduleindex].var[i].varvalptr));
		break;	
		case TELNET_VARTYPE_DOUBLE:
	             client_printf("%g\n",*(double *)(telnetparams.CmdParsers[moduleindex].var[i].varvalptr));
		break;
		case TELNET_VARTYPE_PTR:
	             client_printf("0x%08x\n",*((unsigned int *)(telnetparams.CmdParsers[moduleindex].var[i].varvalptr)));						
                break;
		default:
		     client_printf("unknown type\n");
		break;
		}
	    }
	 if (n > 1 && (getorset == 's' || getorset == 'S'))
	    {
	    client_printf("%s, %s set to \n", telnetparams.CmdParsers[moduleindex].module,
	           telnetparams.CmdParsers[moduleindex].var[i].varname);
		   
	    switch(telnetparams.CmdParsers[moduleindex].var[i].vartype)
	        {
		case TELNET_VARTYPE_INT32:
		     *(int *)(telnetparams.CmdParsers[moduleindex].var[i].varvalptr) = (int)strtol(varval,NULL,0);
	             client_printf("%i\n",*(int *)(telnetparams.CmdParsers[moduleindex].var[i].varvalptr));
		break;
		case TELNET_VARTYPE_INT16:
		     *(short *)(telnetparams.CmdParsers[moduleindex].var[i].varvalptr) = (short)strtol(varval,NULL,0);
	             client_printf("%hi\n",*(short *)(telnetparams.CmdParsers[moduleindex].var[i].varvalptr));
		break;	
		case TELNET_VARTYPE_DOUBLE:
		     *(double *)(telnetparams.CmdParsers[moduleindex].var[i].varvalptr) = strtod(varval,NULL);
	             client_printf("%g\n",*(double *)(telnetparams.CmdParsers[moduleindex].var[i].varvalptr));
		break;				
		default:
		     client_printf("unknown type\n");
		break;
		}		   
	    }		   
	 }
      } 
return CMDSTATUS_VARNOTFOUND;
}
/*----------------------------------------------------------------------------------------------------*/
char *get_time(char *buff,int bufflen)
{

struct tm  tmstruct;
time_t now = time (0);
strftime (buff, bufflen, "%Y-%m-%d %H:%M:%S.000", localtime_r(&now,&tmstruct));
return buff;
}

int process_command(char *buf)
{
int i,j,k;
char modulename[TELNET_CMD_MAXSIZE];
char cmd[TELNET_CMD_MAXSIZE];
char cmdb[TELNET_MAX_MSGLENGTH];
char *bufbck;
int rt;

memset(modulename,0,sizeof(modulename));
memset(cmd,0,sizeof(cmd));
memset(cmdb,0,sizeof(cmdb));
if (strncasecmp(buf,"ex",2) == 0)
   return CMDSTATUS_EXIT;

if (strncasecmp(buf,"help",4) == 0)
   {
   for (i=0; telnetparams.CmdParsers[i].var != NULL && telnetparams.CmdParsers[i].cmd != NULL; i++)
    {
     client_printf("   module %i = %s:\n",i,telnetparams.CmdParsers[i].module);
     for(j=0; telnetparams.CmdParsers[i].var[j].varvalptr != NULL ; j++)
        {
	client_printf("      %s [get set] %s <value>\n",
	       telnetparams.CmdParsers[i].module, telnetparams.CmdParsers[i].var[j].varname);
	} 
     for(j=0; telnetparams.CmdParsers[i].cmd[j].cmdfunc != NULL ; j++)
        {
	client_printf("      %s %s %s\n",
	       telnetparams.CmdParsers[i].module,telnetparams.CmdParsers[i].cmd[j].cmdname,
	       telnetparams.CmdParsers[i].cmd[j].helpstr);
	} 	
    }
   return CMDSTATUS_FOUND;
   }

memset(modulename,0,sizeof(modulename));
memset(cmd,0,sizeof(cmd));
memset(cmdb,0,sizeof(cmdb));  
bufbck=strdup(buf); 
rt=CMDSTATUS_NOTFOUND;   
j = sscanf(buf,"%9s %9s %[^\t\n]",modulename,cmd,cmdb);
if (telnetparams.telnetdbg > 0)
    printf("process_command: %i words, module=%s cmd=%s, parameters= %s\n",j,modulename,cmd,cmdb);   
for (i=0; j>=2 && telnetparams.CmdParsers[i].var != NULL && telnetparams.CmdParsers[i].cmd != NULL; i++)
    {
    if ( (strncasecmp(telnetparams.CmdParsers[i].module,modulename,strlen(telnetparams.CmdParsers[i].module)) == 0))
       {
        if (strncasecmp(cmd,"getall",7) == 0 )
	    { 
            for(j=0; telnetparams.CmdParsers[i].var[j].varvalptr != NULL ; j++)
               {
	       setgetvar(i,'g',telnetparams.CmdParsers[i].var[j].varname);
	       }
	    rt= CMDSTATUS_FOUND;              
	    }       
        else if (strncasecmp(cmd,"get",3) == 0 || strncasecmp(cmd,"set",3) == 0)
	    {
            rt= setgetvar(i,cmd[0],cmdb);	    
	    }	    
        else
	    {
	    for (k=0 ; telnetparams.CmdParsers[i].cmd[k].cmdfunc != NULL ; k++)
	        {
	        if (strncasecmp(cmd, telnetparams.CmdParsers[i].cmd[k].cmdname,sizeof(telnetparams.CmdParsers[i].cmd[k].cmdname)) == 0)
                   {
	           telnetparams.CmdParsers[i].cmd[k].cmdfunc(cmdb, telnetparams.telnetdbg, client_printf);
                   rt= CMDSTATUS_FOUND;
	           }
		} /* for k */
	    }/* else */
	}/* strncmp: module name test */
     else if (strncasecmp(modulename,"loop",4) == 0 )
	{
	int lc;
        int f = fcntl(telnetparams.new_socket,F_GETFL);
	fcntl (telnetparams.new_socket, F_SETFL, O_NONBLOCK | f);
	for(lc=0; lc<telnetparams.loopcount; lc++)
	   {
	   char dummybuff[20];
	   char tbuff[64];
       int rs;
	   client_printf(CSI "1J" CSI "1;10H         " STDFMT "%s %i/%i\n",
	                 get_time(tbuff,sizeof(tbuff)),lc,telnetparams.loopcount );  
       process_command(bufbck+strlen("loop")+1);
       usleep(telnetparams.loopdelay * 1000);
	   rs = read(telnetparams.new_socket,dummybuff,sizeof(dummybuff)); 
	   if ( rs > 0 )
	       {
	       break;
	       }
	   }
	fcntl (telnetparams.new_socket, F_SETFL, f);
	rt= CMDSTATUS_FOUND;	    
	} /* loop */
    } /* for i */
free(bufbck);
return rt;
}

void run_telnetsrv(void) 
{
int sock;
struct sockaddr_in name;
char buf[TELNET_MAX_MSGLENGTH];
struct sockaddr cli_addr;
unsigned int cli_len = sizeof(cli_addr);
int readc , filled;

int status;
int optval = 1;

pthread_setname_np(pthread_self(), "telnet");
set_sched(pthread_self(),0,telnetparams.priority);
sock = socket(AF_INET, SOCK_STREAM, 0);
if (sock < 0) 
    fprintf(stderr,"[TELNETSRV] Error %s on socket call\n",strerror(errno));

setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
name.sin_family = AF_INET;
if (telnetparams.listenaddr == 0)
    name.sin_addr.s_addr = INADDR_ANY;
else
    name.sin_addr.s_addr = telnetparams.listenaddr;
name.sin_port = htons((unsigned short)(telnetparams.listenport));

if(bind(sock, (void*) &name, sizeof(name))) 
     fprintf(stderr,"[TELNETSRV] Error %s on bind call\n",strerror(errno));
if(listen(sock, 1) == -1)
     fprintf(stderr,"[TELNETSRV] Error %s on listen call\n",strerror(errno));



printf("\nInitializing telnet server...\n");
while( (telnetparams.new_socket = accept(sock, &cli_addr, &cli_len)) )
     {
     printf("[TELNETSRV] Telnet client connected....\n");


    if(telnetparams.new_socket < 0)
       fprintf(stderr,"[TELNETSRV] Error %s on accept call\n",strerror(errno));

    while(telnetparams.new_socket>0)
         {
	 filled = 0;
	 memset(buf,0,sizeof(buf));
         while(filled < ( TELNET_MAX_MSGLENGTH-1))
	     {
             readc = recv(telnetparams.new_socket, buf+filled, TELNET_MAX_MSGLENGTH-filled-1, 0);
             if(!readc)
	        break;
             filled += readc;
             if(buf[filled-1] == '\n')
	        {
		buf[filled-1] = 0;
		
	        break;
		}
             }
         if(!readc)
	   {
           printf ("[TELNETSRV] Telnet Client disconnected.\n");
           break;
           }
         if (telnetparams.telnetdbg > 0)
	    printf("[TELNETSRV] Command received: readc %i filled %i %s\n", readc, filled ,buf);
	 if (strlen(buf) >= 2 )
	    {
            status=process_command(buf);
	    }
	 else
	    status=CMDSTATUS_NOCMD;
	    
         if (status != CMDSTATUS_EXIT)
	    {
	    if (status == CMDSTATUS_NOTFOUND)
	       {
	       char msg[TELNET_MAX_MSGLENGTH + 50];
	       sprintf(msg,"Error: \n      %s\n is not a softmodem command\n",buf);
	       send(telnetparams.new_socket, msg, strlen(msg), MSG_NOSIGNAL);
	       }
            send(telnetparams.new_socket, TELNET_PROMPT, sizeof(TELNET_PROMPT), MSG_NOSIGNAL);
	    }
	 else
	    {
	    printf ("[TELNETSRV] Closing telnet connection...\n");
	    break;
	    }
    }

    close(telnetparams.new_socket);
    printf ("[TELNETSRV] Telnet server waitting for connection...\n");
    }
close(sock);
return;
}

/*------------------------------------------------------------------------------------------------*/
/* set_telnetmodule loads the commands delivered with the telnet server
 *
 *
 * 
*/
void exec_moduleinit(char *modname)
{
void (*fptr)();
char initfunc[TELNET_CMD_MAXSIZE+9];

       if (strlen(modname) > TELNET_CMD_MAXSIZE)
	  {
          fprintf(stderr,"[TELNETSRV] module %s not loaded, name exceeds the %i size limit\n",
			 modname, TELNET_CMD_MAXSIZE);
	  return; 
          }
       sprintf(initfunc,"add_%s_cmds",modname);
       fptr = dlsym(RTLD_DEFAULT,initfunc);
       if ( fptr != NULL)
          {
          fptr();
          }
       else
          {
          fprintf(stderr,"[TELNETSRV] couldn't find %s for module %s \n",initfunc,modname);
          }
}

int add_embeddedmodules()
{




    for(int i=0; i<telnetoptions[TELNETSRV_STATICMOD].numelt;i++)
       {
       exec_moduleinit(telnetoptions[TELNETSRV_STATICMOD].strlistptr[i]);
       }
}

int add_sharedmodules()
{
char initfunc[TELNET_CMD_MAXSIZE+9];
void (*fptr)();


    for(int i=0; i<telnetoptions[TELNETSRV_SHRMOD].numelt;i++)
       {
       sprintf(initfunc,"add_%s_cmds",telnetoptions[TELNETSRV_SHRMOD].strlistptr[i]);
       fptr = dlsym(RTLD_DEFAULT,initfunc);
       if ( fptr != NULL)
          {
          fptr();
          }
       else
          {
          fprintf(stderr,"[TELNETSRV] couldn't find %s for module %s \n",initfunc,telnetoptions[TELNETSRV_STATICMOD].strlistptr[i]);
          }
       }
}

int init_telnetsrv(char *cfgfile)
 {
  void *lib_handle;
  char** moduleslist; 

   memset(&telnetparams,0,sizeof(telnetparams));

   config_get( telnetoptions,sizeof(telnetoptions)/sizeof(paramdef_t),NULL); 

 
   if(pthread_create(&telnetparams.telnet_pthread,NULL, (void *(*)(void *))run_telnetsrv, NULL) != 0)
     {
     fprintf(stderr,"[TELNETSRV] Error %s on pthread_create call\n",strerror(errno));
     return -1;
     }
  add_telnetcmd("telnet", telnet_vardef, telnet_cmdarray);
  add_embeddedmodules();
  return 0;
 }
 
/*---------------------------------------------------------------------------------------------*/
/* add_telnetcmd is used to add a set of commands to the telnet server. A module calls this
 * function at init time. the telnet server is delivered with a set of commands which
 * will be loaded or not depending on the telnet section of the config file
*/
int add_telnetcmd(char *modulename, telnetshell_vardef_t *var, telnetshell_cmddef_t *cmd)
 {
 int i;
 if( modulename == NULL || var == NULL || cmd == NULL)
     {
     fprintf(stderr,"[TELNETSRV] Telnet server, add_telnetcmd: invalid parameters\n");
     return -1;
     }
 for (i=0; i<TELNET_MAXCMD ; i++)
     {
     if (telnetparams.CmdParsers[i].var == NULL)
        {
        strncpy(telnetparams.CmdParsers[i].module,modulename,sizeof(telnetparams.CmdParsers[i].module)-1);
        telnetparams.CmdParsers[i].cmd = cmd;
        telnetparams.CmdParsers[i].var = var;
        printf("[TELNETSRV] Telnet server: module %i = %s added to shell\n",
               i,telnetparams.CmdParsers[i].module);
        break;
        }
     }
  return 0;
 }




