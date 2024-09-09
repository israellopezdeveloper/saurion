#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Global variables
int numClients = 0;
int *clients = NULL;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
char *pipePath = NULL;

// Error handling function
void error(const char *msg) {
  perror(msg);
  exit(1);
}

// Function to create a client and connect it to the server
int createClient(int clientId) {
  int sockfd;
  struct sockaddr_in server_addr;
  char tempFile[256];
  snprintf(tempFile, sizeof(tempFile), "/tmp/saurion_sender.%d.log", clientId);

  // Create a temporary file for logs
  FILE *logFile = fopen(tempFile, "a");
  if (logFile == NULL) {
    perror("Error creating temporary file");
    return -1;
  }

  // Create the socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error opening socket");
    fclose(logFile);
    return -1;
  }

  // Set up server address
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(8080);
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  // Connect to the server
  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("Error connecting to server");
    fclose(logFile);
    close(sockfd);
    return -1;
  }

  // Process incoming data from the server
  uint64_t expectedLength = 0;
  char buffer[1024];
  while (1) {
    int n = read(sockfd, buffer, sizeof(buffer));
    if (n < 0) {
      perror("Error reading from socket");
      break;
    } else if (n == 0) {
      // Connection closed
      break;
    }

    if (expectedLength == 0 && n >= 8) {
      memcpy(&expectedLength, buffer, 8);
      expectedLength = ntohl(expectedLength);
    }

    fprintf(logFile, "%s", buffer + 8); // Write data to the log file
    fflush(logFile);                    // Ensure immediate writing of data
  }

  fclose(logFile);
  close(sockfd);
  return 0;
}

// Function to connect multiple clients
void connectClients(int n) {
  pthread_mutex_lock(&clients_mutex);
  clients = realloc(clients, (numClients + n) * sizeof(int));
  for (int i = 0; i < n; i++) {
    int clientId = numClients + i + 1;
    clients[numClients + i] = createClient(clientId);
  }
  numClients += n;
  pthread_mutex_unlock(&clients_mutex);
}

// Function to send messages to clients
void sendMessages(int n, const char *msg, int delay) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < numClients; i++) {
    for (int j = 0; j < n; j++) {
      uint64_t length = strlen(msg);
      length = htonl(length);
      char buffer[1024];
      memcpy(buffer, &length, sizeof(length));
      strcpy(buffer + sizeof(length), msg);
      write(clients[i], buffer, sizeof(buffer));
      usleep(delay * 1000);
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

// Function to disconnect all clients
void disconnectClients() {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < numClients; i++) {
    close(clients[i]);
  }
  free(clients);
  clients = NULL;
  numClients = 0;
  pthread_mutex_unlock(&clients_mutex);
}

// Function to process commands from the pipe
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

// Function to read from a pipe
void *readPipe(void * /*unused*/) {
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

  // Create a thread to read from the pipe
  pthread_t pipeThread;
  if (pthread_create(&pipeThread, NULL, readPipe, NULL) != 0) {
    perror("Error creating pipe reading thread");
    exit(1);
  }

  // Wait for the thread to finish
  pthread_join(pipeThread, NULL);

  return 0;
}
