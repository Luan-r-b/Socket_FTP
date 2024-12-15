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
  int server_status;
  time_t back_off;
  unsigned int bucket_size;
} ServerState;

int processed_bytes = 0;
time_t start_time;
int overwhelmed_bytes_second = 0;

typedef struct {
    int overwhelmed_bytes;
    int byte_rate;
    char message[256];
} ErrorMessage;

void handle_client(int client_socket, char *tipo, char *origem_path, char *file_path);
void daemonize();
void start_server();

int main() {
  //daemonize();
  start_server();
  return 0;
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

void client_connect(){
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int client_fd;
  int len, result;

  if (server_fd == -1) {
    perror("Erro ao criar socket");
    exit(EXIT_FAILURE);
  }

}

void start_server() {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int client_fd;
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
  start_time = time(NULL);

  while (1) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);

    if (client_fd < 0) {
      perror("Erro ao aceitar conexão");
      continue;
    }

    time_t current_time = time(NULL);
    if (difftime(current_time, start_time) >= 1) {
      // Reset the counter every second
      processed_bytes = 0;
      overwhelmed_bytes_second = 0;
      start_time = current_time;
    }

    char buffer[256] = {0};
    char tipo[16], origem_path[100], file_path[100];
    int size;
    int recv_bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (recv_bytes < 0) {
        perror("Error receiving command from client");
        close(client_fd);
        continue;
    }
    buffer[recv_bytes] = '\0'; // Ensure null-termination
    printf("Received command: %s\n", buffer);
    sscanf(buffer, "%s %s %s %d", tipo, origem_path, file_path, &size);
    printf("Parsed command: tipo=%s, origem_path=%s, file_path=%s, size=%d\n", tipo, origem_path, file_path, size);
    printf("tipo = %s\n", tipo);
    printf("origem_path = %s\n", origem_path);
    printf("file_path = %s\n", file_path);
    printf("size = %d\n", size);
    int bytes_received = size;

    if (bytes_received < 0) {
      close(client_fd);
      continue;
    }

    if (processed_bytes + bytes_received > MAX_BYTES_PER_SECOND) {
      // Calculate suggested wait time based on the byte size of the message
      overwhelmed_bytes_second+=bytes_received;
            
      ErrorMessage error_message;
      error_message.overwhelmed_bytes=overwhelmed_bytes_second;
      error_message.byte_rate=MAX_BYTES_PER_SECOND;
      snprintf(error_message.message, sizeof(error_message.message), "Server busy, try again later. Overwhelmed bytes: %d. Byte_rate: %d BYTES/SECOND ", error_message.overwhelmed_bytes,error_message.byte_rate);

      send(client_fd, &error_message, sizeof(ErrorMessage), 0);
      close(client_fd);
      continue;
    }
    else{
      processed_bytes += bytes_received;
      handle_client(client_fd, tipo, origem_path, file_path);
      char response[250] = "Operação executada";
      send(client_fd, response, strlen(response), 0);
      close(client_fd);
    }
  }

  close(server_fd);
}

void handle_client(int client_socket, char *tipo, char *origem_path, char *file_path) {
  FILE *file = fopen(file_path, "wb");
  if (!file) {
      perror("Erro ao criar arquivo");
      printf("Failed to open file: %s\n", file_path);
      return;
  }
  printf("Successfully opened file for writing: %s\n", file_path);
  if (strcmp(tipo, "SEND") == 0) {
      char buffer[BUFFER_SIZE];
      ssize_t bytes_received;
      while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
          printf("Received %ld bytes from client: %.*s\n", bytes_received, (int)bytes_received, buffer);
          if (fwrite(buffer, 1, bytes_received, file) != (size_t)bytes_received) {
              perror("Error writing to file");
              printf("Failed to write all received bytes to file\n");
              break; // Exit loop on write error
          } else {
              printf("Successfully wrote %ld bytes to file\n", bytes_received);
          }
      }
      if (bytes_received < 0) {
          perror("Error receiving data from client");
      }
  }
  else if(strcmp(tipo, "RELOCATE") == 0){
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
}