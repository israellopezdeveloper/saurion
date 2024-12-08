#include "client_interface.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <linux/limits.h>
#include <random>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>

// get_executable_directory
static std::string
get_executable_directory ()
{
  std::string buffer (PATH_MAX, '\0');

  ssize_t len = readlink ("/proc/self/exe", &buffer[0], buffer.size () - 1);
  if (len <= 0 || len >= static_cast<ssize_t> (buffer.size ()))
    {
      throw std::runtime_error ("Failed to read symbolic link /proc/self/exe");
    }

  buffer.resize (len);

  if (size_t last_slash_pos = buffer.find_last_of ('/');
      last_slash_pos != std::string::npos)
    {
      buffer.erase (last_slash_pos);
    }

  std::string real_path (PATH_MAX, '\0');
  if (!realpath (buffer.c_str (), &real_path[0]))
    {
      throw std::runtime_error ("Failed to resolve real path");
    }

  if (size_t libs_pos = real_path.find ("/.libs");
      libs_pos != std::string::npos)
    {
      real_path.erase (libs_pos);
    }

  return real_path;
}

// generate_random_fifo_name
static std::string
generate_random_fifo_name ()
{
  constexpr char FIFO[] = "/tmp/saurion_test_fifo.XXX";
  std::string file_name = FIFO;
  std::random_device rd;
  std::mt19937 gen (rd ());
  std::uniform_int_distribution dis (0, 10);

  for (int i = 23; i < 26; i++)
    {
      char c = dis (gen) + '0';
      file_name[i] = c;
    }

  return file_name;
}

// set_port
static int
set_port ()
{
  std::random_device rd;
  std::mt19937 gen (rd ());
  std::uniform_int_distribution dis (1024, 65535);
  return dis (gen);
}

// set_fifoname
static std::string
set_fifoname ()
{
  std::string filename = generate_random_fifo_name ();
  if (mkfifo (filename.c_str (), 0666) == -1)
    {
      perror ("Error al crear el FIFO");
    }
  return filename;
}

// exec_client
static pid_t
exec_client (const std::string &filename, FILE *&fifo_write)
{
  pid_t pid;
  pid = fork ();
  if (pid < 0)
    {
      perror ("Error on fork");
      return 0;
    }
  if (pid == 0)
    {
      std::vector<const char *> exec_args;

      std::string executable_dir
          = get_executable_directory ().append ("/client");

      for (const char *item :
           { executable_dir.c_str (), (const char *)"-p", filename.c_str () })
        {
          exec_args.push_back (item);
        }
      exec_args.push_back (nullptr);

      execvp (executable_dir.c_str (),
              const_cast<char *const *> (exec_args.data ()));
    }
  else
    {
      fifo_write = fopen (filename.c_str (), "w");
    }
  return pid;
}

// Constructor
ClientInterface::ClientInterface () noexcept : fifoname (set_fifoname ()),
                                               port (set_port ())
{
  pid = exec_client (fifoname, fifo);
}

// Destructor
ClientInterface::~ClientInterface ()
{
  fprintf (fifo, "close;\n");
  fflush (fifo);
  int status;
  waitpid (pid, &status, 0);
  fclose (fifo);
  unlink (fifoname.c_str ());
  clean ();
}

// connect
void
ClientInterface::connect (const uint n)
{
  fprintf (fifo, "connect;%d;%d\n", n, port);
  fflush (fifo);
}

// disconnect
void
ClientInterface::disconnect ()
{
  fprintf (fifo, "disconnect;\n");
  fflush (fifo);
}

// send
void
ClientInterface::send (const uint n, const char *const msg, uint delay)
{
  fprintf (fifo, "send;%d;%s;%d\n", n, msg, delay);
  fflush (fifo);
}

// countOccurrences
static uint64_t
countOccurrences (std::string_view content, const std::string &search)
{
  uint64_t count = 0;
  uint64_t pos = content.find (search);
  while (pos != std::string::npos)
    {
      count++;
      pos = content.find (search, pos + search.length ());
    }
  return count;
}

// reads
uint64_t
ClientInterface::reads (const std::string &search) const
{
  uint64_t occurrences = 0;
  for (const auto &entry : std::filesystem::directory_iterator ("/tmp/"))
    {
      const auto &path = entry.path ();
      if (path.filename ().string ().find ("saurion_sender.") == 0)
        {
          std::ifstream file (path);
          if (file.is_open ())
            {
              std::stringstream buffer;
              buffer << file.rdbuf ();
              std::string content = buffer.str ();
              occurrences += countOccurrences (content, search);
            }
        }
    }
  return occurrences;
}

// clean
void
ClientInterface::clean () const
{
  for (const auto &entry : std::filesystem::directory_iterator ("/tmp/"))
    {
      const auto &path = entry.path ();
      if (path.filename ().string ().find ("saurion_sender.") == 0)
        {
          try
            {
              std::filesystem::remove (path);
            }
          catch (std::filesystem::filesystem_error &)
            {
            }
        }
    }
}

// getFifoPath
const std::string
ClientInterface::getFifoPath () const
{
  return this->fifoname;
}

// getPort
int
ClientInterface::getPort () const
{
  return this->port;
}
