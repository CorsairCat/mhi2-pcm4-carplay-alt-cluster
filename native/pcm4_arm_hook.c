#define _GNU_SOURCE
#include "pcm4_arm_hook.h"
#include <string.h>
#include <stdint.h>
#if defined(__arm__) && defined(__QNXNTO__)
#include <sys/mman.h>
#include <unistd.h>
static int write_entry(struct pcm4_arm_hook *h,int restore)
{
    long size=sysconf(_SC_PAGESIZE);uintptr_t page;uint32_t words[2];
    if(size<=0)return -1;
    page=(uintptr_t)h->entry&~((uintptr_t)size-1);
    if(((uintptr_t)h->entry-page)+8>(uintptr_t)size)return -1;
    if(mprotect((void *)page,(size_t)size,PROT_READ|PROT_WRITE))return -1;
    words[0]=restore?h->original[0]:0xe51ff004u;
    words[1]=restore?h->original[1]:(uint32_t)(uintptr_t)h->replacement;
    memcpy(h->entry,words,8);__builtin___clear_cache(h->entry,(char *)h->entry+8);
    /* Restore executable permissions before returning, including failure paths. */
    if(mprotect((void *)page,(size_t)size,PROT_READ|PROT_EXEC)) _exit(126);
    return 0;
}
#endif
int pcm4_arm_prepare(struct pcm4_arm_hook *h,void *entry,void *replacement,uint32_t a,uint32_t b)
{
#if defined(__arm__) && defined(__QNXNTO__)
    uint32_t words[2],*stub;
    if(!h || !entry || !replacement || ((uintptr_t)entry&3))return -1;
    memcpy(words,entry,8);if(words[0]!=a || words[1]!=b)return -1;
    stub=mmap(NULL,16,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    if(stub==MAP_FAILED)return -1;
    stub[0]=a;stub[1]=b;stub[2]=0xe51ff004u;stub[3]=(uint32_t)(uintptr_t)entry+8;
    __builtin___clear_cache((char *)stub,(char *)stub+16);
    if(mprotect(stub,16,PROT_READ|PROT_EXEC)){munmap(stub,16);return -1;}
    memset(h,0,sizeof(*h));h->entry=entry;h->replacement=replacement;h->trampoline=stub;
    h->original[0]=a;h->original[1]=b;return 0;
#else
    (void)h;(void)entry;(void)replacement;(void)a;(void)b;return -1;
#endif
}
int pcm4_arm_install(struct pcm4_arm_hook *h)
{
#if defined(__arm__) && defined(__QNXNTO__)
    return write_entry(h,0);
#else
    (void)h;return -1;
#endif
}
int pcm4_arm_restore(struct pcm4_arm_hook *h)
{
#if defined(__arm__) && defined(__QNXNTO__)
    return write_entry(h,1);
#else
    (void)h;return -1;
#endif
}
