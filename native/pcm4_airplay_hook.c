/* Original Porsche MU1476 candidate. See IPHONE_SESSION_HOOKS_MU1476.md.
 * Disabled unless explicitly enabled at process startup. No vendor bytes
 * distributed. Stock calls delegated; no session key or video logging. */
#define _GNU_SOURCE
#include "pcm4_crypto.h"
#include "pcm4_profile.h"
#include "pcm4_ready.h"
#include "pcm4_screen_receiver.h"
#include "pcm4_nv_binding.h"
#include "pcm4_identity.h"
#include "pcm4_arm_hook.h"
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

typedef const void *CF;
typedef void *Mutable;
typedef long Index;
typedef int Status;
typedef unsigned char Boolean;
static struct {
    CF (*string)(void *,const char *,uint32_t);
    unsigned long (*type)(CF),(*dict_type)(void),(*array_type)(void);
    Index (*dict_count)(CF),(*array_count)(CF);
    void (*keys)(CF,const void **,const void **);
    CF (*get)(CF,CF),(*at)(CF,Index),(*retain)(CF);
    void (*set)(Mutable,CF,CF),(*remove)(Mutable,CF),(*append)(Mutable,CF),(*release)(CF);
    int64_t (*integer)(CF,CF,Status *);
    void (*set_integer)(Mutable,CF,int64_t);
    Mutable (*dictionary)(void *,Index,const void *,const void *);
    Mutable (*array)(void *,Index,const void *);
    const void *key_callbacks,*value_callbacks,*array_callbacks;
} cf;
static struct pcm4_crypto_api crypto_api;
static Status (*stock_security)(void *,const unsigned char *,const unsigned char *);
static Status (*stock_setup)(void *,CF,CF *);
static Status (*stock_start)(void *,void *);
static void (*stock_teardown)(void *,CF,Status,Boolean *);
static CF (*stock_info)(void *,CF,unsigned char *,Status *);
static Status (*send_command)(void *,CF,void (*)(Status,CF,void *),void *);
static pthread_mutex_t life=PTHREAD_MUTEX_INITIALIZER, lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_t worker;
static int worker_started, stopping, listener=-1, client=-1, enabled, key_valid, session_started;
static void *key_session;
static unsigned char master[16];
static uint64_t generation;
static const char *uuid="5a924e64-639d-4ac3-bf30-50434d340060";
static const char *nav_url="maps:/car/instrumentcluster";
static unsigned width=PCM4_CFG_WIDTH,height=PCM4_CFG_HEIGHT,fps=PCM4_CFG_FPS;
static unsigned safe_x=PCM4_CFG_SAFE_X,safe_y=PCM4_CFG_SAFE_Y,safe_w=PCM4_CFG_SAFE_W,safe_h=PCM4_CFG_SAFE_H;
static unsigned physical_w,physical_h;
static struct pcm4_crypto cipher;
static void wipe(void *p,size_t n){volatile unsigned char *b=p;while(n--)*b++=0;}
static uint64_t now_ms(void)
{struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+(unsigned)t.tv_nsec/1000000;}
static int is_type(CF value,unsigned long (*kind)(void))
{return value && cf.type(value)==kind();}
static CF str(const char *s){return cf.string(NULL,s,0x08000100u);}
static CF get(CF d,const char *name)
{CF k=str(name),v;if(!k)return NULL;v=cf.get(d,k);cf.release(k);return v;}
static int put(Mutable d,const char *name,CF value)
{CF k=str(name);if(!k || !value){if(k)cf.release(k);return -1;}cf.set(d,k,value);cf.release(k);return 0;}
static int put_text(Mutable d,const char *name,const char *value)
{CF v=str(value);int rc=put(d,name,v);if(v)cf.release(v);return rc;}
static int put_int(Mutable d,const char *name,int64_t value)
{CF k=str(name);if(!k)return -1;cf.set_integer(d,k,value);cf.release(k);return 0;}
static int64_t integer(CF d,const char *name,int *okay)
{CF k=str(name);Status error=-1;int64_t v=0;if(k){v=cf.integer(d,k,&error);cf.release(k);}if(okay)*okay=!error;return v;}
static Mutable dict(void){return cf.dictionary(NULL,0,cf.key_callbacks,cf.value_callbacks);}
static Mutable array(void){return cf.array(NULL,0,cf.array_callbacks);}
static Mutable clone(CF source)
{
    Mutable result; const void **keys,**values;Index i,n;
    if(!is_type(source,cf.dict_type))return NULL;
    result=dict();if(!result)return NULL;n=cf.dict_count(source);
    if(n<0 || n>4096){cf.release(result);return NULL;}
    keys=calloc((size_t)n+1,sizeof(*keys));values=calloc((size_t)n+1,sizeof(*values));
    if(!keys || !values){free(keys);free(values);cf.release(result);return NULL;}
    cf.keys(source,keys,values);for(i=0;i<n;i++)cf.set(result,keys[i],values[i]);
    free(keys);free(values);return result;
}
static Mutable copy_array(CF source)
{
    Mutable result=array();Index i,n=source?cf.array_count(source):0;
    if(!result)return NULL;
    if(n<0 || n>128){cf.release(result);return NULL;}
    for(i=0;i<n;i++)cf.append(result,cf.at(source,i));return result;
}
static CF streams(CF request)
{CF v;if(!is_type(request,cf.dict_type))return NULL;v=get(request,"streams");return is_type(v,cf.array_type)?v:NULL;}
/* -1 malformed descriptor list, otherwise count of requested stream type. */
static int find_stream(CF request,int type,CF *found)
{
    CF a=streams(request);Index i,n=a?cf.array_count(a):0;int count=0,okay;
    if(found)*found=NULL;if(n<0 || n>32)return -1;
    for(i=0;i<n;i++){
        CF d=cf.at(a,i);int64_t value;if(!is_type(d,cf.dict_type))return -1;
        value=integer(d,"type",&okay);if(!okay)return -1;
        if(value==type){if(found)*found=d;count++;}
    }return count;
}
static int features(Mutable d)
{
    CF source=get(d,"enabledFeatures");Mutable a;CF alt;
    if(source && !is_type(source,cf.array_type))return -1;
    a=copy_array(source);alt=str("altScreen");
    if(!a || !alt){if(a)cf.release(a);if(alt)cf.release(alt);return -1;}
    cf.append(a,alt);cf.release(alt);
    alt=str("viewAreas");if(!alt){cf.release(a);return -1;}cf.append(a,alt);cf.release(alt);
    {int rc=put(d,"enabledFeatures",a);cf.release(a);return rc;}
}
static int safe_area(Mutable alt)
{
    Mutable view=dict(),safe=dict(),areas=array();int rc=-1;
    if(!view || !safe || !areas)goto done;
    if(put_int(view,"widthPixels",width)||put_int(view,"heightPixels",height)||
       put_int(view,"originXPixels",0)||put_int(view,"originYPixels",0)||
       put_int(safe,"widthPixels",safe_w)||put_int(safe,"heightPixels",safe_h)||
       put_int(safe,"originXPixels",safe_x)||put_int(safe,"originYPixels",safe_y)||put(view,"safeArea",safe))goto done;
    cf.append(areas,view);if(put(alt,"viewAreas",areas)||put_int(alt,"initialViewArea",0))goto done;
    rc=0;
done:
    if(view)cf.release(view);if(safe)cf.release(safe);if(areas)cf.release(areas);return rc;
}
static CF info_hook(void *session,CF properties,unsigned char *mac,Status *error)
{
    CF base=stock_info(session,properties,mac,error),old;Mutable result=NULL,alt=NULL,displays=NULL;unsigned i;
    static const char *remove[]={"primaryInputDevice","edid","platformLayer","avcc","xOffset","yOffset","windowWidth","windowHeight","widthPhysical","heightPhysical"};
    if(!enabled || !base)return base;
    old=get(base,"displays");if(!is_type(old,cf.array_type) || cf.array_count(old)<1)return base;
    result=clone(base);alt=clone(cf.at(old,0));displays=copy_array(old);
    if(!result || !alt || !displays)goto failed;
    for(i=0;i<sizeof(remove)/sizeof(remove[0]);i++){CF k=str(remove[i]);if(!k)goto failed;cf.remove(alt,k);cf.release(k);}
    if(features(result)||put_int(alt,"type",111)||put_int(alt,"maxFPS",fps)||
       put_int(alt,"features",0)||put_int(alt,"widthPixels",width)||put_int(alt,"heightPixels",height)||
       put_text(alt,"uuid",uuid)||put_text(alt,"initialURL",nav_url)||safe_area(alt))goto failed;
    if(physical_w && (put_int(alt,"widthPhysical",physical_w)||put_int(alt,"heightPhysical",physical_h)))goto failed;
    {Mutable urls=array();CF role=str(nav_url);
     if(!urls || !role){if(urls)cf.release(urls);if(role)cf.release(role);goto failed;}
     cf.append(urls,role);cf.release(role);
     if(put(alt,"altScreenSuggestUIURLs",urls)){cf.release(urls);goto failed;}cf.release(urls);}
    cf.append(displays,alt);if(put(result,"displays",displays))goto failed;
    cf.release(alt);cf.release(displays);cf.release(base);return result;
failed:
    if(result)cf.release(result);if(alt)cf.release(alt);if(displays)cf.release(displays);return base;
}
static int stopped(void)
{int value;pthread_mutex_lock(&lock);value=stopping;pthread_mutex_unlock(&lock);return value;}
static int transfer(int fd,void *data,size_t count,int sending)
{
    unsigned char *p=data;uint64_t start=now_ms();
    while(count && !stopped()) {
        fd_set reads,writes;struct timeval timeout={0,200000};int rc;ssize_t n;
        FD_ZERO(&reads);FD_ZERO(&writes);
        if(sending)FD_SET(fd,&writes);else FD_SET(fd,&reads);
        rc=select(fd+1,&reads,&writes,NULL,&timeout);
        if(rc<0 && errno==EINTR)continue;
        if(rc<0 || now_ms()-start>10000)return -1;
        if(!rc)continue;
        n=sending?send(fd,p,count,0):recv(fd,p,count,0);
        if(n<0 && (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK))continue;
        if(n<=0)return -1;p+=n;count-=(size_t)n;
    }
    return count?-1:0;
}
static void *receive_worker(void *unused)
{
    unsigned char header[128],*body=malloc(512000);
    struct sockaddr_in address;int fd=-1,output=-1,listen_fd;
    uint64_t began=now_ms();sigset_t blocked;(void)unused;
    sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);pthread_sigmask(SIG_BLOCK,&blocked,NULL);
    pthread_mutex_lock(&lock);listen_fd=listener;pthread_mutex_unlock(&lock);
    if(!body)goto done;
    while(!stopped() && now_ms()-began<10000) {
        fd_set reads;struct timeval timeout={0,200000};int rc;
        FD_ZERO(&reads);FD_SET(listen_fd,&reads);rc=select(listen_fd+1,&reads,NULL,NULL,&timeout);
        if(rc<0 && errno==EINTR)continue;if(rc<0)goto done;if(!rc)continue;
        pthread_mutex_lock(&lock);if(!stopping){fd=accept(listen_fd,NULL,NULL);client=fd;}pthread_mutex_unlock(&lock);break;
    }
    if(fd<0)goto done;
    output=socket(AF_INET,SOCK_STREAM,0);if(output<0)goto done;
    memset(&address,0,sizeof(address));address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(PCM4_CFG_PORT);
    if(connect(output,(struct sockaddr *)&address,sizeof(address)))goto done;
    if(fcntl(output,F_SETFL,O_NONBLOCK) || fcntl(fd,F_SETFL,O_NONBLOCK))goto done;
    while(!stopped()) {
        size_t size;
        if(transfer(fd,header,sizeof(header),0))break;
        size=(size_t)header[0]|(size_t)header[1]<<8|(size_t)header[2]<<16|(size_t)header[3]<<24;
        if(size>512000 || header[4]>5 || ((header[4]==0 || header[4]==1) && !size))break;
        if(transfer(fd,body,size,0))break;
        if(header[4]==0 && pcm4_crypto_decrypt(&cipher,body,size))break;
        /* Private loopback protocol: original Screen framing with plaintext
         * video bodies. Never send a key or expose this listener remotely. */
        if(transfer(output,header,128,1)||transfer(output,body,size,1))break;
    }
