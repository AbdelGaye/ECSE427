#define _SVID_SOURCE
#define _BSD_SOURCE
#define _XOPEN_SOURCE 500
#define _XOPEN_SORUCE 600
#define _XOPEN_SORUCE 600

#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define BUFF_SIZE 20
#define BUFF_SHM "/OS_BUFF"
#define BUFF_MUTEX_A "/OS_MUTEX_A"
#define BUFF_MUTEX_B "/OS_MUTEX_B"

//declaring semaphores names for local usage
sem_t *mutexA;
sem_t *mutexB;

//declaring the shared memory and base address
int shm_fd;
void *base;

//structure for indivdual table
struct table
{
    int num;
    char name[10];
};

//initializing the table numbers
void initNumbers(struct table *base)
{
    //capture both mutexes using sem_wait
    sem_wait(mutexA);
    sem_wait(mutexB);

    int i;
    int j;
    
    //initialise the tables with table numbers
    for(i=0; i<=9; i++)
    {
        (base+i)->num = i+100;
    }

    for(j=10; j<=19; j++)
    {
        (base+j)->num = j+190;
    }

    //release the mutexes using sem_post
    sem_post(mutexA);
    sem_post(mutexB);
}

//resetting the tables
void initTables(struct table *base)
{
    //capture both mutexes using sem_wait
    sem_wait(mutexA);
    sem_wait(mutexB);

    int i;
    int j;
    
    //reinitializing the tables to empty
    for(i=0; i<=9; i++)
    {
        strcpy((base+i)->name, "");
    }

    for(j=10; j<=19; j++)
    {
        strcpy((base+j)->name, "");
    }
    
    //perform a random sleep  
    sleep(rand() % 10);

    printf("Table has been reinitialized.\n");
    //release the mutexes using sem_post
    sem_post(mutexA);
    sem_post(mutexB);
    return;
}

void printTableInfo(struct table *base)
{
    //capture both mutexes using sem_wait
    sem_wait(mutexA);
    sem_wait(mutexB);
    
    //print the tables with table numbers and name
    int i;
    for(i=0; i<=19; i++)
    {
        printf("Table no. %d reserved by %s\n", (base+i)->num, (base+i)->name);
    }
    
    //perform a random sleep  
    sleep(rand() % 10);
    
    //release the mutexes using sem_post
    sem_post(mutexA);
    sem_post(mutexB);
    return; 
}

void reserveSpecificTable(struct table *base, char *nameHld, char *section, int tableNo)
{
    switch (section[0])
    {
    case 'A':
        //capture mutex for section A
        sem_wait(mutexA);
        
        //check if table number belongs to section specified
        if(tableNo >= 100 && tableNo <= 109)
        {
            //If nobody has reserved the table
            if(strcmp(((base+(tableNo-100))->name), "") == 0)
            {
                strcpy((base+(tableNo-100))->name, nameHld);
                printf("%s\n", (base+(tableNo-100))->name);
            }
            else
            {
                printf("Cannot reserve table.\n");
                printf("%s has reserved %d\n", (base+(tableNo-100))->name, (base+(tableNo-100))->num);
            }
        }
        //if not: print Invalid table number
        else
        {
            printf("Invalid table number.");
        } 
        
       // release mutex
        sem_post(mutexA);
        break;
    case 'B':
        //capture mutex for section B
        sem_wait(mutexB);
        
       //check if table number belongs to section specified
        if(tableNo >= 200 && tableNo <= 209)
        {
            //If nobody has reserved the table
            if(strcmp(((base+tableNo-190)->name), "") == 0)
            {
                strcpy((base+tableNo-190)->name, nameHld);
                printf("%s\n", (base+(tableNo-190))->name);
            }
            else
            {
                printf("Cannot reserve table.\n");
                printf("%s has reserved %d\n", (base+(tableNo-190))->name, (base+(tableNo-190))->num);
            }
        }
        //if not: print Invalid table number
        else
        {
            printf("Invalid table number.");
        } 
        
       // release mutex
        sem_post(mutexB);
        break;
    default:
        printf("Only sections A or B.\n");
    }
    return;
}

