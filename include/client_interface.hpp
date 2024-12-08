#ifndef CLIENT_INTERFACE_HPP
#define CLIENT_INTERFACE_HPP

#include <cstdint>
#include <cstdio>
#include <string>
#include <sys/types.h>
#include <unistd.h>

class ClientInterface
{
public:
  explicit ClientInterface () noexcept;
  ~ClientInterface ();

  ClientInterface (const ClientInterface &) = delete;
  ClientInterface (ClientInterface &&) = delete;
  ClientInterface &operator= (const ClientInterface &) = delete;
  ClientInterface &operator= (ClientInterface &&) = delete;

  void connect (const uint n);
  void disconnect ();

  void send (const uint n, const char *const msg, uint delay);
  uint64_t reads (const std::string &search) const;
  void clean () const;

  const std::string getFifoPath () const;
  int getPort () const;

private:
  pid_t pid;
  FILE *fifo;
  std::string fifoname;
  int port;
};

#endif // !CLIENT_INTERFACE_HPP
