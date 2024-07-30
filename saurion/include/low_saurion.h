#ifndef LOW_SAURION_H
#define LOW_SAURION_H

#include <liburing.h>
#include <pthread.h>
#include <stdint.h>

#include "cthreadpool.hpp"
#include "linked_list.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PACKING_SZ 128

/*!
 * @brief Estructura principal para el manejo de io_uring y eventos de socket.
 *
 * Esta estructura contiene todos los datos necesarios para manejar la cola de eventos
 * de io_uring, así como los callbacks para los eventos de socket.
 */
struct saurion {
  struct io_uring *rings;   /**< Array de estructuras io_uring para manejar la cola de eventos. */
  pthread_mutex_t *m_rings; /**< Array de mutex para proteger los anillos */
  int ss;                   /**< Descriptor de socket del servidor. */
  int *efds;                /**< Descriptores de eventfd para señales internas. */
  struct Node *list;        /**< Lista enlazada para almacenar las solicitudes en curso. */
  pthread_mutex_t status_m; /**< Mutex para proteger el estado de la estructura. */
  pthread_cond_t status_c;  /**< Condición para señalar cambios en el estado. */
  int status;               /**< Estado actual de la estructura. */
  ThreadPool *pool;         /**< Pool de Threads para ejecutar en paralelo las operaciones */
  uint32_t n_threads;       /**< Numero de threads */
  uint32_t next;            /**< Ring al cual añadir evento. */

  /*!
   * @brief Estructura de callbacks para manejar los eventos de socket.
   *
   * Esta estructura contiene punteros a funciones de callback para manejar
   * los eventos de conexión, lectura, escritura, cierre y error.
   */
  struct saurion_callbacks {
    /*!
     * @brief Callback para el evento de conexión.
     *
     * @param fd Descriptor de archivo del socket conectado.
     * @param arg Argumento adicional proporcionado por el usuario.
     */
    void (*on_connected)(const int fd, void *arg);
    void *on_connected_arg; /**< Argumento adicional para el callback de conexión. */

    /*!
     * @brief Callback para el evento de lectura.
     *
     * @param fd Descriptor de archivo del socket.
     * @param content Puntero al contenido leído.
     * @param len Longitud del contenido leído.
     * @param arg Argumento adicional proporcionado por el usuario.
     */
    void (*on_readed)(const int fd, const void *const content, const ssize_t len, void *arg);
    void *on_readed_arg; /**< Argumento adicional para el callback de lectura. */

    /*!
     * @brief Callback para el evento de escritura.
     *
     * @param fd Descriptor de archivo del socket.
     * @param arg Argumento adicional proporcionado por el usuario.
     */
    void (*on_wrote)(const int fd, void *arg);
    void *on_wrote_arg; /**< Argumento adicional para el callback de escritura. */

    /*!
     * @brief Callback para el evento de cierre.
     *
     * @param fd Descriptor de archivo del socket cerrado.
     * @param arg Argumento adicional proporcionado por el usuario.
     */
    void (*on_closed)(const int fd, void *arg);
    void *on_closed_arg; /**< Argumento adicional para el callback de cierre. */

    /*!
     * @brief Callback para el evento de error.
     *
     * @param fd Descriptor de archivo del socket.
     * @param content Puntero al contenido leído.
     * @param len Longitud del contenido leído.
     * @param arg Argumento adicional proporcionado por el usuario.
     */
    void (*on_error)(const int fd, const char *const content, const ssize_t len, void *arg);
    void *on_error_arg; /**< Argumento adicional para el callback de error. */
  } __attribute__((aligned(PACKING_SZ))) cb;
} __attribute__((aligned(PACKING_SZ)));

/*! TODO: Eliminar
 *  \todo Eliminar
 */
int EXTERNAL_set_socket(int p);

/*!
 * @brief Crea una instancia de la estructura saurion.
 *
 * Esta función inicializa la estructura saurion, configura el eventfd y la cola de io_uring,
 * y prepara la estructura para su uso.
 *
 * @return struct saurion* Puntero a la estructura saurion creada, o NULL en caso de error.
 */
[[nodiscard]]
struct saurion *saurion_create(uint32_t n_threads);

/*!
 * @brief Inicia el procesamiento de eventos en la estructura saurion.
 *
 * Esta función inicia la aceptación de conexiones y el procesamiento de eventos de io_uring.
 * Se ejecuta en un bucle hasta que se recibe una señal de parada.
 *
 * @param s Puntero a la estructura saurion.
 * @return int 0 en caso de éxito, 1 en caso de error.
 */
[[nodiscard]]
int saurion_start(struct saurion *s);

/*!
 * @brief Detiene el procesamiento de eventos en la estructura saurion.
 *
 * Esta función envía una señal al eventfd para indicar que se debe detener el bucle de eventos.
 *
 * @param s Puntero a la estructura saurion.
 */
void saurion_stop(const struct saurion *s);

/*!
 * @brief Destruye la estructura saurion y libera los recursos asociados.
 *
 * Esta función espera a que se detenga el procesamiento de eventos, libera la memoria
 * de la estructura saurion y cierra los descriptores de archivo.
 *
 * @param s Puntero a la estructura saurion.
 */
void saurion_destroy(struct saurion *s);

/*!
 * @brief Envía un mensaje a través de un socket utilizando io_uring.
 *
 * Esta función prepara y envía un mensaje a través del socket especificado utilizando
 * la cola de eventos de io_uring.
 *
 * @param s Puntero a la estructura saurion.
 * @param fd Descriptor de archivo del socket al que se enviará el mensaje.
 * @param msg Puntero a la cadena de bytes que se enviará.
 */
void saurion_send(struct saurion *s, const int fd, const void *const msg);

#ifdef __cplusplus
}
#endif

#endif  // !LOW_SAURION_H
