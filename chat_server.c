/*
 * Authors: 		Dinux, Shane Feek
 * Description:		Simple chatroom in C
 * Version:		2.0
 *
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "tinyexpr.h"

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#define MAX_COMPARES 12 /* Maximum number of compare strings */
#define MAX_COMPARE_LENGTH 20 /* Set for length of maximum compare string */
#define MAX_NAME_LENGTH 32 /* Max name length */
#define MAX_CLIENTS	100 /* Max number of clients */
#define MAX_BUFFER_LENGTH 1024 /* Max buffer size */

static unsigned int cli_count = 0;
static char colors[4][10] = {KGRN,KBLU,KMAG,KCYN};

/* Client structure */
typedef struct 
{
	struct sockaddr_in addr;	/* Client remote address */
	int connfd;					/* Connection file descriptor */
	int uid;					/* Client unique identifier */
	char name[MAX_NAME_LENGTH];	/* Client name */
	char room[MAX_NAME_LENGTH]; /* Client room */
} client_t;

client_t *clients[MAX_CLIENTS];

/* String compare case insensitive */
int strcicmp(char const *a, char const *b)
{
    for (;; a++, b++) {
        int d = tolower(*a) - tolower(*b);
        if (d != 0 || !*a)
            return d;
    }
}

/* Strip CRLF */
void strip_newline(char *s)
{
	while(*s != '\0')
	{
		if(*s == '\r' || *s == '\n')
		{
			*s = '\0';
		}
		s++;
	}
}

/* Add client to queue */
void queue_add(client_t *cl)
{
	int i;
	for(i=0;i<MAX_CLIENTS;i++)
	{
		if(!clients[i])
		{
			clients[i] = cl;
			return;
		}
	}
}

/* Delete client from queue */
void queue_delete(int uid)
{
	int i;
	for(i=0;i<MAX_CLIENTS;i++)
	{
		if(clients[i])
		{
			if(clients[i]->uid == uid)
			{
				clients[i] = NULL;
				return;
			}
		}
	}
}

/* Send message to all clients in the same room */
void send_message_all(char *s,char *room)
{
	int i;
	for(i=0;i<MAX_CLIENTS;i++)
	{
		if(clients[i])
		{
			if (!strcicmp(clients[i]->room,room))
			{
				if (write(clients[i]->connfd, s, strlen(s)) != strlen(s)) printf("\x1B[34mMessage failed to send %d\x1B[37m\n",clients[i]->uid);
			}
		}
	}
}

/* Send message to sender */
void send_message_self(const char *s, int connfd)
{
	if (write(connfd, s, strlen(s)) != strlen(s)) printf("\x1B[34mSelf message failed to send\x1B[37m\n");
}

/* Send message to specific client, regardless of room */
void send_message_client(char *s, int uid)
{
	int i;
	for(i=0;i<MAX_CLIENTS;i++)
	{
		if(clients[i])
		{
			if(clients[i]->uid == uid)
			{
				if (write(clients[i]->connfd, s, strlen(s)) != strlen(s)) printf("\x1B[34mMessage failed to send %d\x1B[37m\n",clients[i]->uid);
			}
		}
	}
}

/* Send list of active clients */
void send_active_clients(int connfd)
{
	int i;
	char s[64];

	for(i=0;i<MAX_CLIENTS;i++)
	{
		if(clients[i])
		{
			sprintf(s, "  %s<%s>[%s]\x1B[37m\r\n", colors[clients[i]->uid % 4],clients[i]->room, clients[i]->name);
			send_message_self(s, connfd);
		}
	}
}

/* Send list of active clients in a specific room */
void send_active_clients_room(int connfd,char *room)
{
	int i;
	char s[64];

	for(i=0;i<MAX_CLIENTS;i++)
	{
		if(clients[i])
		{
			if (!strcicmp(clients[i]->room,room))
			{
				sprintf(s, "  %s[%s]\x1B[37m\r\n", colors[clients[i]->uid % 4], clients[i]->name);
				send_message_self(s, connfd);
			}
		}
	}
}

