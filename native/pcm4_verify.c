#include "pcm4_identity.h"
#include <stdio.h>
int main(int argc,char **argv)
{
    if(argc!=3){fprintf(stderr,"usage: %s ABSOLUTE_FILE SHA256\n",argv[0]);return 2;}
    if(pcm4_identity_file(argv[1],argv[2])){fprintf(stderr,"Identity mismatch: %s\n",argv[1]);return 1;}
    return 0;
}
