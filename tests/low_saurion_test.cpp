#include "config.h"
#include "low_saurion.h"

#include <cstdio>
#include <ctime>
#include <memory>
#include <ostream>
#include <pthread.h>
#include <random>
#include <stdatomic.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>

#include "gtest/gtest.h"

static int PORT = 8080;

constexpr char FIFO[] = "/tmp/saurion_test_fifo.XXX";

std::string
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

struct summary
{
  explicit summary () = default;
  ~summary () = default;
  summary (const summary &) = delete;
  summary (summary &&) = delete;
  summary &operator= (const summary &) = delete;
  summary &operator= (summary &&) = delete;
  uint32_t connected = 0;
  std::vector<int> fds;
  pthread_cond_t connected_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t connected_m = PTHREAD_MUTEX_INITIALIZER;
  uint32_t disconnected = 0;
  pthread_cond_t disconnected_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t disconnected_m = PTHREAD_MUTEX_INITIALIZER;
  size_t readed = 0;
  pthread_cond_t readed_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t readed_m = PTHREAD_MUTEX_INITIALIZER;
  uint32_t wrote = 0;
  pthread_cond_t wrote_c = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t wrote_m = PTHREAD_MUTEX_INITIALIZER;
} __attribute__ ((aligned (128))) summary;

void signalHandler (int signum);

class low_saurion : public ::testing::Test
{
public:
  struct saurion *saurion;
  static std::string fifo_name;
  static FILE *fifo_write;
  static pid_t pid;

protected:
  static std::string
  generate_random_fifo_name ()
  {
    fifo_name = FIFO;
    std::random_device rd;
    std::mt19937 gen (rd ());
    std::uniform_int_distribution dis (0, 10);

    for (int i = 23; i < 26; i++)
      {
        char c = dis (gen) + '0';
        fifo_name[i] = c;
      }

    return fifo_name;
  }

  static constexpr size_t EXECUTABLE_LENGTH = 1024;

  static void
  SetUpTestSuite ()
  {
    std::random_device rd;
    std::mt19937 gen (rd ());
    std::uniform_int_distribution dis (1024, 65535);
    PORT = dis (gen);
    fifo_name = generate_random_fifo_name ();
    std::signal (SIGINT, signalHandler);
    if (mkfifo (fifo_name.c_str (), 0666) == -1)
      {
        perror ("Error al crear el FIFO");
      }
    pid = fork ();
    if (pid < 0)
      {
        perror ("Error on fork");
        return;
      }
    if (pid == 0)
      {
        std::vector<const char *> exec_args;

        std::string executable_dir
            = get_executable_directory ().append ("/client");

        for (const char *item : { executable_dir.c_str (), (const char *)"-p",
                                  fifo_name.c_str () })
          {
            exec_args.push_back (item);
          }
        exec_args.push_back (nullptr);

        execvp (executable_dir.c_str (), (char *const *)exec_args.data ());
      }
    else
      {
        fifo_write = fopen (fifo_name.c_str (), "w");
      }
  }

  static void
  TearDownTestSuite ()
  {
    close_clients ();
    int status;
    waitpid (pid, &status, 0);
    fclose (fifo_write);
    unlink (fifo_name.c_str ());
  }

  void
  SetUp () override
  {
    const unsigned int N_THREADS = 6;
    summary.connected = 0;
    summary.disconnected = 0;
    summary.readed = 0;
    summary.wrote = 0;
    summary.fds.clear ();
    saurion = saurion_create (N_THREADS);
    if (!saurion)
      {
        return;
      }
    saurion->ss = EXTERNAL_set_socket (PORT);
    if (!saurion->ss)
      {
        throw std::runtime_error (strerror (errno));
      }
    saurion->cb.on_connected = [] (int sfd, void *) {
      pthread_mutex_lock (&summary.connected_m);
      summary.connected++;
      summary.fds.push_back (sfd);
      pthread_cond_signal (&summary.connected_c);
      pthread_mutex_unlock (&summary.connected_m);
    };
    saurion->cb.on_readed
        = [] (int, const void *const, const ssize_t size, void *) {
            pthread_mutex_lock (&summary.readed_m);
            summary.readed += size;
            pthread_cond_signal (&summary.readed_c);
            pthread_mutex_unlock (&summary.readed_m);
          };
    saurion->cb.on_wrote = [] (int, void *) {
      pthread_mutex_lock (&summary.wrote_m);
      summary.wrote++;
      pthread_cond_signal (&summary.wrote_c);
      pthread_mutex_unlock (&summary.wrote_m);
    };
    saurion->cb.on_closed = [] (int sfd, void *) {
      pthread_mutex_lock (&summary.disconnected_m);
      atomic_fetch_add_explicit ((atomic_int *)&summary.disconnected, 1,
                                 memory_order_relaxed);
      pthread_cond_signal (&summary.disconnected_c);
      pthread_mutex_unlock (&summary.disconnected_m);
      pthread_mutex_lock (&summary.connected_m);
      auto &vec = summary.fds;
      vec.erase (std::remove (vec.begin (), vec.end (), sfd), vec.end ());
      pthread_cond_signal (&summary.connected_c);
      pthread_mutex_unlock (&summary.connected_m);
    };
    saurion->cb.on_error = [] (int, const char *const, const ssize_t, void *) {
      // Not tested yet
    };
    if (!saurion_start (saurion))
      {
        exit (ERROR_CODE);
      }
  }

