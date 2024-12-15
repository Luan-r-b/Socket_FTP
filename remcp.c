#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <netinet/in.h>

#define BUFFER_SIZE 255

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

void send_file(int socket_fd, const char *file_path);
void relocate_file(int socket_fd, const char *file_path);
int connect_to_server(const char *ip);
Connect_Message parse_input(char *argv[]);
ServerState server_confirm(Connect_Message connect, int socket_fd);

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Uso: %s <arquivo origem> <pasta destino>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  //Structure to send
  Connect_Message connect = parse_input(argv);
  //Conecta ao servidor
  int socket_fd = connect_to_server(connect.ip);
  printf("Connected to server, socket_fd = %d\n", socket_fd);

  ServerState server_state = server_confirm(connect, socket_fd);
  if (server_state.server_status == 1)
    if (strcmp(connect.type, "SEND") == 0) send_file(socket_fd, connect.origin_path);
    if (strcmp(connect.type, "RELOCATE") == 0) relocate_file(socket_fd, connect.origin_path);
}

ServerState server_confirm(Connect_Message connect, int socket_fd) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    // Serialize Connect_Message
    int bytes_written = snprintf(buffer, BUFFER_SIZE, "%s|%s|%s|%s", 
                               connect.ip, connect.type, connect.origin_path, connect.target_path);

    if (bytes_written >= BUFFER_SIZE) {
        fprintf(stderr, "Connect_Message too large to fit in buffer.\n");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // Send the message
    if (send(socket_fd, buffer, strlen(buffer), 0) == -1) {
        perror("Error sending Connect_Message");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // Receive ServerState
    ServerState network_state;
    ServerState host_state;
    
    int bytes_received = recv(socket_fd, &network_state, sizeof(ServerState), 0);
    if (bytes_received != sizeof(ServerState)) {
        perror("Error receiving ServerState");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // Convert from network byte order to host byte order
    host_state.server_status = ntohl(network_state.server_status);
    host_state.back_off = ntohl(network_state.back_off);
    host_state.bucket_size = ntohl(network_state.bucket_size);

    printf("ServerState received:\n");
    printf("  server_status: %d\n", host_state.server_status);
    printf("  back_off: %ld\n", host_state.back_off);
    printf("  bucket_size: %u\n", host_state.bucket_size);

    return host_state;
}


Connect_Message parse_input(char *argv[]){
  Connect_Message connect;
  // Parse argumentos (arquivo, IP e destino)
  memset(&connect, 0, sizeof(Connect_Message));

  if (strchr(argv[1], ':')) {
    // Padrão <IP>:<arquivo_path> <destino_path>
    //RELOCATE
    sscanf(argv[1], "%[^:]:%s", connect.ip, connect.origin_path);
    strcpy(connect.target_path, argv[2]);
    strcpy(connect.type, "RELOCATE");
  } else if (strchr(argv[2], ':')) {
    // Padrão <arquivo_path> <IP>:<destino_path>
    //SEND
    strcpy(connect.origin_path, argv[1]);
    sscanf(argv[2], "%[^:]:%s", connect.ip, connect.target_path);
    strcpy(connect.type, "SEND");
  } else {
    fprintf(stderr, "Erro: ao menos um argumento deve estar no formato <IP>:<path>.\n");
    exit(EXIT_FAILURE);
  }

  printf("Parsed input:\n");
  printf("  origin_path: %s\n", connect.origin_path);
  printf("  ip: %s\n", connect.ip);
  printf("  target_path: %s\n", connect.target_path);
  printf("  type: %s\n", connect.type);

  return connect;
}

int connect_to_server(const char *ip) {
      // Configuração do socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
      perror("Erro ao criar socket");
      exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9734);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
      perror("Endereço IP inválido");
      close(socket_fd);
      exit(EXIT_FAILURE);
    }

    // Conexão com o servidor
    int len = sizeof(server_addr); 
    int result = connect(socket_fd, (struct sockaddr *)&server_addr, len);
    if (result == -1) {
      perror("Erro ao conectar");
      close(socket_fd);
      exit(EXIT_FAILURE);
    }

    return socket_fd;
}

void send_file(int socket_fd, const char *file_path) {
  printf("Iniciando transferencia do arquivo \n");
  FILE *file = fopen(file_path, "rb");
  if (!file) {
    perror("Erro ao abrir arquivo");
    return;
  }

  char buffer[BUFFER_SIZE];
  size_t bytes_read;

  while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
    // printf("Dentro do while \n");
    printf("Buffer: %s \n", buffer);
    int sent = send(socket_fd, buffer, bytes_read, 0);
    if(sent < 0) {
      perror("Error sending file");
      close(socket_fd);
      fclose(file);
      break;
    } 
  }

  fclose(file);
}

void relocate_file(int socket_fd, const char *file_path){
  //Pass
  return;
}