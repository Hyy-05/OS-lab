#include "kernel/types.h"
#include "user.h"

int main(int argc,char* argv[]){

    int p2c[2];
    int c2p[2];
    typedef struct msg {
        int pid;
        char text[16];
    }msg;

    if (pipe(p2c) < 0 || pipe(c2p) < 0) {
        printf("pipe error\n");
        exit(1);
    }

    int pid = fork();

    if (pid == 0) {

        close(p2c[1]); // 关闭写端
        close(c2p[0]); // 关闭读端

        msg m1;

        int n = read(p2c[0], &m1, sizeof(msg));
        if (n > 0) {
            printf("%d: received %s from pid %d\n",getpid(),m1.text,m1.pid);
        }

        m1.pid=getpid();
        strcpy(m1.text, "pong");
        write(c2p[1], &m1, sizeof(msg));

        close(p2c[0]);
        close(c2p[1]);
        exit(0);

    } else {

        close(p2c[0]); //关闭读端
        close(c2p[1]); //关闭写端

        msg m2;
        m2.pid=getpid();
        strcpy(m2.text, "ping");

        write(p2c[1], &m2, sizeof(msg));


        int n = read(c2p[0], &m2, sizeof(msg));
        if (n > 0) {
            printf("%d: received %s from pid %d\n",getpid(),m2.text,m2.pid);
        }

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}