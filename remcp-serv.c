#include <sys/socket.h>
#include <string.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define BUFFER_SIZE 128
#define MAX_CLIENTS 10
#define MAX_BYTES_PER_SECOND 100 

typedef struct {
  int server_status; // Backoff == 0 | Send == 1 | Transefer == 2 
  time_t back_off;
  unsigned int bucket_size;
} ServerState;

typedef struct {
  char ip[INET_ADDRSTRLEN];
  char type[16];
  char origin_path[100];
  char target_path[100];  
} Connect_Message;

time_t start_time;

void handle_client(int client_socket, char *tipo, char *origem_path, char *file_path);
void daemonize();
int start_server();
int listen_client(int server_fd);
Connect_Message receive_connect_message(int client_fd);
int receive_file(int client_fd, Connect_Message connect);
void send_status(int client_socket, ServerState status);

int main() {
  //daemonize();
  int server_fd = start_server();
// In main function, modify the string comparison:
  while (1) {
      int client_fd = listen_client(server_fd);
      Connect_Message connect_message = receive_connect_message(client_fd);
      printf("Connect message received in main\n");
      
      // Correct string comparison
      if (strcmp(connect_message.type, "SEND") == 0) {
          handle_client(client_fd, connect_message.type, connect_message.origin_path, connect_message.target_path);
      }
  }
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int start_server(){
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int len, result;

  if (server_fd == -1) {
    perror("Erro ao criar socket");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in server_addr = {0};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  server_addr.sin_port = htons(9734);

  len = sizeof(server_addr); 
  result = bind(server_fd, (struct sockaddr *)&server_addr, len);
  if (result == -1) {
    perror("Erro ao fazer bind");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, MAX_CLIENTS) < 0) {
    perror("Erro ao fazer listen");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  printf("Servidor escutando na porta 9734...\n");
  return server_fd;
}

int listen_client(int server_fd){
  int client_fd;
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);

  if (client_fd < 0) {
    perror("Erro ao aceitar conexão");
  }
  return client_fd;
}

Connect_Message receive_connect_message(int client_fd) {
    char buffer[BUFFER_SIZE] = {0};
    Connect_Message connect_msg = {0};
    
    // Receive the message
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        perror("Error receiving Connect_Message");
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    buffer[bytes_received] = '\0';

    // Parse the message
    sscanf(buffer, "%[^|]|%[^|]|%[^|]|%[^|]",
           connect_msg.ip, connect_msg.type, connect_msg.origin_path, connect_msg.target_path);

    printf("Received Connect_Message:\n");
    printf("  ip: %s\n", connect_msg.ip);
    printf("  type: %s\n", connect_msg.type);
    printf("  origin_path: %s\n", connect_msg.origin_path);
    printf("  target_path: %s\n", connect_msg.target_path);

    // Create and serialize the response
    ServerState server_state = {0};
    
    // Check message type and set appropriate response
    if (strcmp(connect_msg.type, "SEND") == 0 || strcmp(connect_msg.type, "RELOCATE") == 0) {
        server_state.server_status = 1;
        server_state.bucket_size = MAX_BYTES_PER_SECOND;
        server_state.back_off = 0;
    } else {
        server_state.server_status = 0;
        server_state.bucket_size = 0;
        server_state.back_off = 5;
    }

    // Convert ServerState to network byte order before sending
    ServerState network_state = {
        .server_status = htonl(server_state.server_status),
        .back_off = htonl(server_state.back_off),
        .bucket_size = htonl(server_state.bucket_size)
    };

    // Send the response
    if (send(client_fd, &network_state, sizeof(ServerState), 0) == -1) {
        perror("Error sending ServerState");
        exit(EXIT_FAILURE);
    }

    return connect_msg;
}


void handle_client(int client_socket, char *tipo, char *origem_path, char *file_path) {
  printf("entering handle_client");
  FILE *file = fopen(file_path, "wb");
  if (!file) {
    perror("Erro ao criar arquivo");
    return;
  }

  if(strcmp(tipo, "SEND") == 0){
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
      printf("buffer: %s \n", buffer);
      fwrite(buffer, 1, bytes_received, file);
    }
  }else if(strcmp(tipo, "RELOCATE") == 0){
    char buffer_relocate[4096];
    size_t bytes_read;

    FILE *source_file = fopen(origem_path, "rb");
    if (source_file == NULL) {
      perror("Erro ao abrir arquivo");
    }

    while ((bytes_read = fread(buffer_relocate, 1, sizeof(buffer_relocate), source_file)) > 0) {
      if (fwrite(buffer_relocate, 1, bytes_read, file) != bytes_read) {
        perror("Error writing to destination file");
        fclose(source_file);
      }
    }
  }

  fclose(file);
  close(client_socket);
}

int receive_file(int client_fd, Connect_Message connect) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    size_t total_bytes_received = 0;

    // Open the target file for writing
    FILE *file = fopen(connect.target_path, "wb");
    if (!file) {
        perror("Error opening file for writing");
        return -1;
    }

    printf("Receiving file and saving to: %s\n", connect.target_path);

    // Receive data in chunks and write to the file
    while ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        if (fwrite(buffer, 1, bytes_received, file) != (size_t)bytes_received) {
            perror("Error writing to file");
            fclose(file);
            return -1;
        }
        total_bytes_received += bytes_received;
    }

    // Handle errors or end of transmission
    if (bytes_received < 0) {
        perror("Error receiving data from client");
        fclose(file);
        return -1;
    }

    fclose(file);
    printf("File transfer completed successfully. Total bytes received: %zu\n", total_bytes_received);
    return 0;
}