  void
  TearDown () override
  {
    disconnect_clients ();
    saurion_stop (saurion);
    close (saurion->ss);
    saurion_destroy (saurion);
    deleteLogFiles ();
    struct timespec tim;
    tim.tv_sec = 0;
    tim.tv_nsec = 10000000L;
    nanosleep (&tim, nullptr);
  }

  static void
  wait_connected (uint32_t n)
  {
    pthread_mutex_lock (&summary.connected_m);
    while (summary.connected != n)
      {
        pthread_cond_wait (&summary.connected_c, &summary.connected_m);
      }
    pthread_mutex_unlock (&summary.connected_m);
  }

  static void
  wait_disconnected (uint32_t n)
  {
    pthread_mutex_lock (&summary.disconnected_m);
    while (summary.disconnected != n)
      {
        pthread_cond_wait (&summary.disconnected_c, &summary.disconnected_m);
      }
    pthread_mutex_unlock (&summary.disconnected_m);
  }

  static void
  wait_readed (size_t n)
  {
    pthread_mutex_lock (&summary.readed_m);
    while (summary.readed < n)
      {
        pthread_cond_wait (&summary.readed_c, &summary.readed_m);
      }
    pthread_mutex_unlock (&summary.readed_m);
  }

  static void
  wait_wrote (uint32_t n)
  {
    pthread_mutex_lock (&summary.wrote_m);
    while (summary.wrote < n)
      {
        pthread_cond_wait (&summary.wrote_c, &summary.wrote_m);
      }
    pthread_mutex_unlock (&summary.wrote_m);
  }

  static void
  connect_clients (uint32_t n)
  {
    fprintf (fifo_write, "connect;%d;%d\n", n, PORT);
    fflush (fifo_write);
  }

  static void
  disconnect_clients ()
  {
    fprintf (fifo_write, "disconnect;\n");
    fflush (fifo_write);
  }

  static void
  clients_2_saurion (uint32_t n, const char *const msg, uint32_t delay)
  {
    fprintf (fifo_write, "send;%d;%s;%d\n", n, msg, delay);
    fflush (fifo_write);
  }

  static void
  close_clients ()
  {
    fprintf (fifo_write, "close;\n");
    fflush (fifo_write);
  }

  void
  saurion_2_client (int sfd, uint32_t n, const char *const msg) const
  {
    for (uint32_t i = 0; i < n; ++i)
      {
        saurion_send (saurion, sfd, msg);
      }
  }

  void
  saurion_sends_to_all_clients (uint32_t n, const char *const msg)
  {
    for (auto sfd : summary.fds)
      {
        for (uint32_t i = 0; i < n; ++i)
          {
            saurion_send (saurion, sfd, msg);
          }
      }
  }

  static size_t
  countOccurrences (std::string_view content, const std::string &search)
  {
    size_t count = 0;
    size_t pos = content.find (search);
    while (pos != std::string::npos)
      {
        count++;
        pos = content.find (search, pos + search.length ());
      }
    return count;
  }

  static size_t
  read_from_clients (const std::string &search)
  {
    size_t occurrences = 0;
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

private:
  void
  deleteLogFiles ()
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
            catch (...)
              {
                // Do nothing
              }
          }
      }
  }
};

std::string low_saurion::fifo_name = "";
pid_t low_saurion::pid = 0;
FILE *low_saurion::fifo_write = nullptr;

