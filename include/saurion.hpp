/*!
 * @defgroup HighSaurion
 *
 * @brief Header file for the Saurion library.
 *
 * The Saurion library provides an abstraction for handling network connections
 * with callback-based event handling.
 *
 * ### Overview:
 *
 * The `Saurion` class manages a network socket and provides callbacks for
 * various events:
 * - Connection established
 * - Data received
 * - Data sent
 * - Connection closed
 * - Errors
 *
 * ### Class Diagram:
 * ```
 * +-------------------+
 * |      Saurion      |
 * +-------------------+
 * | + init()          |
 * | + stop()          |
 * | + send(fd, msg)   |
 * | + on_connected()  |
 * | + on_readed()     |
 * | + on_wrote()      |
 * | + on_closed()     |
 * | + on_error()      |
 * +-------------------+
 *       | Uses
 *       v
 * +-------------------+
 * |  saurion struct   |
 * +-------------------+
 * ```
 *
 * ### Example Usage:
 *
 * ```cpp
 * #include "saurion.hpp"
 * #include <iostream>
 *
 * void on_connected(const int fd, void *arg) {
 *     std::cout << "Connected: " << fd << std::endl;
 * }
 *
 * void on_readed(const int fd, const void *const data, const int64_t len,
 *     void *arg) {
 *   std::cout << "Read data: " << std::string((char *)data, len) << std::endl;
 * }
 *
 * void on_closed(const int fd, void *arg) {
 *     std::cout << "Connection closed: " << fd << std::endl;
 * }
 *
 * int main() {
 *     Saurion server(4, 8080);
 *     server.on_connected(on_connected, nullptr)
 *           .on_readed(on_readed, nullptr)
 *           .on_closed(on_closed, nullptr);
 *
 *     server.init();
 *
 *     std::this_thread::sleep_for(std::chrono::seconds(10));
 *
 *     server.stop();
 *     return 0;
 * }
 * ```
 *
 * @author Israel
 * @date 2024
 *
 * @{
 */
#ifndef SAURION_HPP
#define SAURION_HPP

#include <cstdint>
#include <stdint.h> // for uint32_t, int64_t

/*!
 * @brief A class for managing network connections with callback-based event
 * handling.
 *
 * The `Saurion` class encapsulates a network socket and provides methods for
 * initializing, stopping, and sending data, as well as setting up event
 * callbacks.
 */
class Saurion
{
public:
  /*!
   * @typedef ConnectedCb
   * @brief Callback type for connection events.
   * @param fd File descriptor for the connected socket.
   * @param arg User-defined argument.
   */
  using ConnectedCb = void (*) (const int, void *);
  /*!
   * @typedef ReadedCb
   * @brief Callback type for data received events.
   * @param fd File descriptor of the socket.
   * @param data Pointer to the received data.
   * @param len Length of the received data.
   * @param arg User-defined argument.
   */
  using ReadedCb
      = void (*) (const int, const void *const, const int64_t, void *);
  /*!
   * @typedef WroteCb
   * @brief Callback type for data sent events.
   * @param fd File descriptor of the socket.
   * @param arg User-defined argument.
   */
  using WroteCb = void (*) (const int, void *);
  /*!
   * @typedef ClosedCb
   * @brief Callback type for connection closed events.
   * @param fd File descriptor of the closed socket.
   * @param arg User-defined argument.
   */
  using ClosedCb = void (*) (const int, void *);
  /*!
   * @typedef ErrorCb
   * @brief Callback type for error events.
   * @param fd File descriptor of the socket where the error occurred.
   * @param msg Error message.
   * @param len Length of the error message.
   * @param arg User-defined argument.
   */
  using ErrorCb
      = void (*) (const int, const char *const, const int64_t, void *);

  /*!
   * @brief Constructs a `Saurion` instance.
   * @param thds Number of threads for handling connections.
   * @param sck Listening socket file descriptor.
   */
  explicit Saurion (const uint32_t thds, const int sck) noexcept;
  /*!
   * @brief Destroys the `Saurion` instance, releasing resources.
   */
  ~Saurion ();

  Saurion (const Saurion &) = delete;
  Saurion (Saurion &&) = delete;
  Saurion &operator= (const Saurion &) = delete;
  Saurion &operator= (Saurion &&) = delete;

  /*!
   * @brief Initializes the server and starts listening for connections.
   */
  void init ();
  /*!
   * @brief Stops the server and all associated threads.
   */
  void stop () const noexcept;

  /*!
   * @brief Sets the callback for connection events.
   * @param ncb The callback function.
   * @param arg User-defined argument for the callback.
   * @return Pointer to the `Saurion` instance for chaining.
   */
  Saurion *on_connected (ConnectedCb ncb, void *arg) noexcept;
  /*!
   * @brief Sets the callback for data received events.
   * @param ncb The callback function.
   * @param arg User-defined argument for the callback.
   * @return Pointer to the `Saurion` instance for chaining.
   */
  Saurion *on_readed (ReadedCb ncb, void *arg) noexcept;
  /*!
   * @brief Sets the callback for data sent events.
   * @param ncb The callback function.
   * @param arg User-defined argument for the callback.
   * @return Pointer to the `Saurion` instance for chaining.
   */
  Saurion *on_wrote (WroteCb ncb, void *arg) noexcept;
  /*!
   * @brief Sets the callback for connection closed events.
   * @param ncb The callback function.
   * @param arg User-defined argument for the callback.
   * @return Pointer to the `Saurion` instance for chaining.
   */
  Saurion *on_closed (ClosedCb ncb, void *arg) noexcept;
  /*!
   * @brief Sets the callback for error events.
   * @param ncb The callback function.
   * @param arg User-defined argument for the callback.
   * @return Pointer to the `Saurion` instance for chaining.
   */
  Saurion *on_error (ErrorCb ncb, void *arg) noexcept;

  /*!
   * @brief Sends a message to the specified file descriptor.
   * @param fd File descriptor to send the message to.
   * @param msg Pointer to the message to send.
   */
  void send (const int fd, const char *const msg) noexcept;

private:
  struct saurion *s; //!< Pointer to the underlying `saurion` structure.
};

#endif // !SAURION_HPP

/*!
 * @}
 */
