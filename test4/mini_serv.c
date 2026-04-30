#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

typedef struct s_parameter{
	int maxFd;
	int sockFd;
	int clientCounter;
	int ids[65636];
	char *msg[65536];
	fd_set activeFds;
	fd_set readyReadFds;
	char bufRead[1024];
	char bufWrite[42];
}t_parameter;

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void initParam(t_parameter *param){
	param->maxFd = 0;
	param->sockFd = 0;
	param->clientCounter = 0;
	// for (int i = 0; i < 1024; i++)
	// 	param->bufRead[i] = '\0';
	// for (int i = 0; i < 42; i++)
	// 	param->bufWrite[i] = '\0';
}

int fatalError(){
	write(2, "fatal error\n", 12);
	exit(0);
}

int createSocket(t_parameter *param){
	param->sockFd = socket(AF_INET, SOCK_STREAM, 0); 
	if (param->sockFd == -1) { 
		printf("socket creation failed...\n"); 
		fatalError();
	} 
	else{
		printf("Socket successfully created..\n"); 
	}
	FD_SET(param->sockFd, &param->activeFds);
	return param->sockFd;
		
}



void warningMessage(int clientFd, char *str, t_parameter *param){
	for (int fd = 0; fd <= param->maxFd; ++fd){
		printf("fd = %d\n", fd);
		if (fd != clientFd && fd != param->sockFd 
				&& FD_ISSET(fd, &param->activeFds)){
			send(fd, str, strlen(str), 0);
			printf("warning message sent to client %d\n", param->ids[clientFd]);
		}
	}
	
}

void registerClient(int clientFd, t_parameter *param){
	param->maxFd = (param->maxFd < clientFd) ? clientFd : param->maxFd;
	FD_SET(clientFd, &param->activeFds);
	param->ids[clientFd] = param->clientCounter++;
	sprintf(param->bufWrite, "server: client %d just arrived\n", 
		param->ids[clientFd]);
	warningMessage(clientFd, param->bufWrite, param);
}

void removeClient(int clientFd, t_parameter *param){
	FD_CLR(clientFd, &param->activeFds);
	free (param->msg[clientFd]);
	sprintf(param->bufWrite, "server: client %d just left\n", 
		param->ids[clientFd]);
	warningMessage(clientFd, param->bufWrite, param);
	close(clientFd);
}

void sendMessage(int clientFd, t_parameter *param){
	char *msg;

	while (extract_message(&(param->msg[clientFd]), &msg))
	{
		sprintf(param->bufWrite, "client %d: ", param->ids[clientFd]);
		warningMessage(clientFd, param->bufWrite, param);
		warningMessage(clientFd, msg, param);
		free(msg);
	}
}

int main() {
	unsigned int len;
	struct sockaddr_in servaddr, cli; 
	t_parameter param;

	initParam(&param);

	FD_ZERO(&param.activeFds);
	// socket create and verification
	param.maxFd = createSocket(&param);

	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(8081); 

	// Binding newly created socket to given IP and verification 
	if ((bind(param.sockFd, (const struct sockaddr *)&servaddr, 
			sizeof(servaddr))) != 0) { 
		printf("socket bind failed...\n"); 
		fatalError();
	} 
	else{
		printf("Socket successfully binded..\n");
	}

	if (listen(param.sockFd, SOMAXCONN) != 0) { //10
		printf("cannot listen\n"); 
		fatalError();
	}

	while(1){ 
		param.readyReadFds = param.activeFds;
		printf("waiting for any event\n"); 
		if (select(param.maxFd + 1, &param.readyReadFds, NULL, NULL,
			 	NULL) < 0)
			fatalError();
		printf("event occured\n"); 
		for (int fd = 0; fd <= param.maxFd; ++fd){
			if (!FD_ISSET(fd, &param.readyReadFds))
				continue;
			if (fd == param.sockFd){
				len = sizeof(cli);
				int clientFd = accept(param.sockFd, 
					(struct sockaddr *)&cli, &len);
				if ( clientFd < 0){ 
					printf("server accept failed...\n");
					fatalError();
				}
				printf("server accept the client...\n");
				registerClient(clientFd, &param);
				break;
			}
			else {
				printf("client already existing\n"); 
				ssize_t sizeRecv = recv(fd, param.bufRead, 1023, 0);
				if (sizeRecv <= 0){
					printf("reception message failed...\n");
					removeClient(fd, &param);
					break;
				}
				param.bufRead[sizeRecv] = '\0';
				printf("reception message from client %d: %s\n", param.ids[fd],
					param.bufRead);
				param.msg[fd] = str_join(param.msg[fd], param.bufRead);
				if (param.msg[fd] == NULL) // memory allocation failure
                    fatalError();
				sendMessage(fd, &param);

			}
		}
	}
	return 0;	
}