void reserveSomeTable(struct table *base, char *nameHld, char *section)
{
    int idx;
    int i;
    switch (section[0])
    {
    case 'A':
        //capture mutex for section A
        sem_wait(mutexA);

        //look for empty table and reserve it ie copy name to that struct
        idx = -1;
        for(i=0; i<=9; i++)
        {
            if(strcmp(((base+i)->name), "") == 0)
            {
                strcpy((base+i)->name, nameHld);
                printf("%s\n", (base+i)->name);
                idx = 0;
                break;
            }
        }

        //if no empty table print : Cannot find empty table
        if(idx==-1)
            printf("Cannot find empty table in section A.");

        //release mutex for section A
        sem_post(mutexA);
        break;
    case 'B':
        //capture mutex for section B
        sem_wait(mutexB);
    
        //look for empty table and reserve it ie copy name to that struct
        idx = -1;
        for(i=10; i<=19; i++)
        {
            if(strcmp(((base+i)->name), "") == 0)
            {
                strcpy((base+i)->name, nameHld);
                printf("%s\n", (base+i)->name);
                idx = 0;
                break;
            }
        }

        //if no empty table print : Cannot find empty table
        if(idx==-1)
            printf("Cannot find empty table in section B.");

        //release mutex for section A
        sem_post(mutexB);
        break;
    default:
        printf("Only sections A or B.\n");
    }
}

int processCmd(char *cmd, struct table *base)
{
    char *token;
    char *nameHld;
    char *section;
    char *tableChar;
    int tableNo;
    token = strtok(cmd, " ");
    switch (token[0])
    {
    case 'r':
        nameHld = strtok(NULL, " ");
        section = strtok(NULL, " ");
        tableChar = strtok(NULL, " ");
        if(section == NULL)
        {
            printf("Section part empty.");
        }
        else if (tableChar != NULL)
        {
            tableNo = atoi(tableChar);
            reserveSpecificTable(base, nameHld, section, tableNo);
        }
        else
        {
            reserveSomeTable(base, nameHld, section);
        }
        sleep(rand() % 10);
        break;
    case 's':
        printTableInfo(base);
        break;
    case 'i':
        initTables(base);
        break;
    case 'e':
        return 0;
    default:
        printf("Please put a valid command.\n");
    }
    return 1;
}

int main(int argc, char * argv[])
{
    int fdstdin; 
    if(argc>1)
    {
        //store actual stdin before rewiring using dup in fdstdin
        dup2(0, fdstdin);
        close(0);
        //perform stdin rewiring as done in assign 1
        if(open(argv[1], O_RDONLY) < 0)
        {
            printf("The file doesn't exist.\n");
            exit(1);
        }
        else
            open(argv[1], O_RDONLY);
    }

    //open mutex BUFF_MUTEX_A and BUFF_MUTEX_B with inital value 1 using sem_open
    mutexA = sem_open(BUFF_MUTEX_A, O_CREAT, 0777, 1);
    mutexB = sem_open(BUFF_MUTEX_B, O_CREAT, 0777, 1);
    if(mutexA == SEM_FAILED || mutexB == SEM_FAILED)
    {
        printf("Semaphore failed: %s\n", strerror(errno));
        exit(1);
    }
    
    //opening the shared memory buffer ie BUFF_SHM using shm open
    shm_fd = shm_open(BUFF_SHM, O_CREAT | O_RDWR, S_IRWXU);
    if (shm_fd == -1)
    {
        printf("prod: Shared memory failed: %s\n", strerror(errno));
        exit(1);
    }

    //configuring the size of the shared memory to sizeof(struct table) * BUFF_SIZE usinf ftruncate
    ftruncate(shm_fd, sizeof(struct table) * BUFF_SIZE);

    //map this shared memory to kernel space
    base = mmap(NULL, sizeof(struct table) * BUFF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (base == MAP_FAILED)
    {
        printf("prod: Map failed: %s\n", strerror(errno));
        // close and shm_unlink?
        exit(1);
    }

    //initializing the table numbers
    initNumbers(base);

    //intialising random number generator
    time_t now;
    srand((unsigned int)(time(&now)));

    //array in which the user command is held
    char cmd[100];
    int cmdType;
    int ret = 1;
    while (ret)
    {
        printf("\n>>");
        fgets(cmd, 100, stdin);
        if(argc>1)
        {
            printf("Executing Command : %s\n",cmd);

        }
        ret = processCmd(cmd, base);
    }
    
    //close the semphores
    sem_close(mutexA);
    sem_close(mutexB);

    //reset the standard input
    if(argc>1)
    {
        //using dup2
        dup2(fdstdin, 0);
    }

    //unmap the shared memory
    munmap(base, sizeof(struct table) * BUFF_SIZE);
    close(shm_fd);
    return 0;
}