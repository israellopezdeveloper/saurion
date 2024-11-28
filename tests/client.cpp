#include <arpa/inet.h>
#include <cstdlib>
#include <fcntl.h>
#include <memory>
#include <string.h>
#include <sys/mman.h>
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
std::atomic keepRunning (true);

char *globalMessage;
int *globalMessageCount;
int *globalMessageDelay;
int sockfd = 0;

void
signalHandler (int signum)
{
  if (signum == SIGUSR1)
    {
      keepRunning = false;
    }
}

uint64_t
htonll (uint64_t value)
{
  int num = 42;
  if (*(char *)&num == 42)
    {
      uint32_t high_part = htonl ((uint32_t)(value >> 32));
      uint32_t low_part = htonl ((uint32_t)(value & 0xFFFFFFFFLL));
      return ((uint64_t)low_part << 32) | high_part;
    }
  else
    {
      return value;
    }
}

uint64_t
ntohll (uint64_t value)
{
  int num = 42;
  if (*(char *)&num == 42)
    {
      uint32_t high_part = ntohl ((uint32_t)(value >> 32));
      uint32_t low_part = ntohl ((uint32_t)(value & 0xFFFFFFFFLL));
      return ((uint64_t)low_part << 32) | high_part;
    }
  else
    {
      return value;
    }
}

void
sendMessagesHandler (int signum)
{
  if (signum == SIGUSR2)
    {
      struct timespec tim;
      tim.tv_sec = 0;
      tim.tv_nsec = *globalMessageDelay * 1000L;
      for (int i = 0; i < *globalMessageCount; ++i)
        {
          auto length = strlen (globalMessage);

          uint64_t msg_len = strlen (globalMessage) + sizeof (length) + 1;

          auto buffer = std::make_unique<char[]> (msg_len);

          uint64_t send_length = htonll (length);

          std::memcpy (buffer.get (), &send_length, sizeof (length));
          std::memcpy (buffer.get () + sizeof (length), globalMessage, length);

          buffer[msg_len - 1] = 0;

          if (int64_t sent = send (sockfd, buffer.get (), msg_len, 0);
              sent < 0)
            {
              break;
            }

          nanosleep (&tim, nullptr);
        }
    }
}

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
parseMessages (const char *const buffer, const int64_t bytes_read,
               std::ofstream &logStream)
{
  int64_t offset = 0;

  while ((bytes_read > 0)
         && (offset + sizeof (uint64_t) <= static_cast<uint64_t> (bytes_read)))
    {

      uint64_t msg_len;
      memcpy (&msg_len, buffer + offset, sizeof (msg_len));
      offset += sizeof (msg_len);
      msg_len = ntohll (msg_len);

      if (offset + msg_len + 1 > static_cast<uint64_t> (bytes_read))
        {
          return;
        }

      auto msg = std::make_unique<char[]> (msg_len + 1);
      std::memcpy (msg.get (), buffer + offset, msg_len);
      msg[msg_len] = '\0';
      offset += msg_len;

      char end_byte = buffer[offset];
      offset++;

      if (end_byte != 0)
        {
          return;
        }

      logStream.write (msg.get (), msg_len);
    }
}

void
createClient (int clientId)
{

  if (pid_t pid = fork (); pid != 0)
    {
      clients.push_back (pid);
      return;
    }
  signal (SIGUSR1, signalHandler);
  signal (SIGUSR2, sendMessagesHandler);

  std::ostringstream tempFilePath;
  tempFilePath << "/tmp/saurion_sender." << clientId << ".log";
  std::ofstream logStream (tempFilePath.str (), std::ios::app);

  if (!logStream.is_open ())
    {
      std::cerr << "Error al abrir el archivo log para el cliente " << clientId
                << std::endl;
      exit (1);
    }

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
  inet_pton (AF_INET, "localhost", &serv_addr.sin_addr);

  if (connect (sockfd, (struct sockaddr *)&serv_addr, sizeof (serv_addr)) < 0)
    {
      perror ("Error conectando al servidor");
      exit (1);
    }

  makeSocketNonBlocking ();

  char buffer[8192];
  memset (buffer, 0, 8192);

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

          std::cerr << "Error leyendo del socket: " << strerror (errno)
                    << std::endl;
          keepRunning = false;
          break;
        }

      if (!accumulatedBuffer.empty ())
        {
          parseMessages ((char *)accumulatedBuffer.data (), total_len,
                         logStream);

          accumulatedBuffer.clear ();
        }
    }
  logStream.close ();
  close (sockfd);
  exit (0);
}

void
connectClients (int n)
{
  for (int i = 0; i < n; ++i)
    {
      createClient (numClients + i + 1);
    }
  numClients += n;
}

void
sendMessages (int n, const std::string &msg, int delay)
{

  strcpy (globalMessage, msg.c_str ());
  *globalMessageCount = n;
  *globalMessageDelay = delay;

  for (pid_t pid : clients)
    {
      kill (pid, SIGUSR2);
    }
}

void
disconnectClients ()
{
  for (pid_t pid : clients)
    {

      kill (pid, SIGUSR1);

      int status;
      waitpid (pid, &status, 0);
      if (!WIFEXITED (status))
        {
          std::cout << "Cliente " << pid << " terminó por señal "
                    << WTERMSIG (status) << std::endl;
        }
    }
  clients.clear ();
  numClients = 0;
}

__attribute__ ((noreturn)) void
closeApplication ()
{
  disconnectClients ();
  exit (0);
}

void
handleCommand (const std::string &command)
{
  std::string cmd;
  unsigned int n = 0;
  std::string msg;
  unsigned int delay = 0;

  std::istringstream ss (command);
  std::string token;

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
  mkfifo (pipePath.c_str (), 0666);

  while (true)
    {
      readPipe (pipePath);
      sleep (1);
    }

  return 0;
}
