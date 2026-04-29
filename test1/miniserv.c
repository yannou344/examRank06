static Socket createTcpSocket() {
	const int tcp_fd = socket(AF_INET, SOCK_STREAM,
		0);
	if (tcp_fd == -1) {
		throw EndpointError::create(invalid_socket);
	}
	int opt = 1;
	if (setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR,
			&opt, sizeof(opt)) == -1) {
		close(tcp_fd);
		throw EndpointError::create(invalid_socket);
	}
	return Socket(tcp_fd);
}

void listenSocket(Endpoint endpoint) const {
	if (bind(m_fd, static_cast<struct sockaddr *>(
			endpoint.getSockAddr()),sizeof(struct sockaddr))
			== -1)
		throw EndpointError::create(invalid_bind,
			strerror(errno));
	if (listen(m_fd, 10) == -1)
		throw EndpointError::create(invalid_listen,
			strerror(errno));
}

void setupServers(IIOMultiplexer* multiplexer){
	for (size_t index = 0; index < m_configs.size(); ++index) {
		int port = m_configs[index]->getPort().getValue();
		std::string ip = m_configs[index]->getIPAddress().getValue();

		try {
			Socket tempSocket = Server::listen(ip, port);
			Socket* listenerSocket = new Socket(tempSocket);
			int fd = listenerSocket->getSockFd();
			m_listenConfig[fd] = m_configs[index];
			m_listenerSockets[fd] = listenerSocket;
			if (fd > m_maxFd)
				m_maxFd = fd;
			multiplexer->addFd(fd);
			std::stringstream resultInfo;
			resultInfo << ip << ":" << port << " (FD: " << fd << ")";
			Logger::messagesFilter(INFO,
				"Server listening on ",
				resultInfo.str());
		}
		catch (std::exception &e) {
			Logger::messagesFilter(ERR,
				"Server setup failed: ",
							e.what());
		}
	}
}

void run() {
			setupServers();
			while (!Init::stopRequested) {
				// 1. Prepare Write Set based on Clients who have data
        		// (You might need a helper in ConnectionManager to loop clients)
        		// For now, let's assume you do it via add/remove logic or just 
				// iterate: ideally, m_multiplexer should allow dynamic updates.
				int maxFd = m_connectionManager->getMaxFd();
				int activity = m_multiplexer->wait(maxFd);
				if (activity < 0) {
					Logger::messagesFilter(DEBUG,
						"Server Manager::run: activity < 0!", "");
					int errValue = errno;
					if (errValue == EINTR)
						continue;
					Logger::messagesFilter(ERR,
						"ServerManager::run: activity < 0, Select error:",
									strerror(errValue));
					continue;
				}
				std::vector<int> readyReadFds
					= m_multiplexer->getReadyReadFds();
				// PROCESS READ
				for (size_t i = 0; i < readyReadFds.size(); ++i){
					int fd = readyReadFds[i];
					if (m_connectionManager->isListener(fd)) {
						acceptNewConnection(fd);
					} else {
						handleClientActivity(fd);
					}
					m_connectionManager->checkTimeouts(m_multiplexer);
				}
				// PROCESS WRITE
				std::vector<int> writeFds = m_multiplexer->getReadyWriteFds();
				for (size_t i = 0; i < writeFds.size(); ++i) {
					int fd = writeFds[i];
					Client* client = m_connectionManager->getClient(fd);
					if (client) {
						if (isSendFailedFatally(client))
							m_connectionManager->removeClient(fd, m_multiplexer);
						else if (!client->hasPendingData()) {
							m_multiplexer->stopListeningWriting(fd);
							if (client->readyToClose()) {
								m_connectionManager->removeClient(
									fd, m_multiplexer);
							}
						}
					}
					m_connectionManager->checkTimeouts(m_multiplexer);	
				}
			}
		}