#define _POSIX_C_SOURCE 200809L
#include "pcm4_ready.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int pcm4_ready_init(struct pcm4_ready *r, const char *path)
{
    if (!r || !path || path[0]!='/' || strlen(path)>sizeof(r->path)-64) return -1;
    memset(r,0,sizeof(*r)); strcpy(r->path,path); return 0;
}
int pcm4_ready_publish(void *opaque, uint64_t generation, uint64_t frames)
{
    struct pcm4_ready *r=opaque;
    char temp[512], text[96]; size_t at=0; int fd,n,rc=0;
    if (!r || !r->path[0]) return -1;
    if (!frames) return unlink(r->path)==0 || errno==ENOENT ? 0 : -1;
    if (!generation || generation>INT64_MAX || frames>INT64_MAX) return -1;
    n=snprintf(text,sizeof(text),"PCM4READY1\n%llu\n%llu\n",
               (unsigned long long)generation,(unsigned long long)frames);
    if (n<=0 || (size_t)n>=sizeof(text)) return -1;
    snprintf(temp,sizeof(temp),"%.448s.%ld.%u",r->path,(long)getpid(),++r->sequence);
    fd=open(temp,O_WRONLY|O_CREAT|O_EXCL,0600);
    if (fd<0) return -1;
    while (at<(size_t)n) {
        ssize_t wrote=write(fd,text+at,(size_t)n-at);
        if (wrote<0 && errno==EINTR) continue;
        if (wrote<=0) {rc=-1;break;} at+=(size_t)wrote;
    }
    if (close(fd)) rc=-1;
    if (!rc && rename(temp,r->path)) rc=-1;
    if (rc) {unlink(temp); unlink(r->path);} /* Never leave readiness after failure. */
    return rc;
}
