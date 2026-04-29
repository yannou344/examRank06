#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <errno.h>

typedef struct e_parameters{
	int count ;
	int max_fd ;
	int ids[65536];
	char *msgs[65536];
	fd_set readyReadFds;
	fd_set activeFds;
	int sockfd;
	char buf_read[1024];
	char buf_write[42];
}t_parameters;

void initStruct(t_parameters* param){
	param->count = 0;
	param->max_fd = 0;
}


// START COPY-PASTE FROM GIVEN MAIN

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
			newbuf = calloc(1, sizeof(*newbuf) 
				* (strlen(*buf + i + 1) + 1));
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

// END COPY-PASTE


void	fatal_error()
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void	notify_other(int author, char *str, t_parameters* param)
{
	for (int fd = 0; fd <= param->max_fd; fd++)
	{
		if (fd != param->sockfd && fd != author 
				&& FD_ISSET(fd, &param->activeFds))
			send(fd, str, strlen(str), 0);
	}
}

void	register_client(int fd, t_parameters* param)
{
	param->max_fd = (fd > param->max_fd) ? fd : param->max_fd;
	param->ids[fd] = param->count++;
	param->msgs[fd] = NULL;
	FD_SET(fd, &param->activeFds);
	sprintf(param->buf_write, "server: client %d just arrived\n", 
		param->ids[fd]);
	notify_other(fd, param->buf_write, param);
}

void	remove_client(int fd, t_parameters* param)
{
	sprintf(param->buf_write, "server: client %d just left\n", 
		param->ids[fd]);
	notify_other(fd, param->buf_write, param);
	free(param->msgs[fd]);
	FD_CLR(fd, &param->activeFds);
	close(fd);
}

void	send_msg(int fd, t_parameters* param)
{
	char *msg;

	while (extract_message(&(param->msgs[fd]), &msg))
	{
		sprintf(param->buf_write, "client %d: ", param->ids[fd]);
		notify_other(fd, param->buf_write, param);
		notify_other(fd, msg, param);
		free(msg);
	}
}

int		create_socket(t_parameters* param)
{
	// START COPY-PASTE FROM MAIN
	param->max_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (param->max_fd < 0)
		fatal_error();
	// END COPY-PASTE FROM MAIN
	FD_SET(param->max_fd, &param->activeFds);
	return param->max_fd;
}

int		main(int ac, char **av)
{
	t_parameters param;
	struct sockaddr_in servaddr;

	initStruct(&param);
	if (ac != 2){
		write(2, "Wrong number of arguments\n", 26);
		return 1;
	}
	int port = atoi(av[1]);
	if (port <= 0 || port > 65535) // invalid port
		fatal_error();

	FD_ZERO(&param.activeFds);

	param.sockfd = create_socket(&param);

	// START COPY-PASTE FROM MAIN

	bzero(&servaddr, sizeof(servaddr));
	// assign IP, PORT 
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); // replace 8080
    // int reuse = 1;
    // if (setsockopt(param.sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
    //     perror("setsockopt(SO_REUSEADDR) failed");
	// #ifdef SO_REUSEPORT
	// 	if (setsockopt(param.sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) 
	// 		perror("setsockopt(SO_REUSEPORT) failed");
	// #endif

	// Binding newly created socket to given IP and verification
	int bindResult = bind(param.sockfd, 
		(const struct sockaddr *)&servaddr, 
		sizeof(servaddr));
	if (bindResult < 0){
		printf("Bind failed: %s\n", strerror(errno));
		fatal_error();
	}
		
	if (listen(param.sockfd, SOMAXCONN))
		fatal_error();

	// END COPY-PASTE

	while (1)
	{
		// param.readyReadFds = param.readyWriteFds = param.activeFds;
		param.readyReadFds = param.activeFds;
		// printf("waiting for socket\n");
		if (select(param.max_fd + 1, &param.readyReadFds, NULL, NULL, NULL)
				< 0)
			fatal_error();

		for (int fd = 0; fd <= param.max_fd; fd++)
		{
			if (!FD_ISSET(fd, &param.readyReadFds))
				continue;
			if (fd == param.sockfd)
			{
				socklen_t addr_len = sizeof(servaddr);
				int client_fd = accept(param.sockfd, 
					(struct sockaddr *)&servaddr, &addr_len);
				if (client_fd < 0)
                    fatal_error();
				if (client_fd >= 0)	{
					register_client(client_fd, &param);
					break ;
				}
			} else {
				int read_bytes = recv(fd, param.buf_read, 1023, 0);
				if (read_bytes <= 0){
					remove_client(fd, &param);
					break ;
				}
				param.buf_read[read_bytes] = '\0';
				param.msgs[fd] = str_join(param.msgs[fd], param.buf_read);
				if (param.msgs[fd] == NULL) // memory allocation failure
                    fatal_error();
				send_msg(fd, &param);
			}
		}
	}
	return 0;
}
