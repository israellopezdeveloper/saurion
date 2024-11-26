#include <arpa/inet.h>
#include <cstdlib>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h> // Para memoria compartida
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "low_saurion.h"

std::vector<pid_t> clients;
int numClients = 0;
std::atomic<bool>
    keepRunning (true); // Indicador para terminar el bucle de lectura

// Variables globales compartidas para el envío de mensajes
char *globalMessage;
int *globalMessageCount;
int *globalMessageDelay;
int sockfd = 0;

// Manejador de la señal para finalizar el bucle de lectura
void
signalHandler (int signum)
{
  if (signum == SIGUSR1)
    {
      keepRunning = false; // Termina el bucle del cliente
    }
}

uint64_t
htonll (uint64_t value)
{
  int num = 42;
  if (*(char *)&num == 42)
    { // Little endian check
      uint32_t high_part = htonl ((uint32_t)(value >> 32));
      uint32_t low_part = htonl ((uint32_t)(value & 0xFFFFFFFFLL));
      return ((uint64_t)low_part << 32) | high_part;
    }
  else
    { // Already big endian
      return value;
    }
}

uint64_t
ntohll (uint64_t value)
{
  int num = 42;
  if (*(char *)&num == 42)
    { // Little endian check
      uint32_t high_part = ntohl ((uint32_t)(value >> 32));
      uint32_t low_part = ntohl ((uint32_t)(value & 0xFFFFFFFFLL));
      return ((uint64_t)low_part << 32) | high_part;
    }
  else
    { // Already big endian
      return value;
    }
}

// Manejador de la señal para enviar mensajes
void
sendMessagesHandler (int signum)
{
  if (signum == SIGUSR2)
    {
      // Enviar los mensajes
      struct timespec tim;
      tim.tv_sec = 0;
      tim.tv_nsec = *globalMessageDelay * 1000L;
      for (int i = 0; i < *globalMessageCount; ++i)
        {
          // Crear el mensaje
          auto length = strlen (globalMessage);

          // 2. Crear un buffer que contendrá el mensaje completo
          uint64_t msg_len = strlen (globalMessage) + sizeof (length) + 1;

          // 3. Crear un buffer para almacenar todo el mensaje
          auto *buffer = new char[msg_len];
          if (buffer == nullptr)
            {
              perror ("Error en malloc");
              exit (EXIT_FAILURE);
            }

          uint64_t send_length = htonll (length);

          // 4. Copiar la longitud del mensaje (entero de 64 bits) al buffer
          memcpy (buffer, &send_length, sizeof (length));

          // 5. Copiar el mensaje de texto al buffer
          memcpy (buffer + sizeof (length), globalMessage, length);

          // 6. Agregar el bit a 0 al final
          buffer[msg_len - 1] = 0;

          // Enviar mensaje al servidor
          if (int64_t sent = send (sockfd, buffer, msg_len, 0); sent < 0)
            {
              delete[] buffer;
              break;
            }
          delete[] buffer;

          // Esperar el delay antes de enviar el siguiente mensaje
          nanosleep (&tim, nullptr); // Delay en milisegundos
        }
    }
}

