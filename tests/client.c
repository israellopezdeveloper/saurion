#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Global variables
int numClients = 0;
int *clients = NULL;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
char *pipePath = NULL;
volatile int stopClients = 0;     // Bandera para detener a los hilos de los clientes
pthread_t *clientThreads = NULL;  // Array para almacenar los hilos de los clientes

// Estructura para pasar los datos al hilo del cliente
typedef struct {
  int sockfd;
  FILE *logFile;
} ClientData;

// Error handling function
void error(const char *msg) {
  perror(msg);
  exit(1);
}

// Función que se ejecutará en el hilo para leer el socket

void *clientHandler(void *arg) {
  ClientData *clientData = (ClientData *)arg;
  int sockfd = clientData->sockfd;
  FILE *logFile = clientData->logFile;
  uint64_t expectedLength = 0;
  char buffer[1024];

  while (!stopClients) {
    printf("CLIENT: stopClients => %d\n", stopClients);
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    // Tiempo de espera de 1 segundo para la llamada select
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;

    int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

    if (activity < 0 && errno != EINTR) {
      perror("Error in select");
      break;
    } else if (activity > 0 && FD_ISSET(sockfd, &readfds)) {
      // Hay datos disponibles para leer
      int n = read(sockfd, buffer, sizeof(buffer));
      if (n < 0) {
        break;
      } else if (n == 0) {
        // Conexión cerrada
        break;
      }

      if (expectedLength == 0 && n >= 8) {
        memcpy(&expectedLength, buffer, 8);
        expectedLength = ntohl(expectedLength);
      }

      fprintf(logFile, "%s", buffer + 8);  // Escribir datos en el archivo de log
      fflush(logFile);                     // Asegurar que los datos se escriban inmediatamente
    }
  }

  fclose(logFile);
  close(sockfd);
  free(clientData);
  return NULL;
}

// Función para crear un cliente y conectarlo al servidor
int createClient(int clientId) {
  int sockfd;
  struct sockaddr_in server_addr;
  char tempFile[256];
  snprintf(tempFile, sizeof(tempFile), "/tmp/saurion_sender.%d.log", clientId);

  // Crear un archivo temporal para logs
  FILE *logFile = fopen(tempFile, "a");
  if (logFile == NULL) {
    perror("Error creating temporary file");
    return -1;
  }

  // Crear el socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error opening socket");
    fclose(logFile);
    return -1;
  }

  // Configurar la dirección del servidor
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(8080);
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  // Conectar al servidor
  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("Error connecting to server");
    fclose(logFile);
    close(sockfd);
    return -1;
  }

  // Preparar los datos para el hilo
  ClientData *clientData = (ClientData *)malloc(sizeof(ClientData));
  clientData->sockfd = sockfd;
  clientData->logFile = logFile;

  // Crear un nuevo hilo para manejar la lectura del socket
  pthread_t *clientThread = (pthread_t *)malloc(sizeof(pthread_t));
  if (pthread_create(clientThread, NULL, clientHandler, clientData) != 0) {
    perror("Error creating client thread");
    fclose(logFile);
    close(sockfd);
    free(clientData);
    return -1;
  }

  // Almacenar el hilo en el array de hilos
  clientThreads[clientId] = *clientThread;
  clients[clientId] = sockfd;

  // Detach el hilo para que se libere al terminar
  pthread_detach(*clientThread);

  return 0;
}

// Función para conectar múltiples clientes
void connectClients(int n) {
  pthread_mutex_lock(&clients_mutex);
  clients = realloc(clients, (numClients + n) * sizeof(int));
  clientThreads =
      realloc(clientThreads, (numClients + n) * sizeof(pthread_t));  // Redimensionar array de hilos

  for (int i = 0; i < n; i++) {
    createClient(numClients + i);
  }

  numClients += n;
  pthread_mutex_unlock(&clients_mutex);
}

// Función para enviar mensajes a los clientes
void sendMessages(int n, const char *msg, int delay) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < numClients; i++) {
    for (int j = 0; j < n; j++) {
      uint64_t length = strlen(msg);
      size_t size = sizeof(msg) + 9;
      char buffer[size];
      memset(buffer, 0, size);
      memcpy(buffer, &length, sizeof(length));
      strcpy(buffer + sizeof(length), msg);
      write(clients[i], buffer, size);
      usleep(delay * 1000);
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

// Función para desconectar a todos los clientes
void disconnectClients() {
  puts("CLIENTS: disconect");
  pthread_mutex_lock(&clients_mutex);

  // Establecer la bandera para detener los hilos
  stopClients = 1;

  // Esperar a que los hilos terminen
  for (int i = 0; i < numClients; i++) {
    printf("CLIENTS: waiting for disconect [%d]", clients[i]);
    if (clientThreads[i] > 0) {
      pthread_join(clientThreads[i], NULL);
    }
  }

  free(clients);
  free(clientThreads);
  clients = NULL;
  clientThreads = NULL;
  numClients = 0;

  stopClients = 0;
  pthread_mutex_unlock(&clients_mutex);
}

// Función para procesar comandos desde la tubería
void processCommand(char *command) {
  char *cmd = strtok(command, ";");
  if (strcmp(cmd, "connect") == 0) {
    int n = atoi(strtok(NULL, ";"));
    connectClients(n);
  } else if (strcmp(cmd, "send") == 0) {
    int count = atoi(strtok(NULL, ";"));
    char *msg = strtok(NULL, ";");
    int delay = atoi(strtok(NULL, ";"));
    sendMessages(count, msg, delay);
  } else if (strcmp(cmd, "disconnect") == 0) {
    disconnectClients();
  } else if (strcmp(cmd, "close") == 0) {
    disconnectClients();
    exit(0);
  } else {
    printf("Unknown command: %s\n", cmd);
  }
}

// Función para leer desde una tubería
void *readPipe(void *arg) {
  if (arg) {
    free(arg);
  }
  int fd;
  while ((fd = open(pipePath, O_RDONLY)) < 0) {
    perror("Error opening pipe");
    sleep(1);
  }

  char buffer[4096];
  while (1) {
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    if (bytesRead > 0) {
      buffer[bytesRead] = '\0';
      processCommand(buffer);
    } else if (bytesRead == 0) {
      close(fd);
      break;
    } else {
      perror("Error reading from pipe");
    }
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s -p <pipe_path>\n", argv[0]);
    exit(1);
  }

  // Parse command-line arguments
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
      pipePath = argv[i + 1];
      break;
    }
  }

  if (pipePath == NULL) {
    fprintf(stderr, "You must provide a pipe path with -p\n");
    exit(1);
  }

  // Crear un hilo para leer desde la tubería
  pthread_t pipeThread;
  if (pthread_create(&pipeThread, NULL, readPipe, NULL) != 0) {
    perror("Error creating pipe reading thread");
    exit(1);
  }

  // Esperar a que el hilo termine
  pthread_join(pipeThread, NULL);

  return 0;
}
