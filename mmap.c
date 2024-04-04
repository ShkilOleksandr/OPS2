#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

void child_work(int Rounds,int index, int* places,sem_t* round_semaphore,pthread_barrier_t* round_barrier)
{
    //printf("[%d]\n",getpid());
    sem_t* round_sem = (sem_t*)round_semaphore;
    pthread_barrier_t* round_bar = (pthread_barrier_t*)round_barrier;
    srand(getpid());
    struct timespec sleeping;
    sleeping.tv_nsec = 500000;
    sleeping.tv_sec = rand()%2;
    int sem;
    for(int i=0;i<Rounds;i++)
    {
        
        //sem_getvalue(round_sem,&sem);
        //printf("Semaphore value is [%d]\n",sem);
        sem_wait(round_sem);
        printf("Child [%d] enters the round [%d].\n", getpid(), i+1);
        //nanosleep(&sleeping,NULL);
        places[index] = rand()%21 - 10;
        
        printf("Child [%d] finished the round [%d].\n", getpid(), i+1);
    }
    pthread_barrier_wait(round_bar);
    for(int k=0;k<Rounds;k++)
    {
        sem_post(round_semaphore);
    }
}

void parent_work(sem_t* round_semaphore, int N, int Rounds,pthread_barrier_t* round_barrier)
{
    sem_t* round_sem = (sem_t*)round_semaphore;

    pthread_barrier_t* round_bar = round_barrier;
   
    for(int i=0;i<Rounds;i++)
    {
        
        for(int j=0;j<N;j++)
        {
            printf("posting semaphore\n");
            sem_post(round_sem);
        }
        printf("Round [%d] started.\n", i+1);
        
        printf("Round [%d] ended.\n", i+1);
    }
    for(int k=0;k<N;k++)
    {
        sem_wait(round_semaphore);
    }
    printf("after the barrier\n");
}


void create_children(int Rounds,int n, int* places, sem_t* round_semaphore, pthread_barrier_t* round_barrier)
{
    int res;

    for (int i=0;i< n;i++)
    {
        res = fork();
        switch(res)
        {
            case 0:
                child_work(Rounds,i,places,round_semaphore, round_barrier);
                exit(EXIT_SUCCESS);
            case -1:
                ERR("fork");
            default:
                break;
        }
    }
}


int main (int argc, char** argv)
{
    if(argc != 3)
        ERR(
            "No proper argumentation!"
        );

    int N = atoi(argv[1]);
    
    //crating shared memory
    char name[100];
    snprintf(name,100,"/Map_[%d]",getpid());
    int* children_cards;
    pthread_barrier_t* round_barrier;
    int barrier_descriptor = shm_open("Barrier", O_CREAT | O_RDWR | O_TRUNC, 0666 );
    ftruncate(barrier_descriptor,sizeof(pthread_barrier_t));
    round_barrier = (pthread_barrier_t*)mmap(NULL, sizeof(pthread_barrier_t), PROT_READ | PROT_WRITE, MAP_SHARED,barrier_descriptor,0);
    pthread_barrier_init(round_barrier,NULL,N+1);
    int descriptor = shm_open(name, O_CREAT | O_RDWR | O_TRUNC, 0666);
    ftruncate(descriptor, N*sizeof(N));
    children_cards = (int*)mmap(NULL, N*sizeof(int),PROT_READ | PROT_WRITE, MAP_SHARED,descriptor,0);
    sem_t round_semaphore;
    sem_init(&round_semaphore,1,0);
    create_children(N,N, children_cards, &round_semaphore, round_barrier);
    parent_work(&round_semaphore,N,N,round_barrier);
    
   

    for(int i=0;i<N;i++)
        printf("number : %d\n",children_cards[i]);


    munmap(children_cards,N*sizeof(int));
    munmap(round_barrier,sizeof(pthread_barrier_t));

    exit(EXIT_SUCCESS);
}