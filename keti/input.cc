#include "input.h"

void Input::InputSnippet(){
	cout << "<-----------  Input Layer Running...  ----------->\n";

	int server_fd;
	int client_fd;
	int opt = 1;
	struct sockaddr_in serv_addr; // 소켓주소
	struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(INPUT_IF_PORT); // port

	if (bind(server_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		perror("bind");
		exit(EXIT_FAILURE);
	} // 소켓을 지정 주소와 포트에 바인딩

	if (listen(server_fd, 3) < 0){
		perror("listen");
		exit(EXIT_FAILURE);
	} // 리스닝

	while(1){
		if ((client_fd = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen)) < 0){
			perror("accept");
        	exit(EXIT_FAILURE);
		}
		// printf("Reading from client\n");

		std::string json;
		char buffer[BUFF_SIZE] = {0};
		
		size_t length;
		read( client_fd , &length, sizeof(length));

		int numread;
		while(1) {
			if ((numread = read( client_fd , buffer, BUFF_SIZE - 1)) == -1) {
				cout << "read error" << endl;
				perror("read");
				exit(1);
			}
			length -= numread;
		    buffer[numread] = '\0';
			json += buffer;

		    if (length == 0)
				break;
		}

		// cout << "*******************Snippet JSON*****************" << endl;
		// cout << json << endl;
		// cout << "#json len: " << strlen(json.c_str()) << endl;
		// cout << "#read_size: " << length << endl;
		// cout << "************************************************" << endl;
		
		Snippet parsedSnippet(json.c_str());
		// cout << json.c_str() << endl;
		
		EnQueueScan(parsedSnippet);

		close(client_fd);
	}

	close(server_fd);
	
}

void Input::EnQueueScan(Snippet parsedSnippet_){
	// cout << "[Put Snippet to Scan Queue]" << endl;
	ScanQueue.push_work(parsedSnippet_);
}
