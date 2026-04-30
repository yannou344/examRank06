#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct s_parameter
{
    int clientCount;
    int maxFd; 
    int ids[65536];
    char *msg[65536];
    fd_set readyReadFds;
    fd_set activeFds;
    int sockFd; 
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

void initStruct(t_parameter *param){
    param->maxFd = 0;
    param->sockFd = 0;
    param->clientCount = 0;
}

int fatalError(){
    write(2, "Fatal error\n", 12);
    exit(1);
}

void alertOtherClient(int clientFd, char *str, 
        t_parameter *param){
    for (int fd = 0; fd <= param->maxFd; ++fd){
        if (fd != clientFd && fd != param->sockFd 
            && FD_ISSET(fd, &param->activeFds) )
            send(fd, str, strlen(str), 0);
    }
    printf("Finish alerting other client\n");
}

void registerClient(int clientFd, t_parameter *param){
    param->maxFd = (clientFd > param->maxFd) ? clientFd : param->maxFd;
    param->ids[clientFd] = param->clientCount++;
    param->msg[clientFd] = NULL;
    FD_SET(clientFd, &param->activeFds);
    printf("into registerClient\n");
    sprintf(param->bufWrite, "server: client %d just arrived\n",
         param->ids[clientFd]);
    alertOtherClient(clientFd, param->bufWrite, param);
}

void removeClient(int clientFd, t_parameter *param){
    sprintf(param->bufWrite, "client %d just left", 
        param->ids[clientFd]);
    alertOtherClient(clientFd, param->bufWrite, param);
    FD_CLR(clientFd, &param->activeFds);
    free(param->msg[clientFd]);
    close(clientFd);
}

void sendMessage(int clientFd, t_parameter *param){
    char *msg;

	while (extract_message(&(param->msg[clientFd]), &msg))
	{
        sprintf(param->bufWrite, "client %d: ", 
            param->ids[clientFd]);
        alertOtherClient(clientFd, param->bufWrite, param);
        alertOtherClient(clientFd, msg, param);
        free(msg);
    }
}

int createSocket(t_parameter *param){
    param->sockFd = socket(AF_INET, SOCK_STREAM, 0); 
	if (param->sockFd == -1) { 
		printf("socket creation failed...\n"); 
		fatalError(); 
	}
    FD_SET(param->sockFd, &param->activeFds);
    return param->sockFd;
}

int main(int argc, char**argv){
    t_parameter param;
    struct sockaddr_in servaddr;
    int port;

    initStruct(&param);

    if (argc != 2){
        write(2, "Wrong number of arguments\n", 26);
        return 1;
    }
    port = atoi(argv[1]);
    if (port < 0 || port > 65535)
        fatalError();
    
    FD_ZERO(&param.activeFds);
    
    param.maxFd = createSocket(&param);

    bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 

	// Binding newly created socket to given IP and verification 
	if ((bind(param.sockFd, (const struct sockaddr *)&servaddr, 
            sizeof(servaddr))) < 0) { 
		printf("socket bind failed...\n"); 
		// exit(0);
        fatalError();
	} 
	// else
	printf("Socket successfully binded..\n");
	if (listen(param.sockFd, SOMAXCONN) != 0) {    //10
		printf("cannot listen\n"); 
		// exit(0);
        fatalError();
	}
    printf("Entering into loop..\n");
    while (1){ 
        param.readyReadFds = param.activeFds;
        if (select(param.maxFd + 1, &param.readyReadFds, NULL, NULL,
                 NULL) < 0)
            fatalError();
        printf("something is occuring...\n"); 
        for (int fd = 0; fd <= param.maxFd; fd++){
            if (!FD_ISSET(fd, &param.readyReadFds))
                continue;
            if (fd == param.sockFd){
                socklen_t servaddr_len = sizeof(servaddr);
                int clientFd = accept(param.sockFd, 
                    (struct sockaddr *)&servaddr, &servaddr_len);
                if (clientFd < 0) { 
                    printf("server acccept failed...\n"); 
                    // exit(0);
                    fatalError();
                } 
                printf("server accept the client...\n");
                registerClient(clientFd, &param);
                break;
            } 
            else{
                ssize_t recvSize = recv(fd, param.bufRead, 1023, 0);
                if (recvSize <= 0){
                    removeClient(fd, &param);
                    break;
                }
                param.bufRead[recvSize] = '\0';
                param.msg[fd] = str_join(param.msg[fd], param.bufRead);
				if (param.msg[fd] == NULL) // memory allocation failure
                    fatalError();
				sendMessage(fd, &param);
            }
        }
    }
    return 0;
}