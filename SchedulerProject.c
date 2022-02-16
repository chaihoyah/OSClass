#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <signal.h>
#include <fcntl.h>

#define TIMEQUANTUM 5 //Real Timequantum: 1 sec

struct msgbuf
{
    long msgtype; //parent 1, child 2~11
    char mtext[256];
    int data;
};

typedef struct node
{
	int key;
	pid_t pid;
	struct node *prev;
	struct node *next;
}node;

typedef struct queue
{
	node *head;
	node *tail;
	int count;
}queue;

typedef enum {true, false}bool;

int CPU_burst_time[10];
int IO_burst_time[10];
int ProcessNum;
int time_count;
int child_msg[10];
static int tick_num = 0; //tick_num 1 -> 0.5 sec

void message_send(key_t key_id, struct msgbuf *sndbuf);
int parent_Receivemsg(int total_child, struct msgbuf *rcvbuffunc, key_t key_id);
bool isQueueEmpty(queue *Queue);

//DoubleLinkedList Queue
node *CreateNode(int key, pid_t pid);
void InitQueue(queue *Queue);
void Enqueue(queue *Queue, node *nod);
node *Dequeue(queue *Queue);

//buf for sprintf, write
char buf[256];



void timer_handler(int signum)
{
	tick_num ++;
	time_count++;
}



int main()
{    
	//File open
	int fd;
	int written;
	fd = open("/home/shin/Result.txt", O_RDWR);
	if(fd==-1)
	{
		printf("openError");
		return 1;
	}
	// set msgq
	key_t key_id[10];
	struct msgbuf sndbuf, rcvbuf;
	memset(sndbuf.mtext, 0x00, sizeof(sndbuf.mtext));
	memset(rcvbuf.mtext, 0x00, sizeof(rcvbuf.mtext));
	sndbuf.msgtype=2;
	sndbuf.data =0;
	rcvbuf.msgtype=2;
	rcvbuf.data=0;
	int k=0;
	int i=0;
	for(k=0;k<10;k++)
	{
		key_id[k]= msgget((key_t)(1234+k), IPC_CREAT|0666);
		if(key_id[k] ==-1)
		{
			perror("msgget error");
			exit(0);			
		}
	}

	//set timer
	struct sigaction sa;
	struct itimerval timer;
	time_count =0;
	tick_num =0;
	
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &timer_handler;
	sigaction(SIGVTALRM, &sa, NULL);
	
	timer.it_value.tv_sec =0;
	timer.it_value.tv_usec = 100000* TIMEQUANTUM;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 100000 * TIMEQUANTUM;
	
	setitimer(ITIMER_VIRTUAL, &timer, NULL);
	

	//child process fork
	ProcessNum = 10;
	int runningProcess = ProcessNum;

	pid_t pid[ProcessNum];
	for(i=0;i<ProcessNum;i++)
	{
		CPU_burst_time[i] = rand()%20 + 60;
		IO_burst_time[i] = rand()%10;
		pid[i] = 50;
		child_msg[i]=0;
		sprintf(buf, "Child No.%d, CPU Time:%d, IO Time:%d \n", i,CPU_burst_time[i],IO_burst_time[i]);
		written = write(fd, buf, strlen(buf));
		memset(buf, 0x00, 256);
	}

	for(i=0;i<ProcessNum;i++)
	{
	        pid[i] = fork();
	        
	        if(pid[i] == -1)
	        {
	            perror("fork error");
	            exit(1);
	        }
	        else if(pid[i] == 0)
	        {
	        	//Child Process i
	            printf("Child Process No.%d PID: %d ", i, (int)getpid() );
	            printf("CPUTIME:%d \n", CPU_burst_time[i]);
				struct msgbuf sndbufT, rcvbufT;
				int Num = i;
				memset(sndbufT.mtext, 0x00, sizeof(sndbuf.mtext));
				sndbufT.data = Num;
				sndbufT.msgtype = i+2;
				rcvbufT.data =0;
				rcvbufT.msgtype = i+2;
				key_t key_idt;

				key_idt = msgget((1234+i), IPC_CREAT|0666);
	
				if(key_idt<0)
				{
					perror("Child msgget error");
					exit(0);
				}

				while(CPU_burst_time[Num]>0)
				{
					if(msgrcv( key_idt, &rcvbufT, sizeof(struct msgbuf), 0, 0) == -1)
					{
						perror("msgrcv error");
						exit(0);
					}

					CPU_burst_time[Num]-=2;
					printf("\n Num: %d, left: %d \n", Num, CPU_burst_time[Num]);
					fflush(stdout);
				}
				
				sndbufT.data = IO_burst_time[i];
				message_send(key_id[i], &sndbufT);
				printf("child IO");
				fflush(stdout);
				sleep(IO_burst_time[Num]+30);
				printf("exit");
				fflush(stdout);
	            exit(100+i);
	         }  

	}
	//init readyqueue & waitqueue
	queue readyQueue;
	queue waitQueue;
	InitQueue(&readyQueue);
	InitQueue(&waitQueue);
	for(i=0;i<ProcessNum;i++)
	{
		Enqueue(&readyQueue, CreateNode(CPU_burst_time[i], i));
	}
	
	//Schedule Start
	node *nod = NULL;
	node *nodT = NULL;
	bool Q = true;

	while(runningProcess>0)
	{

		if(time_count == 2)
		{
			if((readyQueue.count)>0)
			{
				nod = Dequeue(&readyQueue);
				sndbuf.data = (int)(nod->pid);
				sndbuf.msgtype = (long)(sndbuf.data+2);
				if(CPU_burst_time[sndbuf.data]>0)
				{
					//Send message to child & decrease CPU_burst
					message_send(key_id[sndbuf.data], &sndbuf);
					CPU_burst_time[sndbuf.data]-=2;
					printf("Parentcheck: %d, %d \n",sndbuf.data, CPU_burst_time[sndbuf.data]);
					fflush(stdout);
					time_count =0;
					Enqueue(&readyQueue, nod);
				}
				else
				{
					//If CPU_burst is zero move to waitQueue and do I/O
					CPU_burst_time[sndbuf.data]=0;
					if(child_msg[sndbuf.data]==0)
					{
						if(msgrcv(key_id[sndbuf.data], &rcvbuf, sizeof(struct msgbuf), 0, 0)==-1)
						{
							perror("msgrcv error");
							exit(0);
						}
					child_msg[sndbuf.data]=1;
					Enqueue(&waitQueue, nod);
					printf("in IO");
					fflush(stdout);
					}
				}
				if(CPU_burst_time[sndbuf.data]<1)  CPU_burst_time[sndbuf.data]=0;
				sprintf(buf, "Time: %d, Child No.%d gets CPU time, Remaining:%d, Run Queue: ", tick_num/2, sndbuf.data, CPU_burst_time[sndbuf.data]);
				written = write(fd, buf, strlen(buf));
				memset(buf, 0x00, 256);
				for(i=0;i<readyQueue.count;i++)
				{
					nod = Dequeue(&readyQueue);
					sprintf(buf, "%d ", nod->pid);
					written = write(fd, buf, strlen(buf));
					Enqueue(&readyQueue, nod);
				}
			}
			else
			{
			sprintf(buf, "Time: %d, ",tick_num/2);
			written = write(fd, buf, strlen(buf));
			memset(buf, 0x00, 256);
			}
			
			sprintf(buf, "IO Wait Queue: ");
			written = write(fd, buf, strlen(buf));
			memset(buf, 0x00, 256);
			
			if(waitQueue.count>0)
			{
				//Schedule I/O queue
				nodT=Dequeue(&waitQueue);
				IO_burst_time[nodT->pid]-=2;
				printf("\nIO: %d \n", IO_burst_time[nodT->pid]);
				fflush(stdout);
				if(IO_burst_time[nodT->pid]<1) 
				{
					IO_burst_time[nodT->pid]=0;
					child_msg[nodT->pid]=2;
					runningProcess--;
				}
				else Enqueue(&waitQueue, nodT);
				
				for(i=0;i<waitQueue.count;i++)
				{
					nodT = Dequeue(&waitQueue);
					sprintf(buf, "%d ", nodT->pid);
					written = write(fd, buf, strlen(buf));
					Enqueue(&waitQueue, nodT);
				}
			}
			sprintf(buf, "\n");
			written = write(fd, buf, strlen(buf));
			memset(buf, 0x00, 256);	
			printf("\n runningPro: %d \n", runningProcess);
			fflush(stdout);
			time_count =0;
		}
		
	}
	
	printf("finish");
	fflush(stdout);
	close(fd);
    return 0;
}

