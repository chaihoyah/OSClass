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
#include <time.h>

#define TIMEQUANTUM 1 //Real Timequantum: 0.2 sec
#define PASIZE 180


struct msgbuf
{
    long msgtype; //parent 1, child 2~11
    char mtext[256];
    int data;
};

struct msgbuf_child
{
	long msgtype_child;
//	char mtext_child[256];
	int data_child[10];

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
int virtual_address[200];//ProcessNum 10 X each PageNum 20
int page_table[10][20];//ProcessNum 10 X each PageNum 20
int pt_counter[10][20];
int MMU[10];
int physical_address[256];
int free_address;

static int tick_num = 0; //tick_num 1 -> 0.1 sec

void message_send(key_t key_id, struct msgbuf *sndbuf);
void message_sendChild(key_t key_id, struct msgbuf_child *sndbuf);
int parent_Receivemsg(int total_child, struct msgbuf *rcvbuffunc, key_t key_id);
bool isQueueEmpty(queue *Queue);

int minFind(int arr[][20]);

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
	struct msgbuf_child rcvbufC;
	memset(sndbuf.mtext, 0x00, sizeof(sndbuf.mtext));
	memset(rcvbuf.mtext, 0x00, sizeof(rcvbuf.mtext));
	//memset(rcvbufC.mtext_child, 0x00, sizeof(rcvbufC.mtext_child));
	memset(rcvbufC.data_child, 0, 10*sizeof(int));
	sndbuf.msgtype=2;
	sndbuf.data =0;
	rcvbuf.msgtype=2;
	rcvbuf.data=0;
	free_address=0;
	int k=0;
	int i=0;
	srand(time(NULL));
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
		MMU[i] = i*20;
		//sprintf(buf, "Child No.%d, CPU Time:%d, IO Time:%d \n", i,CPU_burst_time[i],IO_burst_time[i]);
		written = write(fd, buf, strlen(buf));
		memset(buf, 0x00, 256);
		//page table reset
		for(k=0;k<20;k++)
		{
			page_table[i][k] = -1;
			pt_counter[i][k] = 0;
		}
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
				struct msgbuf_child sndbufC;
				int Num = i;
				int k=0;
				memset(sndbufT.mtext, 0x00, sizeof(sndbuf.mtext));
				//memset(sndbufC.mtext_child, 0x00, sizeof(sndbufC.mtext_child));
				memset(sndbufC.data_child,0,10*sizeof(int));
				sndbufT.data = Num;
				sndbufT.msgtype = i+2;
				rcvbufT.data =0;
				rcvbufT.msgtype = i+2;
				sndbufC.msgtype_child = 1;
				srand(time(NULL));
				for(k=0;k<10;k++)
				{
					sndbufC.data_child[k] = k*2+1;
				
				}
				
				key_t key_idt;

				key_idt = msgget((1234+i), IPC_CREAT|0666);
	
				if(key_idt<0)
				{
					perror("Child msgget error");
					exit(0);
				}

				while(CPU_burst_time[Num]>0)
				{
					if(msgrcv( key_idt, &rcvbufT, sizeof(struct msgbuf)-sizeof(long), i+2, MSG_NOERROR) == -1)
					{
						perror("msgrcv error");
						exit(0);
					}
					//sndbufC data random set
					for(k=0;k<10;k++)
					{
						sndbufC.data_child[k] = rand()%20 + i*20;
					}
					printf("\n sndbufC: %d, %d, %d", sndbufC.data_child[0], sndbufC.data_child[1], sndbufC.data_child[2]);
					fflush(stdout);
					//message_sendChild(key_id[i], &sndbufC);
					if(msgsnd(key_idt, &sndbufC, sizeof(struct msgbuf_child)-sizeof(long), IPC_NOWAIT) == -1)
					{
						perror("msgsnd error");
						exit(0);
					}
					
					
					CPU_burst_time[Num]-=2;
				}
				
				sndbufT.data = IO_burst_time[i];
				sndbufT.msgtype = 1;
				if(msgsnd(key_idt, &sndbufT, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT) == -1)
				{
					perror("msgsnd error");
					exit(0);
				}
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
	int temp=0;
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
					//message_send(key_id[sndbuf.data], &sndbuf);
					if(msgsnd(key_id[sndbuf.data], &sndbuf, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT) == -1)
					{
						perror("msgsnd error");
						exit(0);
					}
					
