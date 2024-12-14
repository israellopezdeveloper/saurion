#ifndef CLIENT_INTERFACE_HPP
#define CLIENT_INTERFACE_HPP

#include <cstdint> // for uint64_t
#include <string>  // for string

// set_port
int set_port ();

// set_fifoname
std::string set_fifoname ();

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

  std::string getFifoPath () const;
  int getPort () const;

private:
  pid_t pid;
  FILE *fifo;
  std::string fifoname = set_fifoname ();
  int port = set_port ();
};

#endif // !CLIENT_INTERFACE_HPP