void
signalHandler (int signum)
{
  std::cout << "Interceptada la señal " << signum << std::endl;

  if (std::remove (low_saurion::fifo_name.c_str ()) != 0)
    {
      std::cerr << "Error al eliminar " << low_saurion::fifo_name << std::endl;
    }
  else
    {
      std::cout << low_saurion::fifo_name << " eliminado exitosamente."
                << std::endl;
    }
  std::regex pattern ("^saurion_sender.*\\.log$");

  for (const auto &entry : std::filesystem::directory_iterator ("/tmp"))
    {
      if (std::filesystem::is_regular_file (entry))
        {
          std::string filename = entry.path ().filename ().string ();
          if (std::regex_match (filename, pattern))
            {
              try
                {
                  std::filesystem::remove (entry.path ());
                  std::cout << "Archivo borrado: " << filename << std::endl;
                }
              catch (const std::filesystem::filesystem_error &e)
                {
                  std::cerr << "Error al borrar " << filename << ": "
                            << e.what () << std::endl;
                }
            }
        }
    }
  exit (ERROR_CODE);
}

TEST_F (low_saurion, initServerAndCloseCorrectly) { EXPECT_TRUE (true); }

TEST_F (low_saurion, connectMultipleClients)
{
  uint32_t clients = 20;
  connect_clients (clients);
  wait_connected (clients);
  EXPECT_EQ (summary.connected, clients);
  disconnect_clients ();
  wait_disconnected (clients);
  EXPECT_EQ (summary.disconnected, clients);
}

TEST_F (low_saurion, readWriteMsgsToClients)
{
  uint32_t clients = 20;
  uint32_t msgs = 100;
  connect_clients (clients);
  wait_connected (clients);
  EXPECT_EQ (summary.connected, clients);
  for (const auto &cfd : summary.fds)
    {
      saurion_2_client (cfd, msgs, "Hola");
    }
  clients_2_saurion (msgs, "Hola", 0);
  wait_readed (msgs * clients * 4);
  EXPECT_EQ (summary.readed, msgs * clients * 4);
  disconnect_clients ();
  wait_disconnected (clients);
  EXPECT_EQ (summary.disconnected, clients);
}

TEST_F (low_saurion, reconnectClients)
{
  uint32_t clients = 5;
  connect_clients (clients);
  wait_connected (clients);
  EXPECT_EQ (summary.connected, clients);
  disconnect_clients ();
  wait_disconnected (clients);
  EXPECT_EQ (summary.disconnected, clients);
  connect_clients (clients);
  wait_connected (clients * 2);
  EXPECT_EQ (summary.connected, clients * 2);
  disconnect_clients ();
  wait_disconnected (clients * 2);
  EXPECT_EQ (summary.disconnected, clients * 2);
}

TEST_F (low_saurion, readWriteWithLargeMessageMultipleOfChunkSize)
{
  uint32_t clients = 1;
  size_t size = CHUNK_SZ * 2;
  auto str = std::make_unique<char[]> (size + 1);
  std::memset (str.get (), 'A', size);
  str[size - 1] = '1';
  str[size] = 0;
  connect_clients (clients);
  wait_connected (clients);
  EXPECT_EQ (summary.connected, clients);
  clients_2_saurion (1, str.get (), 0);
  wait_readed (size);
  EXPECT_EQ (summary.readed, size);
  saurion_2_client (summary.fds.front (), 1, str.get ());
  wait_wrote (1);
  disconnect_clients ();
  wait_disconnected (clients);
  EXPECT_EQ (1UL, read_from_clients (std::string (str.get ())));
  EXPECT_EQ (summary.disconnected, clients);
}

TEST_F (low_saurion, handleConcurrentReadsAndWrites)
{
  uint32_t clients = 20;
  uint32_t msgs = 10;
  connect_clients (clients);
  wait_connected (clients);
  EXPECT_EQ (summary.connected, clients);
  clients_2_saurion (msgs, "Hola", 2);
  saurion_sends_to_all_clients (msgs, "Hola");
  wait_readed (msgs * clients * 4);
  EXPECT_EQ (msgs * clients * 4, summary.readed);
  wait_wrote (msgs * clients);
  EXPECT_EQ (msgs * clients, summary.wrote);
  disconnect_clients ();
  wait_disconnected (clients);
}
