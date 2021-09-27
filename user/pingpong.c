#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
pingpong()
{
    char receive[512];
    char send1[512] = {"00000000"};
    char send2[512] = {"10000000"};
    char send3[512] = {"11000000"};
    int p1[2], p2[2], p3[2];
    pipe(p1);
    pipe(p2);
    pipe(p3);
    printf("%d \n", getpid());
    write(p1[1], send1, sizeof(send1));
    int pid = fork();
    if(pid == 0){
        int n;
        if((n = read(p1[0], receive, sizeof(receive))) < 0){
            printf("child: read error\n");
        }else{
            printf("<%d>:received ping-%s \n", getpid(), receive);
            write(p2[1], send2, sizeof(send2));

            fork(); //child's son
            if((n = read(p2[0], receive, sizeof(receive))) < 0){
                printf("child's child: read error\n");
            }else{ //getpid() still is 4
                printf("<%d>:received pingpong-%s \n", getpid(), receive);
                write(p3[1], send3, sizeof(send3));
            }

        }
    }else{
        int m;
        wait(0);
        if((m = read(p3[0], receive, sizeof(receive))) < 0){
            printf("parent: read error\n");
        }else{
            printf("<%d>:received pong-%s \n", getpid(), receive);  
        }
    }
}

int
main(int argc, char *argv[])
{
  pingpong();
  exit(0);
}