/* Print ip address */
void print_client_addr(struct sockaddr_in addr)
{
	printf("%d.%d.%d.%d\n",
		addr.sin_addr.s_addr & 0xFF,
		(addr.sin_addr.s_addr & 0xFF00)>>8,
		(addr.sin_addr.s_addr & 0xFF0000)>>16,
		(addr.sin_addr.s_addr & 0xFF000000)>>24);
}

/* Handle all communication with the client */
void *handle_client(void *arg)
{
	char buff_out[MAX_BUFFER_LENGTH];
	char buff_in[MAX_BUFFER_LENGTH];
	int rlen;
	char *cmp[MAX_COMPARES];
	int i;
	char *param;
	int quit=0;
	int used;
	int x;

	 /* Comparison string array */
	for(i=0; i<MAX_COMPARES; i++) cmp[i] = calloc(MAX_COMPARE_LENGTH, sizeof(char));

	strcpy(cmp[0],"\\quit"); /* Comparison candidates must be all lower case */
	strcpy(cmp[1],"\\ping"); /* Index number must match switch cases below */
	strcpy(cmp[2],"\\nick");
	strcpy(cmp[3],"\\pm");
	strcpy(cmp[4],"\\who");
	strcpy(cmp[5],"\\me");
	strcpy(cmp[6],"\\help");
	strcpy(cmp[7],"\\room");
	strcpy(cmp[8],"\\time");
	strcpy(cmp[9],"\\math");

	/* Add one to the client counter */
	cli_count++;
	client_t *cli = (client_t *)arg;

	printf("\x1B[33mACCEPT\x1B[37m ");
	print_client_addr(cli->addr);

	sprintf(buff_out, "\x1B[33mJOIN, WELCOME\x1B[37m %s\r\n", cli->name);
	send_message_all(buff_out,cli->room);

	/* Receive input from client */
	while((rlen = read(cli->connfd, buff_in, sizeof(buff_in)-1)) > 0)
	{
	    buff_in[rlen] = '\0';
	    buff_out[0] = '\0';
		strip_newline(buff_in);

		/* Ignore empty buffer */
		if(!strlen(buff_in)) continue;

		/* Look for command tokens */
		if(buff_in[0] == '\\')
		{
			strtok(buff_in," ");

			/* Compare strings until we hit an empty candidate string or get a match */	
			for(i=0; *cmp[i]; i++)
			{
				if(!strcicmp(buff_in,cmp[i]))
				{
					switch(i) /* Found a match, choose correct case */
					{
						case 0: /* Quit */
						{
							quit=1;
							break;
						}

						case 1: /* Ping */
						{
							send_message_self("\x1B[33mPONG\x1B[37m\r\n", cli->connfd);
							break;
						}

						case 2: /* Name */
						{
							param = strtok(NULL, " ");
							if(param)
							{
								/* Chop name if too long */
								param[MAX_NAME_LENGTH]=0;

								used = 0;
								/* Check for existing name */
								for(x=0;x<MAX_CLIENTS;x++)
								{
									if(clients[x])
									{
										if (!strcicmp(clients[x]->name,param)) 
										{
											send_message_self("\x1B[33mNAME ALREADY EXISTS\x1B[37m\r\n", cli->connfd);
											used=1;
										}
									}
								}

								/* Stop if name already used */
								if (used) break;

								/* Change the Name */
								char *old_name = strdup(cli->name);
								strcpy(cli->name, param);
								sprintf(buff_out, "\x1B[33mRENAME\x1B[37m %s TO %s\r\n", old_name, cli->name);
								free(old_name);
								send_message_all(buff_out,cli->room);
							}
							else
							{
								send_message_self("\x1B[33mNAME CANNOT BE NULL\x1B[37m\r\n", cli->connfd);
							}
							break;
						}

						case 3: /* Private */
						{
							param = strtok(NULL, " ");
							if(param)
							{
								/* Look up user ID */
								int uid = -1;
								for(int x=0;x<MAX_CLIENTS;x++)
								{
									if(clients[x])
									{
										if (!strcicmp(clients[x]->name,param)) uid = clients[x]->uid;
									}
								}

								/* Check if a valid user was chosen */
								if (uid == -1)
								{
									sprintf(buff_out, "\x1B[33mUNKNOWN USER\x1B[37m - [%s]\r\n", param);
									send_message_self(buff_out, cli->connfd);
									break;
								} 

								/* Send the PM */
								param = strtok(NULL, " ");
								if(param)
								{
									sprintf(buff_out, "\x1B[31m[PM]%s<%s>[%s]\x1B[37m",colors[cli->uid % 4], cli->room, cli->name);
									while(param != NULL)
									{
										strcat(buff_out, " ");
										strcat(buff_out, param);
										param = strtok(NULL, " ");
									}
									strcat(buff_out, "\r\n");
									send_message_client(buff_out, uid);
								}
								else
								{
									send_message_self("\x1B[33mMESSAGE CANNOT BE NULL\x1B[37m\r\n", cli->connfd);
								}
							}
							else
							{
								send_message_self("\x1B[33mUSER CANNOT BE NULL\x1B[37m\r\n", cli->connfd);
							}
							break;
						}

						case 4: /* Who */
						{
							sprintf(buff_out, "\x1B[33mCLIENTS\x1B[37m %d\r\n", cli_count);
							send_message_self(buff_out, cli->connfd);
							send_active_clients(cli->connfd);
							break;
						}

						case 5: /* Me */
						{
							param = strtok(NULL, " ");
							if(param)
							{
								sprintf(buff_out, "%s*** %s",colors[cli->uid % 4], cli->name);
								while(param != NULL)
								{
									strcat(buff_out, " ");
									strcat(buff_out, param);
									param = strtok(NULL, " ");
								}
								strcat(buff_out, " ***\x1B[37m\r\n");
								send_message_all(buff_out,cli->room);
							}
							else
							{
								send_message_self("\x1B[33mMESSAGE CANNOT BE NULL\x1B[37m\r\n", cli->connfd);
							}
							break;
						}

						case 6: /* Help */
						{
							strcat(buff_out, "\x1B[32m\\quit\x1B[37m     Quit chatroom\r\n");
							strcat(buff_out, "\x1B[32m\\me\x1B[37m       <message> Emote\r\n");
							strcat(buff_out, "\x1B[32m\\ping\x1B[37m     Server test\r\n");
							strcat(buff_out, "\x1B[32m\\nick\x1B[37m     <name> Change nickname\r\n");
							strcat(buff_out, "\x1B[32m\\pm\x1B[37m       <name> <message> Send private message\r\n");
							strcat(buff_out, "\x1B[32m\\who\x1B[37m      Show active clients\r\n");
							strcat(buff_out, "\x1B[32m\\help\x1B[37m     Show this help screen\r\n");
							strcat(buff_out, "\x1B[32m\\room\x1B[37m     <room_name> Move to another room or show who is in the current room\r\n");
							strcat(buff_out, "\x1B[32m\\time\x1B[37m     Show the current server time\r\n");
							strcat(buff_out, "\x1B[32m\\math\x1B[37m     <expression> Evaluate a math expression\r\n");
							send_message_self(buff_out, cli->connfd);
							break;
						}

						case 7: /* Room */
						{
							param = strtok(NULL, " ");
							if(param)
							{
								/* Chop name if too long */
								param[MAX_NAME_LENGTH]=0;

								/* Change the room */
								char *old_room = strdup(cli->room);
								strcpy(cli->room, param);
								sprintf(buff_out, "\x1B[33mLEAVE %s[%s]\x1B[37m MOVED TO <%s>\r\n", colors[cli->uid % 4],cli->name, cli->room);
     							send_message_all(buff_out, old_room);

								sprintf(buff_out, "\x1B[33mJOIN, WELCOME %s[%s]\x1B[37m\r\n",colors[cli->uid % 4], cli->name);
								send_message_all(buff_out, cli->room);
								free(old_room);
							}
							else
							{
								int count=0;
								/* Count clients in a room */
								for(x=0;x<MAX_CLIENTS;x++)
								{
									if(clients[x])
									{
										if (!strcicmp(clients[x]->room,cli->room)) count++;
									}
								}

								/* Show clients in the room */
								sprintf(buff_out, "\x1B[33mROOM NAME\x1B[37m <%s> | \x1B[33mCLIENTS\x1B[37m %d\r\n", cli->room, count);
								send_message_self(buff_out, cli->connfd);
								send_active_clients_room(cli->connfd,cli->room);
							}
							break;
						}

						case 8: /* Time */
						{
							time_t rawtime;
  							struct tm * timeinfo;

							time (&rawtime);
							timeinfo = localtime (&rawtime);
							sprintf(buff_out, "\x1B[33mTIME\x1B[37m  %s", asctime(timeinfo));
							send_message_self(buff_out, cli->connfd);
							break;
						}

						case 9: /* Math */
						{
							param = strtok(NULL, " ");
							if(param)
							{
								sprintf(buff_out,"\x1B[33mMATH\x1B[37m  %s = %f\r\n", param, te_interp(param,0));
								send_message_self(buff_out, cli->connfd);
							}
							else
							{
								send_message_self("\x1B[33mMATH MISSING EXPRESSION\x1B[37m\r\n", cli->connfd);
							}
							break;
						}
					}
					break;
				}
			}

			/* Look for bad command */
			if (i>9) send_message_self("\x1B[33mUNKNOWN COMMAND\x1B[37m\r\n", cli->connfd);
			
			/* Leave the loop if user chooses to quit */
			if (quit) break;
		}
		else
		{
			/* No Command, Send as message */
			sprintf(buff_out, "%s<%s>[%s]\x1B[37m %s\r\n", colors[cli->uid % 4], cli->room, cli->name, buff_in);
			send_message_all(buff_out,cli->room);
		}
	}

	/* Close connection */
	close(cli->connfd);
	sprintf(buff_out, "\x1B[33mLEAVE, BYE\x1B[37m %s\r\n", cli->name);
	send_message_all(buff_out,cli->room);

	/* Free comparison strings */
	for(i=0; i<MAX_COMPARES; i++)
	{
		if(cmp[i]) free(cmp[i]);
	}

	/* Delete client from queue and yield thread */
	queue_delete(cli->uid);
	printf("\x1B[33mLEAVE\x1B[37m ");
	print_client_addr(cli->addr);
	free(cli);
	cli_count--;
	pthread_detach(pthread_self());
	
	return NULL;
}

