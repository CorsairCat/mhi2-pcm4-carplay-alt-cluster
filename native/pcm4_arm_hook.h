#ifndef PCM4_ARM_HOOK_H
#define PCM4_ARM_HOOK_H
#include <stdint.h>
struct pcm4_arm_hook { void *entry,*replacement,*trampoline; uint32_t original[2]; };
/* Startup-only installation. All expected entries must be preflighted first.
 * No running-process injection or thread suspension is implemented. */
int pcm4_arm_prepare(struct pcm4_arm_hook *,void *,void *,uint32_t,uint32_t);
int pcm4_arm_install(struct pcm4_arm_hook *);
int pcm4_arm_restore(struct pcm4_arm_hook *);
#endif
