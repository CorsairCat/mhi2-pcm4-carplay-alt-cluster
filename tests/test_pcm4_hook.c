#define PCM4_HOST_TEST 1
#define PCM4_HOOK_TEST 1
#include "../native/pcm4_airplay_hook.c"
#include <assert.h>
/* Retaining synthetic CF implementation: exercises actual hook dictionary
 * ownership and stock delegation without loading a firmware library. */
struct value {unsigned refs,kind,count;char *text;int64_t number;CF keys[128],values[128];};
static unsigned live,stock_calls,commands;
static CF retained(CF v){if(v)((struct value *)v)->refs++;return v;}
static void released(CF v)
{struct value *p=(void *)v;unsigned i;if(!p)return;assert(p->refs);if(--p->refs)return;for(i=0;i<p->count;i++){if(p->kind==2)released(p->keys[i]);released(p->values[i]);}free(p->text);free(p);live--;}
static struct value *make(unsigned kind)
{struct value *p=calloc(1,sizeof(*p));assert(p);p->refs=1;p->kind=kind;live++;return p;}
static CF text_new(void *a,const char *text,uint32_t enc)
{struct value *p=make(1);(void)a;(void)enc;p->text=strdup(text);assert(p->text);return p;}
static unsigned long kind(CF p){return ((struct value *)p)->kind;}
static unsigned long dictionary_kind(void){return 2;}
static unsigned long array_kind(void){return 3;}
static Index count(CF p){return ((struct value *)p)->count;}
static int equal(CF a,CF b)
{return a==b || (kind(a)==1 && kind(b)==1 && !strcmp(((struct value *)a)->text,((struct value *)b)->text));}
static CF lookup(CF d,CF key)
{const struct value *p=d;unsigned i;for(i=0;i<p->count;i++)if(equal(p->keys[i],key))return p->values[i];return NULL;}
static void set_value(Mutable d,CF key,CF v)
{struct value *p=d;unsigned i;for(i=0;i<p->count;i++)if(equal(p->keys[i],key)){retained(v);released(p->values[i]);p->values[i]=v;return;}assert(p->count<128);p->keys[p->count]=retained(key);p->values[p->count++]=retained(v);}
static void remove_value(Mutable d,CF key)
{struct value *p=d;unsigned i;for(i=0;i<p->count;i++)if(equal(p->keys[i],key)){released(p->keys[i]);released(p->values[i]);p->count--;p->keys[i]=p->keys[p->count];p->values[i]=p->values[p->count];return;}}
static void all_keys(CF d,const void **keys,const void **values)
{const struct value *p=d;memcpy(keys,p->keys,p->count*sizeof(CF));memcpy(values,p->values,p->count*sizeof(CF));}
static CF element(CF a,Index n){const struct value *p=a;assert(n>=0 && n<(Index)p->count);return p->values[n];}
static void append_value(Mutable a,CF v){struct value *p=a;assert(p->count<128);p->values[p->count++]=retained(v);}
static int64_t get_integer(CF d,CF key,Status *error)
{const struct value *v=lookup(d,key);*error=(!v || v->kind!=4)?-1:0;return *error?0:v->number;}
static void set_integer(Mutable d,CF key,int64_t n)
{struct value *p=make(4);p->number=n;set_value(d,key,p);released(p);}
static Mutable new_dictionary(void *a,Index n,const void *k,const void *v)
{(void)a;(void)n;(void)k;(void)v;return make(2);}
static Mutable new_array(void *a,Index n,const void *v)
{(void)a;(void)n;(void)v;return make(3);}
static CF stock_response;
static Status security_stock(void *s,const unsigned char *key,const unsigned char *iv)
{(void)s;(void)key;(void)iv;return 0;}
static Status start_stock(void *s,void *info){(void)s;(void)info;return 0;}
static Status command_stock(void *s,CF request,void (*callback)(Status,CF,void *),void *opaque)
{(void)s;(void)callback;(void)opaque;assert(get(request,"type") && get(request,"params"));commands++;return 0;}
static void teardown_stock(void *s,CF request,Status reason,Boolean *done)
{(void)s;(void)request;(void)reason;if(done)*done=1;}
static void derive_stub(const void *master_key,size_t n,uint64_t id,unsigned char *key,unsigned char *iv)
{assert(n==16 && id==123);memcpy(key,master_key,16);memset(iv,0,16);}
static int cipher_init(void *ctx,const unsigned char *key,const unsigned char *iv)
{(void)ctx;(void)key;(void)iv;return 0;}
static int cipher_update(void *ctx,const void *input,size_t n,void *out)
{(void)ctx;memmove(out,input,n);return 0;}
static void cipher_final(void *ctx){(void)ctx;}

