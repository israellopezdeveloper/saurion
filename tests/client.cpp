#include "config.h"

#include <arpa/inet.h> // for htonl, inet_pton, ntohl, htons
#include <atomic>      // for atomic
#include <cstdint>     // for uint64_t, uint32_t, int64_t, uint8_t
#include <cstring>     // for memcpy, strerror, memset, strlen, strcpy
#include <fcntl.h>     // for fcntl, open, F_GETFL, F_SETFL, O_NONBLOCK
#include <filesystem>  // for filesystem, setfill, setw
#include <fstream>     // for basic_ostream, operator<<, endl, basic...
#include <sys/mman.h>  // for mmap, MAP_ANONYMOUS, MAP_FAILED, MAP_S...
#include <sys/stat.h>  // for mkfifo
#include <sys/wait.h>  // for waitpid
#include <vector>      // for vector

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
endianess (uint64_t value, bool toNet)
{
  int num = 42;
  if (*(char *)&num == 42)
    {
      uint32_t high_part = toNet ? htonl ((uint32_t)(value >> 32))
                                 : ntohl ((uint32_t)(value >> 32));
      uint32_t low_part = toNet ? htonl ((uint32_t)(value & 0xFFFFFFFFLL))
                                : ntohl ((uint32_t)(value & 0xFFFFFFFFLL));
      return ((uint64_t)low_part << 32) | high_part;
    }
  else
    {
      return value;
    }
}

uint64_t
htonll (uint64_t value)
{
  return endianess (value, true);
}

uint64_t
ntohll (uint64_t value)
{
  return endianess (value, false);
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
      exit (1);
    }

  if (fcntl (sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
      exit (1);
    }
}

bool
extractMessage (const char *buffer, int64_t &offset, uint64_t bytes_read,
                std::ostream &logStream)
{
  if (offset + sizeof (uint64_t) > bytes_read)
    return false;

  uint64_t msg_len;
  memcpy (&msg_len, buffer + offset, sizeof (msg_len));
  offset += sizeof (msg_len);
  msg_len = ntohll (msg_len);

  if (offset + msg_len + 1 > bytes_read)
    return false;

  auto msg = std::make_unique<char[]> (msg_len + 1);
  memcpy (msg.get (), buffer + offset, msg_len);
  msg[msg_len] = '\0';
  offset += msg_len;

  if (buffer[offset++] != 0)
    return false;

  logStream.write (msg.get (), msg_len);
  return true;
}

void
parseMessages (const char *buffer, int64_t bytes_read,
               std::ofstream &logStream)
{
  int64_t offset = 0;
  while (offset < bytes_read
         && extractMessage (buffer, offset, bytes_read, logStream))
    {
      // parseMessages
    }
}

std::string
createTemporaryFile (int clientId)
{
  if (clientId < 0 || clientId > 999)
    {
      throw std::invalid_argument ("clientId must be between 0 and 999.");
    }

  std::filesystem::path tempDir = std::filesystem::temp_directory_path ();
  const std::string baseName = "saurion_sender";

  std::ostringstream fileNameStream;
  fileNameStream << baseName << "." << std::setfill ('0') << std::setw (3)
                 << clientId << ".log";

  std::filesystem::path filePath = tempDir / fileNameStream.str ();

  std::ofstream tempFile (filePath,
                          std::ios::out | std::ios::trunc | std::ios::binary);
  if (tempFile.is_open ())
    {
      tempFile.close ();
      return filePath.string ();
    }
  else
    {
      throw std::ios_base::failure (
          "Failed to create or truncate the temporary file.");
    }
}

void
createClient (int clientId, int port)
{

  if (pid_t pid = fork (); pid != 0)
    {
      clients.push_back (pid);
      return;
    }
  signal (SIGUSR1, signalHandler);
  signal (SIGUSR2, sendMessagesHandler);

  std::ostringstream tempFilePath;
  tempFilePath << createTemporaryFile (clientId);
  std::ofstream logStream (tempFilePath.str (), std::ios::app);

  if (!logStream.is_open ())
    {
      exit (1);
    }

  sockfd = socket (AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    {
      exit (1);
    }

  struct sockaddr_in serv_addr;
  memset (&serv_addr, 0, sizeof (serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons (port);
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
  int64_t total_len = 0;

  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 1000000L;
  while (keepRunning)
    {
      int dataAvailable = 3;

      total_len = 0;
      while (dataAvailable > 0)
        {
          int64_t len = read (sockfd, buffer, sizeof (buffer));

          if (len > 0)
            {

              accumulatedBuffer.insert (accumulatedBuffer.end (), buffer,
                                        buffer + len);
              total_len += len;
              continue;
            }
          if ((len == 0)
              || (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
            {
              keepRunning = false;
              break;
            }
          nanosleep (&ts, nullptr);
          --dataAvailable;
          continue;
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
connectClients (int n, int port)
{
  for (int i = 0; i < n; ++i)
    {
      createClient (numClients + i + 1, port);
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
      std::getline (ss, token, ';');
      delay = std::stoi (token);
      connectClients (n, delay);
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
}

void
readPipe (const std::string &pipePath)
{
  int fd = open (pipePath.c_str (), O_RDONLY);
  if (fd < 0)
    {
      return;
    }

  char buffer[4096];
  std::string commandBuffer;
  int64_t bytesRead = 0;
  while ((bytesRead = read (fd, buffer, sizeof (buffer))) > 0L)
    {
      commandBuffer.append (buffer, bytesRead);
      if (commandBuffer.find ('\n') != std::string::npos)
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

template <typename T>
T *
mapSharedMemory (int64_t size)
{
  void *addr = mmap (nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (addr == MAP_FAILED)
    {
      perror ("Error mapeando memoria");
      exit (1);
    }
  return static_cast<T *> (addr);
}

void
global_map ()
{
  globalMessage = mapSharedMemory<char> (10 * CHUNK_SZ * sizeof (char));
  globalMessageCount = mapSharedMemory<int> (sizeof (int));
  globalMessageDelay = mapSharedMemory<int> (sizeof (int));
}

int
main (int argc, char *argv[])
{
  if (argc < 3)
    {
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
