#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>

#define BUFFER_SIZE 255

typedef struct {
  int server_status;
  time_t back_off;
  unsigned int bucket_size;
} ServerState;

void send_file(int socket_fd, const char *file_path);
int connect_to_server(const char *ip, const char *origem_path, const char *destino_path, const char *tipo);

int main(int argc, char *argv[]) {
  
  if (argc != 3) {
    fprintf(stderr, "Uso: %s <arquivo origem> <pasta destino>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // Parse argumentos (arquivo, IP e destino)
  char ip[INET_ADDRSTRLEN] = "";
  char origem_path[100] = "";
  char destino_path[100] = "";
  char tipo[16] = "";

  if (strchr(argv[1], ':')) {
    // Padrão <IP>:<arquivo_path> <destino_path>
    sscanf(argv[1], "%[^:]:%s", ip, origem_path);
    strcpy(destino_path, argv[2]);
    strcpy(tipo, "RELOCATE");
  } else if (strchr(argv[2], ':')) {
    // Padrão <arquivo_path> <IP>:<destino_path>
    strcpy(origem_path, argv[1]);
    sscanf(argv[2], "%[^:]:%s", ip, destino_path);
    strcpy(tipo, "SEND");
  } else {
    fprintf(stderr, "Erro: ao menos um argumento deve estar no formato <IP>:<path>.\n");
    exit(EXIT_FAILURE);
  }

  printf("file_path = %s\n", origem_path);
  printf("ip = %s\n", ip);
  printf("destino_path = %s\n", destino_path);
  printf("tipo = %s\n", tipo);


  //Conecta ao servidor
  int socket_fd = connect_to_server(ip, origem_path, destino_path, tipo);
  printf("Connected to server, socket_fd = %d\n", socket_fd);

  while (1) {
    // Envia comando e inicia transferência
    char command[BUFFER_SIZE];
    int file_size = 0;
    if (strcmp(tipo, "SEND") == 0) {
      struct stat st;
      if (stat(origem_path, &st) == 0) {
        file_size = st.st_size;
      } else {
        perror("Could not retrieve file size");
      }
    }
    snprintf(command, BUFFER_SIZE, "%s %s %s %d\n", tipo, origem_path, destino_path, file_size);
    printf("ENVIANDO = %s\n", command);
    send(socket_fd, command, strlen(command), 0); //Manda tipo, nome do arquivo e destino
    if(strcmp(tipo, "SEND") == 0){
      send_file(socket_fd, origem_path);
    }

    // Try to receive either the reversed message or an error message
    ServerState serve_state;
    int bytes_received = recv(socket_fd, &error_message, sizeof(ErrorMessage), 0);
    if (bytes_received < 0) {
      perror("Falha ao receber message or error");
    } else if (bytes_received == sizeof(ErrorMessage)) {
      printf("Erro recebido do servidor: %s\n", error_message.message);
      printf("Overwhelmed Bytes: %d, Byte rate: %d\n",
        error_message.overwhelmed_bytes, error_message.byte_rate);

      // Calculate wait time based on overwhelmed bytes and byte rate
      float wait_time = (float)error_message.overwhelmed_bytes / error_message.byte_rate;
      printf("Waiting for %.2f seconds before retrying...\n", wait_time);
      sleep((int)wait_time + 1); // Sleep for the calculated wait time (rounded up)

      close(socket_fd);
      continue;
    } else {
      // Assuming it received a regular string message
      char buffer_response[256] = {0};
      memcpy(buffer_response, &error_message, bytes_received);
      printf("Mensagem recebida do servidor: %s\n", buffer_response);
      close(socket_fd);
      break;
    }
    close(socket_fd);
  }

  exit(0);
}

int connect_to_server(const char *ip, const char *origem_path, const char *destino_path, const char *tipo) {
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
        printf("Read %zu bytes from file: %.*s\n", bytes_read, (int)bytes_read, buffer);
        int sent = send(socket_fd, buffer, bytes_read, 0);
        if (sent < 0) {
            perror("Error sending file");
            close(socket_fd);
            fclose(file);
            break;
        }
        if (sent != bytes_read) {
            fprintf(stderr, "Mismatch in bytes sent: expected %zu, sent %d\n", bytes_read, sent);
        }
    }

    if (ferror(file)) {
        perror("Error reading file");
    }

    fclose(file);
    if (shutdown(socket_fd, SHUT_WR) < 0) {
        perror("Error shutting down socket write channel");
    }
}