static CF info_stock(void *session,CF p,unsigned char *mac,Status *e)
{(void)session;(void)p;(void)mac;*e=0;stock_calls++;return retained(stock_response);}
static Status setup_stock(void *session,CF req,CF *out)
{(void)session;(void)req;stock_calls++;*out=retained(stock_response);return 0;}
int main(void)
{
    Mutable display,displays,result,feature_values,descriptor,request,requested;
    CF response,selected;Status error=0;int okay;
    cf.string=text_new;cf.type=kind;cf.dict_type=dictionary_kind;cf.array_type=array_kind;
    cf.dict_count=count;cf.array_count=count;cf.keys=all_keys;cf.get=lookup;cf.at=element;
    cf.retain=retained;cf.release=released;cf.set=set_value;cf.remove=remove_value;
    cf.append=append_value;cf.integer=get_integer;cf.set_integer=set_integer;
    cf.dictionary=new_dictionary;cf.array=new_array;stock_info=info_stock;stock_setup=setup_stock;
    result=dict();display=dict();displays=array();feature_values=array();
    put_text(display,"uuid","stock");put_int(display,"widthPixels",800);put_int(display,"heightPixels",480);
    put_int(display,"widthPhysical",160);put_int(display,"primaryInputDevice",1);
    cf.append(displays,display);put(result,"displays",displays);
    {CF feature=str("stockFeature");cf.append(feature_values,feature);cf.release(feature);}
    put(result,"enabledFeatures",feature_values);stock_response=result;
    enabled=0;response=info_hook(NULL,NULL,NULL,&error);assert(response==result);cf.release(response);
    enabled=1;response=info_hook(NULL,NULL,NULL,&error);
    assert(response!=result && !error && count(get(response,"displays"))==2);
    selected=element(get(response,"displays"),1);
    assert(integer(selected,"type",&okay)==111 && okay);
    assert(integer(selected,"widthPixels",NULL)==448 && !get(selected,"primaryInputDevice"));
    assert(!get(selected,"widthPhysical"));
    assert(count(get(response,"enabledFeatures"))==3);
    assert(integer(display,"widthPixels",NULL)==800 && get(display,"primaryInputDevice"));
    cf.release(response);
    /* Repeated /info must not accumulate auxiliary displays or alter stock. */
    response=info_hook(NULL,NULL,NULL,&error);assert(count(get(response,"displays"))==2);cf.release(response);
    request=dict();requested=array();descriptor=dict();put_int(descriptor,"type",111);
    put_int(descriptor,"streamConnectionID",123);cf.append(requested,descriptor);put(request,"streams",requested);
    assert(find_stream(request,111,&selected)==1 && selected==descriptor);
    assert(!setup_hook((void *)1,request,&response)); /* No captured matching key: untouched stock. */
    assert(response==result && !worker_started);cf.release(response);
    cf.append(requested,descriptor);assert(find_stream(request,111,NULL)==2);
    assert(!setup_hook((void *)1,request,&response) && response==result);cf.release(response);
    /* Successful key/session association and real listener allocation. */
    cf.release(request);cf.release(requested);
    request=dict();requested=array();cf.append(requested,descriptor);put(request,"streams",requested);
    {Mutable session=dict();unsigned char key[16]={1},iv[16]={0};Boolean done=0;
     stock_security=security_stock;stock_teardown=teardown_stock;stock_start=start_stock;send_command=command_stock;
     crypto_api.derive=derive_stub;crypto_api.init=cipher_init;crypto_api.update=cipher_update;crypto_api.final=cipher_final;
     assert(!security_hook(session,key,iv));assert(key_valid && key_session==session);
     assert(!setup_hook((void *)2,request,&response) && response==result);cf.release(response);
     assert(!start_hook(session,NULL) && session_started && !commands);
     assert(!setup_hook(session,request,&response));assert(commands==1);
     assert(response!=result && worker_started && streams(response));
     selected=cf.at(streams(response),0);assert(integer(selected,"dataPort",NULL)>0);
     assert(integer(selected,"streamID",NULL)==111);cf.release(response);
     teardown_hook(session,request,0,&done);assert(!done && key_valid && !worker_started && key_session==session);
     assert(!setup_hook(session,request,&response));cf.release(response);assert(worker_started && commands==2);
     teardown_hook(session,NULL,0,&done);assert(done && !key_valid && !worker_started && !key_session);
     cf.release(session);}
    cf.release(request);cf.release(requested);cf.release(descriptor);
    cf.release(feature_values);cf.release(displays);cf.release(display);cf.release(result);
    assert(!live && stock_calls==8);
    puts("PASS: auxiliary descriptor, stock features/ownership preserved, repeated info, missing-key and duplicate-stream delegation");
    return 0;
}
