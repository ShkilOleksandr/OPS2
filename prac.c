#define _GNU_SOURCE
#include <errno.h>
#include <mqueue.h>
#include <stdint.h>
#include <stdio.h>
#include <semaphore.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define LIFE_SPAN 10
#define MAX_NUM 10

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))


typedef struct mqargs
{
    char name[100];
    mqd_t pointer;
}mqargs;



static void tfunc(union sigval sv)
{
    struct mq_attr attr;
    ssize_t nr;
    void *buf;
    mqd_t mqdes = *((mqd_t *) sv.sival_ptr);
    if (mq_getattr(mqdes, &attr) == -1)
        ERR("mq_getattr");
    buf = malloc(attr.mq_msgsize);
    if (buf == NULL)
        ERR("malloc");
    int PID;
    nr = mq_receive(mqdes, buf, attr.mq_msgsize, &PID);
    printf("Result from worker %d: %d.\n", PID, atoi(buf));
    free(buf);
    exit(EXIT_SUCCESS); 
}
void create_child_queue(int PID, int PPID, mqargs* ARGS, int index)
{
    struct mq_attr attr;
    attr.mq_maxmsg = 1;
    attr.mq_msgsize = 10;
    snprintf(ARGS[index].name, 100, "/childqueue_%d_%d", PID, PPID);
    ARGS[index].pointer = mq_open(ARGS[index].name, O_RDWR | O_CREAT | O_EXCL, 0600, &attr);
    mq_close(ARGS[index].pointer);
}

void create_server_queue(int PID, mqargs* arg)
{
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 100;
    snprintf(arg->name, 100, "/server_%d", PID);
    arg->pointer = mq_open(arg->name, O_RDWR | O_CREAT, 0600, &attr);
    mq_close(arg->pointer);
    
}

void child_work(mqargs parent, mqargs result_channel)
{
    char server_message[100];
    char result_message[100];
    int span = 5;
    parent.pointer = mq_open(parent.name, O_RDWR);
    if((int)parent.pointer == -1)
        ERR("mq_open");
    result_channel.pointer = mq_open(result_channel.name, O_WRONLY);
    if((int)result_channel.pointer == -1)
        ERR("mq_open");

    while(span--)
    {
        if(mq_receive(parent.pointer, server_message, 100, NULL) == -1)
            ERR("mq_receive");
        char* token = strtok(server_message, " ");
        int first = atoi(token);
        token = strtok(NULL," ");
        int second = atoi(token);
        snprintf(result_message,100,"%d",first+second);
        printf("[%d] Result sent %d\n",getpid(), first + second);
        mq_send(result_channel.pointer, result_message, 100, getpid());

    }
    close(result_channel.pointer);
    unlink(result_channel.name);
    printf("[%d] child exits\n", getpid());
}

void server_work(int n,mqargs parent)
{
    srand(getpid());
    struct mq_attr attr;
    double k,m;
    char task[100];
    int counter = 5*n;
    parent.pointer = mq_open(parent.name, O_RDWR);

    while(counter > 0)
    {
        mq_getattr(parent.pointer, &attr);
        if(attr.mq_curmsgs == attr.mq_maxmsg)
            {
                printf("Queue is full!\n");
                continue;
            }
        counter--;
        k = rand()%1001 / 10;
        m = rand()%1001 / 10;
        snprintf(task,100,"%f %f",k,m);
        mq_send(parent.pointer,task,100,1);
        printf("[%d]sending task to children!\n",getpid());
    }
}

void create_children(int n, mqargs *server_queue, mqargs* children)
{
    int fork_res;
    struct sigevent not;
    //sem_t creation_waiter;
   // sem_init(&creation_waiter, 1, 0);
    for(int i=0;i<n;i++)
    {
        fork_res = fork();
        switch(fork_res)
        {
            case 0:
                create_child_queue(getpid(),getppid(),children,i);
                //sem_post(&creation_waiter);
                child_work(*server_queue,children[i]);
                exit(EXIT_SUCCESS);
            case -1:
                ERR("fork");
            default:
                create_child_queue(fork_res,getpid(),children,i);
                not.sigev_notify = SIGEV_THREAD;/* Notification by thread*/
                not.sigev_notify_function = tfunc;/* working function */
                not.sigev_notify_attributes = NULL;
                //sem_wait(&creation_waiter);
                children[i].pointer = mq_open(children[i].name, O_RDONLY);
                not.sigev_value.sival_ptr = &children[i].pointer; /* tfunc arguments*/
                if (mq_notify(children[i].pointer, &not) == -1)
                    ERR("mq_notify");

                break;
        }

    }
    //ls
    //sem_destroy(&creation_waiter);
}






void usage(void)
{
    fprintf(stderr, "USAGE: signals n k p l\n");
    fprintf(stderr, "100 >n > 0 - number of children\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int n = atoi(argv[1]);
    srand(getpid());
    int t1 = rand()%4900 + 100;
    int t2 = rand()%(5001 - t1) + t1;
    mqargs Children_queues[n];
    mqargs server_queue;
    create_server_queue(getpid(),&server_queue);
    create_children(n,&server_queue,Children_queues);
    server_work(n,server_queue);
    while(wait(NULL)>0);
    mq_close(server_queue.pointer);
    mq_unlink(server_queue.name);

    return EXIT_SUCCESS;
}