#ifndef WIN32
#include <unistd.h> //execlp()
#endif
#include <stdio.h>
#include <signal.h> //sigaction()


#include "sound.h"


void sound_play(const char* fname)
{
#ifndef WIN32
    //register signal handler
    struct sigaction act;

    //msg("Sound abspielen");
    act.sa_flags = SA_SIGINFO;
    act.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &act, 0);

    if(!fname)
        return;

    pid_t t=fork();
    if(t==-1)
    {
        ;
    }
    else if(t==0)
    {
        //child
        char filename[1024];
        snprintf(filename, 1024, "/usr/share/sounds/%s", fname);

        execlp("/usr/bin/aplay", "aplay", filename, (char*)0);
    }
    else
    {
        //parent
        ;
    }
#endif
}