done:
    if(output>=0)close(output);free(body);
    pthread_mutex_lock(&lock);if(fd>=0)close(fd);client=-1;
    if(listener>=0)close(listener);listener=-1;pthread_mutex_unlock(&lock);
    return NULL;
}
/* life mutex serializes security/setup/teardown, lock protects descriptors. */
static void stop_receiver(void)
{
    pthread_mutex_lock(&lock);stopping=1;
    if(client>=0)shutdown(client,SHUT_RDWR);
    if(listener>=0)shutdown(listener,SHUT_RDWR);
    pthread_mutex_unlock(&lock);
    if(worker_started){pthread_join(worker,NULL);worker_started=0;}
    pthread_mutex_lock(&lock);
    if(listener>=0){close(listener);listener=-1;}
    pthread_mutex_unlock(&lock);
    pcm4_crypto_end(&cipher);
}
static int start_receiver(uint64_t id)
{
    struct sockaddr_in address;socklen_t size=sizeof(address);int port,fd;
    stop_receiver();if(!key_valid || generation==INT64_MAX)return -1;
    if(pcm4_crypto_begin(&cipher,&crypto_api,master,id))return -1;
    fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)goto failed;
    memset(&address,0,sizeof(address));address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(fd,(struct sockaddr *)&address,sizeof(address))||listen(fd,1)||getsockname(fd,(struct sockaddr *)&address,&size)){close(fd);goto failed;}
    port=ntohs(address.sin_port);
    pthread_mutex_lock(&lock);listener=fd;stopping=0;generation++;pthread_mutex_unlock(&lock);
    if(pthread_create(&worker,NULL,receive_worker,NULL)){close(fd);listener=-1;goto failed;}
    worker_started=1;return port;
