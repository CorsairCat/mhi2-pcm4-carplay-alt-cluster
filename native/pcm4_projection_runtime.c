/* Original separate-process PCM4 renderer. Private plaintext Screen stream on
 * loopback only. --enable explicitly opts in; no phone keys enter this process. */
#define _POSIX_C_SOURCE 200809L
#include "pcm4_screen_receiver.h"
#include "pcm4_nv_binding.h"
#include "pcm4_ready.h"
#include "pcm4_profile.h"
#include "pcm4_identity.h"
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
static volatile sig_atomic_t quit;
static void stop(int signal_number){(void)signal_number;quit=1;}
static uint64_t milliseconds(void)
{struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return(uint64_t)t.tv_sec*1000+(unsigned)t.tv_nsec/1000000;}
static int plaintext(void *opaque,void *data,size_t size){(void)opaque;(void)data;(void)size;return 0;}
int main(int argc,char **argv)
{
    struct sockaddr_in address;int server=-1,fd=-1,rc=1,yes=1;
    struct pcm4_ready marker;struct pcm4_nv_binding binding={0};
    struct pcm4_display display={0};struct pcm4_projection p={0};struct pcm4_screen_receiver receiver;
    struct pcm4_display_config config={PCM4_CFG_OUTPUT,448,448,PCM4_CFG_PRIORITY,PCM4_CFG_LAYER,{PCM4_CFG_WIDTH,PCM4_CFG_HEIGHT,PCM4_CFG_FPS},{0,0,448,448}};
    unsigned char *body=NULL,*scratch=NULL,bytes[8192];uint64_t generation=0;
    if(argc!=3 || strcmp(argv[1],"--enable") || argv[2][0]!='/') {
        fprintf(stderr,"usage: %s --enable /absolute/libnvss_video.so\n",argv[0]);return 2;
    }
#ifndef PCM4_HOST_TEST
    if(pcm4_identity_file(argv[2],PCM4_CFG_NVSS_HASH))return 2;
#endif
    signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGPIPE,SIG_IGN);
    if(pcm4_ready_init(&marker,PCM4_CFG_MARKER))return 2;
    server=socket(AF_INET,SOCK_STREAM,0);if(server<0)goto done;
    /* Exclusive loopback bind is also the process singleton. No SO_REUSEPORT. */
    setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    memset(&address,0,sizeof(address));address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(0x7f000001u);address.sin_port=htons(PCM4_CFG_PORT);
    if(bind(server,(struct sockaddr *)&address,sizeof(address)) || listen(server,1))goto done;
    pcm4_ready_publish(&marker,0,0);
    body=malloc(512000);scratch=malloc(512000);if(!body || !scratch)goto done;
    /* Vendor dependencies and constructors stay in this dedicated process. */
    if(pcm4_nv_bind(&binding,argv[2]))goto done;
    while(!quit) {
        fd_set reads;struct timeval delay={0,200000};int target=fd<0?server:fd,n;
        FD_ZERO(&reads);FD_SET(target,&reads);n=select(target+1,&reads,NULL,NULL,&delay);
        if(n<0){if(errno==EINTR)continue;goto done;}
        if(fd>=0 && (pcm4_projection_tick(&p,milliseconds()) || !p.stream.emit))goto session_end;
        if(!n)continue;
        if(fd<0) {
            fd=accept(server,NULL,NULL);if(fd<0)continue;
            if(generation==INT64_MAX)goto done;generation++;
            if(pcm4_display_open(&display,&binding.api,&config)||
               pcm4_projection_init_external(&p,&display,pcm4_ready_publish,&marker,PCM4_CFG_TIMEOUT)||
               pcm4_projection_begin(&p,generation,milliseconds())||
               pcm4_screen_receiver_init(&receiver,&p,generation,body,512000,scratch,512000,plaintext,NULL))goto session_end;
            continue;
        }
        {ssize_t count=recv(fd,bytes,sizeof(bytes),0);
         if(count<0 && errno==EINTR)continue;
         if(count<=0 || pcm4_screen_receiver_feed(&receiver,generation,bytes,(size_t)count,milliseconds()))goto session_end;}
        continue;
session_end:
        if(p.stream.generation)pcm4_projection_end(&p,p.stream.generation,milliseconds());
        pcm4_ready_publish(&marker,0,0);
        if(display.handle){pcm4_display_set_visible(&display,0);pcm4_display_close(&display);}
        memset(&p,0,sizeof(p));if(fd>=0)close(fd);fd=-1;
    }
    rc=0;
done:
    pcm4_ready_publish(&marker,0,0);
    if(display.handle){pcm4_display_set_visible(&display,0);pcm4_display_close(&display);}
    pcm4_nv_unbind(&binding);if(fd>=0)close(fd);if(server>=0)close(server);
    free(body);free(scratch);return rc;
}