int parent_Receivemsg(int total_child, struct msgbuf *rcvbuffunc, key_t key_id)
{
	int num=1;
	int check=10;
	if(msgrcv(key_id, &rcvbuffunc, sizeof(struct msgbuf), 0, 0)==-1)
	{
						perror("msgrcv error");
			exit(0);

	}
	printf("num: %d", num);
	fflush(stdout);
	return num;
}


void message_send(key_t key_id, struct msgbuf *sndbuf)
{
		if(msgsnd(key_id, &sndbuf, sizeof(struct msgbuf), IPC_NOWAIT) == -1)
		{
			perror("msgsnd error");
			exit(0);
		}
}

//Queue Functions
node *CreateNode(int Key, pid_t Pid)
{
	node *new = (node *)malloc(sizeof(node));
	new->key = Key;
	new->pid = Pid;
	new->prev = new->next = NULL;
	return new;

}
void InitQueue(queue *Queue)
{
	Queue->head = CreateNode(0,0);
	Queue->tail = CreateNode(0,0);
	Queue->head->next = Queue->tail;
	Queue->tail->prev = Queue->head;
	Queue->count =0;
}

bool isQueueEmpty(queue *Queue)
{
	if(Queue->head->next = Queue->tail) return true;
	else return false;

}

void Enqueue(queue *Queue, node *nod)
{
	nod->prev = Queue->tail->prev;
	nod->next = Queue->tail;
	Queue->tail->prev->next = nod;
	Queue->tail->prev = nod;
	Queue->count++;
}

node *Dequeue(queue *Queue)
{

	if(Queue->head->next != NULL)
	{
	node *popnode = Queue->head->next;
	popnode->prev->next = popnode->next;
	popnode->next->prev = popnode->prev;
	Queue->count--;
	return popnode;
	}
	else return NULL;
}