failed:pcm4_crypto_end(&cipher);return -1;
}
static Status security_hook(void *session,const unsigned char *key,const unsigned char *iv)
{
    Status result=stock_security(session,key,iv);
    if(!enabled)return result;
    pthread_mutex_lock(&life);stop_receiver();
    wipe(master,sizeof(master));key_valid=0;
    if(key_session!=session)session_started=0;
    if(key_session){cf.release(key_session);key_session=NULL;}
    if(!result && key){key_session=(void *)cf.retain(session);memcpy(master,key,16);key_valid=1;}
    pthread_mutex_unlock(&life);return result;
}
static void request_projection(void *session)
{
    Mutable command=dict(),params=dict();
    if(command && params && !put_text(command,"type","showUI") &&
       !put_text(params,"uuid",uuid) && !put_text(params,"url",nav_url) && !put(command,"params",params))
        send_command(session,command,NULL,NULL);
    if(command)cf.release(command);if(params)cf.release(params);
}
static Status setup_hook(void *session,CF request,CF *out)
{
    CF base=NULL,descriptor=NULL,old;Mutable result=NULL,a=NULL,desc=NULL;Status error;
    int count,okay,port=-1,show=0;uint64_t id;
    error=stock_setup(session,request,&base);
    if(error || !enabled)goto done;
    count=find_stream(request,111,&descriptor);if(count!=1)goto done;
    id=(uint64_t)integer(descriptor,"streamConnectionID",&okay);if(!okay || !id)goto done;
    pthread_mutex_lock(&life);
    if(!key_valid || key_session!=session)goto unlock;
    result=clone(base);desc=clone(descriptor);old=result?streams(result):NULL;a=copy_array(old);
    if(!result || !desc || !a)goto unlock;
    port=start_receiver(id);if(port<0)goto unlock;
    if(put_int(desc,"dataPort",port)||put_int(desc,"streamID",111)||features(result)){stop_receiver();port=-1;goto unlock;}
    cf.append(a,desc);if(put(result,"streams",a)){stop_receiver();port=-1;goto unlock;}
    show=session_started;if(show)cf.retain(session);
unlock:
    pthread_mutex_unlock(&life);
    if(port>0){cf.release(base);base=result;result=NULL;}
    if(show){request_projection(session);cf.release(session);}
done:
    if(result)cf.release(result);if(a)cf.release(a);if(desc)cf.release(desc);
    if(out)*out=base;else if(base)cf.release(base);return error;
}
static Status start_hook(void *session,void *info)
{
    Status rc=stock_start(session,info);int active;
    pthread_mutex_lock(&life);
    if(enabled && !rc && key_session==session)session_started=1;
    active=enabled && !rc && key_session==session && worker_started;
    if(active)cf.retain(session);
    pthread_mutex_unlock(&life);
    if(active){request_projection(session);cf.release(session);}return rc;
}
static void teardown_hook(void *session,CF request,Status reason,Boolean *done)
{
    CF requested_streams=streams(request);
    int full=!requested_streams || cf.array_count(requested_streams)==0,aux=find_stream(request,111,NULL),main_stream=find_stream(request,110,NULL);
    if(enabled){pthread_mutex_lock(&life);
        if(session==key_session && (full || aux!=0 || main_stream!=0)){
            stop_receiver();if(full || main_stream!=0){wipe(master,16);key_valid=0;session_started=0;cf.release(key_session);key_session=NULL;}
        }pthread_mutex_unlock(&life);}
    if(enabled && aux>0) {
        CF old=streams(request);Index i,n=cf.array_count(old);
        Mutable filtered=clone(request),kept=array();
        if(n==aux){if(filtered)cf.release(filtered);if(kept)cf.release(kept);if(done)*done=0;return;}
        if(filtered && kept) {
            for(i=0;i<n;i++) {CF d=cf.at(old,i);if(integer(d,"type",NULL)!=111)cf.append(kept,d);}
            if(!put(filtered,"streams",kept)) {stock_teardown(session,filtered,reason,done);cf.release(filtered);cf.release(kept);return;}
        }
        if(filtered)cf.release(filtered);if(kept)cf.release(kept);
    }
    stock_teardown(session,request,reason,done);
}
static int bind_api(void)
{
#define LOAD(destination,name) do{*(void **)(&(destination))=dlsym(RTLD_DEFAULT,name);if(!(destination))return -1;}while(0)
    LOAD(cf.string,"CFStringCreateWithCString");LOAD(cf.type,"CFGetTypeID");
    LOAD(cf.dict_type,"CFDictionaryGetTypeID");LOAD(cf.array_type,"CFArrayGetTypeID");
    LOAD(cf.dict_count,"CFDictionaryGetCount");LOAD(cf.keys,"CFDictionaryGetKeysAndValues");
    LOAD(cf.get,"CFDictionaryGetValue");LOAD(cf.set,"CFDictionarySetValue");LOAD(cf.remove,"CFDictionaryRemoveValue");
    LOAD(cf.array_count,"CFArrayGetCount");LOAD(cf.at,"CFArrayGetValueAtIndex");LOAD(cf.append,"CFArrayAppendValue");
    LOAD(cf.retain,"CFRetain");LOAD(cf.release,"CFRelease");LOAD(cf.integer,"CFDictionaryGetInt64");
    LOAD(cf.set_integer,"CFDictionarySetInt64");LOAD(cf.dictionary,"CFDictionaryCreateMutable");LOAD(cf.array,"CFArrayCreateMutable");
    LOAD(cf.key_callbacks,"kCFLDictionaryKeyCallBacksCFLTypes");LOAD(cf.value_callbacks,"kCFLDictionaryValueCallBacksCFLTypes");LOAD(cf.array_callbacks,"kCFLArrayCallBacksCFLTypes");
    LOAD(crypto_api.derive,"AirPlay_DeriveAESKeySHA512ForScreen");LOAD(crypto_api.init,"AES_CTR_Init");
    LOAD(crypto_api.update,"AES_CTR_Update");LOAD(crypto_api.final,"AES_CTR_Final");LOAD(send_command,"AirPlayReceiverSessionSendCommand");
#undef LOAD
    return 0;
}
#ifndef PCM4_HOOK_TEST
__attribute__((constructor)) static void initialize(void)
{
    static struct pcm4_arm_hook hooks[5];unsigned i,installed=0;void *entries[5],*replacements[5];
    const char *names[]={"AirPlayReceiverSessionSetSecurityInfo","AirPlayCopyServerInfo","AirPlayReceiverSessionSetup","AirPlayReceiverSessionStart","AirPlayReceiverSessionTearDown"};
    const uint32_t words[5][2]={{0xe92d41f0,0xe28050bc},{0xe92d4ff0,0xe24dde4d},{0xe92d4ff0,0xed2d8b02},{0xe92d4ff0,0xed2d8b02},{0xe92d4ff0,0xed2d8b02}};
    const char *opt=getenv("PCM4_ENABLE"),*airplay=getenv("PCM4_AIRPLAY_LIBRARY");
    if((!opt || strcmp(opt,"1")) && access("/mnt/app/root/pcm4-projection/enabled",F_OK)) {
        fprintf(stderr,"PCM4: auxiliary candidate loaded, disabled; no hooks installed.\n");return;
    }
    {const char *w=getenv("PCM4_PHYSICAL_WIDTH_MM"),*h=getenv("PCM4_PHYSICAL_HEIGHT_MM");
     if(w || h){char *end;unsigned long n;
        if(!w || !h || !w[0] || !h[0])return;
        errno=0;n=strtoul(w,&end,10);if(errno || *end || !n || n>1000)return;physical_w=(unsigned)n;
        errno=0;n=strtoul(h,&end,10);if(errno || *end || !n || n>1000)return;physical_h=(unsigned)n;}}

    if(!airplay)airplay="/eso/lib/libairplay.so";
    if(pcm4_identity_file(airplay,PCM4_CFG_AIRPLAY_HASH) || bind_api()) {
        fprintf(stderr,"PCM4: AirPlay identity/API gate failed; projection disabled.\n");return;
    }
    replacements[0]=(void *)security_hook;replacements[1]=(void *)info_hook;replacements[2]=(void *)setup_hook;
    replacements[3]=(void *)start_hook;replacements[4]=(void *)teardown_hook;
    for(i=0;i<5;i++)entries[i]=dlsym(RTLD_DEFAULT,names[i]);
    {static const uintptr_t offsets[]={0x25ab4,0x1ef54,0x23fb8,0x21f78,0x23860};
     uintptr_t image=(uintptr_t)entries[0]-offsets[0];
     for(i=0;i<5;i++)if(!entries[i] || (uintptr_t)entries[i]!=image+offsets[i])return;
     if((uintptr_t)crypto_api.derive!=image+0x2a2fc || (uintptr_t)crypto_api.init!=image+0x2e804 ||
        (uintptr_t)crypto_api.update!=image+0x2e500 || (uintptr_t)crypto_api.final!=image+0x2e4b8)return;}
    for(i=0;i<5;i++){if(pcm4_arm_prepare(&hooks[i],entries[i],replacements[i],words[i][0],words[i][1])) {
        fprintf(stderr,"PCM4: hook preflight failed: %s; projection disabled.\n",names[i]);return;}}
    *(void **)(&stock_security)=hooks[0].trampoline;*(void **)(&stock_info)=hooks[1].trampoline;
    *(void **)(&stock_setup)=hooks[2].trampoline;*(void **)(&stock_start)=hooks[3].trampoline;*(void **)(&stock_teardown)=hooks[4].trampoline;
    for(i=0;i<5;i++){if(pcm4_arm_install(&hooks[i])) {
        fprintf(stderr,"PCM4: text permission/install failed: %s; restoring prior entries.\n",names[i]);break;}installed++;}
    if(installed!=5){while(installed){installed--;if(pcm4_arm_restore(&hooks[installed]))_exit(126);}return;}
    enabled=1;
    fprintf(stderr,"PCM4: MU1476 auxiliary hook initialized (output0/displayable60/context900); vehicle validation pending.\n");
}
#endif