// Función para hacer el socket no bloqueante
void
makeSocketNonBlocking ()
{
  int flags = fcntl (sockfd, F_GETFL, 0);
  if (flags == -1)
    {
      std::cerr << "Error obteniendo flags del socket: " << strerror (errno)
                << std::endl;
      exit (1);
    }

  if (fcntl (sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
      std::cerr << "Error configurando el socket como no bloqueante: "
                << strerror (errno) << std::endl;
      exit (1);
    }
}

void
parseMessages (char *buffer, int64_t bytes_read, std::ofstream &logStream)
{
  int64_t offset = 0;

  while ((bytes_read > 0)
         && (offset + sizeof (uint64_t) <= static_cast<uint64_t> (bytes_read)))
    {
      // Leer el entero de 64 bits (8 bytes) que indica la longitud del mensaje
      uint64_t msg_len;
      memcpy (&msg_len, buffer + offset, sizeof (msg_len));
      offset += sizeof (msg_len);
      msg_len = ntohll (msg_len);

      // Asegurarse de que tenemos suficientes bytes para el mensaje completo
      if (offset + msg_len + 1 > static_cast<uint64_t> (bytes_read))
        {
          return;
        }

      // Leer el mensaje
      auto *msg = new char[msg_len + 1];
      memcpy (msg, (char *)(buffer + offset), msg_len);
      msg[msg_len] = '\0'; // Agregar terminador de cadena
      offset += msg_len;

      // Leer el byte final (que debe ser 0)
      char end_byte = buffer[offset];
      offset++;

      if (end_byte != 0)
        {
          delete[] msg;
          return;
        }

      // Imprimir el mensaje
      logStream.write (msg, msg_len);
      delete[] msg;
    }
}

// Crear un cliente usando fork()
void
createClient (int clientId)
{
  pid_t pid = fork ();
  if (pid != 0)
    {
      clients.push_back (pid);
      return;
    }
  signal (SIGUSR1,
          signalHandler); // Asignar el manejador de la señal para finalizar
  signal (SIGUSR2,
          sendMessagesHandler); // Manejador de la señal para enviar mensajes

  std::ostringstream tempFilePath;
  tempFilePath << "/tmp/saurion_sender." << clientId << ".log";
  std::ofstream logStream (tempFilePath.str (), std::ios::app);

  if (!logStream.is_open ())
    {
      std::cerr << "Error al abrir el archivo log para el cliente " << clientId
                << std::endl;
      exit (1);
    }

  // Crear socket y conectarse al servidor
  sockfd = socket (AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    {
      std::cerr << "Error creando socket" << std::endl;
      exit (1);
    }

  struct sockaddr_in serv_addr;
  memset (&serv_addr, 0, sizeof (serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons (8080);
  inet_pton (AF_INET, "localhost",
             &serv_addr.sin_addr); // Asumiendo localhost para la conexión

  if (connect (sockfd, (struct sockaddr *)&serv_addr, sizeof (serv_addr)) < 0)
    {
      perror ("Error conectando al servidor");
      exit (1);
    }

  // Hacer el socket no bloqueante
  makeSocketNonBlocking ();

  char buffer[8192];
  memset (buffer, 0, 8192);

  // Leer datos del servidor en modo no bloqueante
  std::vector<uint8_t> accumulatedBuffer;
  ssize_t total_len = 0;

  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 1000000L;
  while (keepRunning)
    {
      int dataAvailable = 3;

      total_len = 0;
      while (dataAvailable > 0)
        {
          ssize_t len = read (sockfd, buffer, sizeof (buffer));

          if (len > 0)
            {
              // Acumular los datos leídos en el buffer
              accumulatedBuffer.insert (accumulatedBuffer.end (), buffer,
                                        buffer + len);
              total_len += len;
              continue;
            }
          if (len == 0)
            {
              keepRunning = false;
              break;
            }
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
              nanosleep (&ts, nullptr);
              --dataAvailable;
              continue;
            }
          // Error de lectura, pero no debido a que no haya datos
          // disponibles
          std::cerr << "Error leyendo del socket: " << strerror (errno)
                    << std::endl;
          keepRunning = false;
          break;
        }

      // Procesar todos los datos acumulados en una sola llamada a
      // parseMessages
      if (!accumulatedBuffer.empty ())
        {
          parseMessages ((char *)accumulatedBuffer.data (), total_len,
                         logStream);

          // Limpiar el buffer acumulado después de procesar los mensajes
          accumulatedBuffer.clear ();
        }
    }
  logStream.close ();
  close (sockfd);
  exit (0); // El proceso hijo termina aquí
}

// Conectar múltiples clientes
void
connectClients (int n)
{
  for (int i = 0; i < n; ++i)
    {
      createClient (numClients + i + 1);
    }
  numClients += n;
}

// Función para que todos los clientes envíen n mensajes con retraso
void
sendMessages (int n, const std::string &msg, int delay)
{
  // Actualizar los valores en la memoria compartida
  strcpy (globalMessage, msg.c_str ());
  *globalMessageCount = n;
  *globalMessageDelay = delay;

  // Enviar la señal SIGUSR2 a todos los clientes para que empiecen a enviar
  // los mensajes
  for (pid_t pid : clients)
    {
      kill (pid, SIGUSR2);
    }
}

// Desconectar clientes enviándoles la señal SIGUSR1 y esperar que terminen
void
disconnectClients ()
{
  for (pid_t pid : clients)
    {
      // Enviar la señal SIGUSR1 al cliente para que termine
      kill (pid, SIGUSR1);

      // Esperar que el proceso hijo termine
      int status;
      waitpid (pid, &status,
               0); // Espera bloqueante hasta que el proceso hijo termine
      if (!WIFEXITED (status))
        {
          std::cout << "Cliente " << pid << " terminó por señal "
                    << WTERMSIG (status) << std::endl;
        }
    }
  clients.clear ();
  numClients = 0;
}

// Cerrar la aplicación
__attribute__ ((noreturn)) void
closeApplication ()
{
  disconnectClients ();
  exit (0);
}

// Manejar comandos recibidos
void
handleCommand (const std::string &command)
{
  std::string cmd;
  unsigned int n = 0;
  std::string msg;
  unsigned int delay = 0;

  // Crear un stream a partir del string
  std::istringstream ss (command);
  std::string token;

  // Extraer los valores del string usando getline y ';' como delimitador
  std::getline (ss, cmd, ';');

  if (cmd == "connect")
    {
      std::getline (ss, token, ';');
      n = std::stoi (token);
      connectClients (n);
    }
  else if (cmd == "send")
    {
      std::getline (ss, token, ';');
      n = std::stoi (token);
      std::getline (ss, msg, ';');
      std::getline (ss, token, ';');
      delay = std::stoi (token);
      sendMessages (n, msg, delay);
    }
  else if (cmd == "disconnect")
    {
      disconnectClients ();
    }
  else if (cmd == "close")
    {
      closeApplication ();
    }
  else
    {
      std::cout << "Comando desconocido: " << cmd << std::endl;
    }
}

// Leer comandos desde un pipe
void
readPipe (const std::string &pipePath)
{
  int fd = open (pipePath.c_str (), O_RDONLY);
  if (fd < 0)
    {
      std::cerr << "Error al abrir el pipe: " << pipePath << std::endl;
      return;
    }

  char buffer[4096];
  std::string commandBuffer;
  ssize_t bytesRead = 0;
  while ((bytesRead = read (fd, buffer, sizeof (buffer))) > 0L)
    {
      commandBuffer.append (buffer, bytesRead);
      if (commandBuffer.contains ('\n'))
        {
          handleCommand (commandBuffer);
          commandBuffer.clear ();
        }
    }

  close (fd);
}
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
__attribute__ ((no_sanitize ("thread")))
#endif
#endif
void
global_map ()
{
  // Crear la memoria compartida para las variables
  globalMessage = static_cast<char *> (
      mmap (nullptr, 10 * CHUNK_SZ * sizeof (char), PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_ANONYMOUS, -1, 0));
  globalMessageCount = static_cast<int *> (
      mmap (nullptr, sizeof (int), PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_ANONYMOUS, -1, 0));
  globalMessageDelay = static_cast<int *> (
      mmap (nullptr, sizeof (int), PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_ANONYMOUS, -1, 0));
}

int
main (int argc, char *argv[])
{
  if (argc < 3)
    {
      std::cerr << "Uso: " << argv[0] << " -p <pipe_path>" << std::endl;
      return 1;
    }

  std::string pipePath;

  for (int i = 1; i < argc; ++i)
    {
      if (std::string (argv[i]) == "-p" && i + 1 < argc)
        {
          pipePath = argv[i + 1];
          break;
        }
    }

  if (pipePath.empty ())
    {
      return 1;
    }

  global_map ();
  mkfifo (pipePath.c_str (), 0666); // Crear el pipe si no existe

  // Leer comandos desde el pipe
  while (true)
    {
      readPipe (pipePath);
      sleep (1); // Esperar antes de volver a intentar leer
    }

  return 0;
}