/* Chat Server Main */
int main(int argc, char *argv[]){
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;

	/* Socket settings */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(5000); 

	/* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);
	
	/* Bind */
	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("\x1B[34mSocket binding failed\x1B[37m");
		return 1;
	}

	/* Listen */
	if(listen(listenfd, 10) < 0)
	{
		perror("\x1B[34mSocket listening failed\x1B[37m");
		return 1;
	}

	printf("\x1B[34m<[SERVER STARTED]>\x1B[37m\n");

	/* Accept clients */
	while(1)
	{
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		/* Check if max clients is reached */
		if((cli_count+1) == MAX_CLIENTS)
		{
			printf("\x1B[34mMAX CLIENTS REACHED\x1B[37m\n");
			printf("\x1B[34mREJECT\x1B[37m ");
			print_client_addr(cli_addr);
			printf("\n");
			close(connfd);
			continue;
		}

		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->addr = cli_addr;
		cli->connfd = connfd;

		/* Find next available client UID */
		for(int i=0;i<MAX_CLIENTS;i++)
		{
			if(!clients[i])
			{
				cli->uid = i;
				break;
			}
		}
		sprintf(cli->name, "%d", cli->uid);
		sprintf(cli->room, "Common");

		/* Add client to the queue and fork thread */
		queue_add(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
	}
}