					if(msgrcv(key_id[sndbuf.data], &rcvbufC, sizeof(struct msgbuf_child)-sizeof(long), 1, MSG_NOERROR)==-1)
					{
						perror("msgrcv error");
						exit(0);
					}
					sprintf(buf, "Time: %d, Child No.%d,", tick_num/2, sndbuf.data);
					for(i=0;i<10;i++)
					{
					sprintf(buf, " Requests: VA[%d]",rcvbufC.data_child[i]);
						if(page_table[sndbuf.data][rcvbufC.data_child[i]-20*(sndbuf.data)]==-1)
						{
					sprintf(buf, " Page fault change page_table[%d][%d] to PA[%d], ", sndbuf.data, rcvbufC.data_child[i]-20*(sndbuf.data),free_address);
							if(free_address>PASIZE-1)
							{
							//Swap out LRU and swap in new
								temp=minFind(pt_counter);
								page_table[(int)(temp/20)][temp-((int)(temp/20))*20]= -1;
								physical_address[temp]= rcvbufC.data_child[i];
								page_table[sndbuf.data][rcvbufC.data_child[i]-20*(sndbuf.data)]=temp;
								pt_counter[sndbuf.data][rcvbufC.data_child[i]-20*(sndbuf.data)]=tick_num;
								printf("pt[%d][%d] changed HA[%d] changed to %d ",(int)(temp/20),temp-((int)(temp/20))*20,temp,rcvbufC.data_child[i]);
								sprintf(buf, "PA[%d] swapped out page_table[%d][%d] changed, ", temp, (int)(temp/20),temp-((int)(temp/20))*20); 
							}
							else
							{
								//get memory from PA, set pagetable
								physical_address[free_address]= rcvbufC.data_child[i];
								page_table[sndbuf.data][rcvbufC.data_child[i]-20*(sndbuf.data)]=free_address;
								free_address++;
								printf("\n PA[%d]:%d, child:%d, pt[%d]:%d",free_address-1,physical_address[free_address-1],sndbuf.data,rcvbufC.data_child[i]-20*(sndbuf.data),page_table[sndbuf.data][rcvbufC.data_child[i]-20*(sndbuf.data)]);
								pt_counter[sndbuf.data][rcvbufC.data_child[i]-20*(sndbuf.data)] = tick_num;
							}
						}
						else
						{
							sprintf(buf, "page_table[%d][%d] accessed PA[%d], ",sndbuf.data, rcvbufC.data_child[i]-20*(sndbuf.data), page_table[sndbuf.data][rcvbufC.data_child[i]-20*(sndbuf.data)]);
						}
					written = write(fd, buf, strlen(buf));
					memset(buf, 0x00, 256);
					}
					
					printf("bufC: %d, %d, %d \n", rcvbufC.data_child[0], rcvbufC.data_child[1], rcvbufC.data_child[2]);
					
					fflush(stdout);
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
						if(msgrcv(key_id[sndbuf.data], &rcvbuf, sizeof(struct msgbuf)-sizeof(long), 1, MSG_NOERROR)==-1)
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
				//sprintf(buf, "Time: %d, Child No.%d gets CPU time, Remaining:%d, Run Queue: ", tick_num/2, sndbuf.data, CPU_burst_time[sndbuf.data]);
				written = write(fd, buf, strlen(buf));
				memset(buf, 0x00, 256);
				for(i=0;i<readyQueue.count;i++)
				{
					nod = Dequeue(&readyQueue);
					//sprintf(buf, "%d ", nod->pid);
					written = write(fd, buf, strlen(buf));
					Enqueue(&readyQueue, nod);
				}
			}
			else
			{
			//sprintf(buf, "Time: %d, ",tick_num/2);
			written = write(fd, buf, strlen(buf));
			memset(buf, 0x00, 256);
			}
			
			//sprintf(buf, "IO Wait Queue: ");
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
					//sprintf(buf, "%d ", nodT->pid);
					written = write(fd, buf, strlen(buf));
					Enqueue(&waitQueue, nodT);
				}
			}
			//sprintf(buf, "\n");
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
		if(msgsnd(key_id, &sndbuf, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT) == -1)
		{
			perror("msgsnd error");
			exit(0);
		}
}

void message_sendChild(key_t key_id, struct msgbuf_child *sndbuf)
{
		if(msgsnd(key_id, &sndbuf, sizeof(struct msgbuf_child)-sizeof(long), IPC_NOWAIT) == -1)
		{
			perror("msgsnd error");
			exit(0);
		}
}

//Memory Management
/**int get_freeframe(int *PA)
{
	int i=0;
	for(i=0;i<sizeof(PA)/sizeof(int);i++)
	{
		if(PA[i] == -1)
		{
			break;
		}
	}
	return i;
}**/
//Find min
int minFind(int arr[][20])
{
	int i=0;
	int k=0;
	int minimum =300;
	int minindex =0;
	for(i=0;i<10;i++)
	{
		for(k=0;k<20;k++)
		{
			if(arr[i][k]<minimum && page_table[i][k]!=-1)
			{
				minimum = arr[i][k];
				minindex = i*20+k;
			}
		}
	}
	
	return minindex;
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




