[TOC]

# ThreadPool

La clase `ThreadPool` proporciona una implementación de un pool de hilos en
C++. Este pool de hilos permite la ejecución concurrente de tareas en varios
hilos, lo que puede mejorar el rendimiento de las aplicaciones que necesitan
manejar múltiples tareas de manera simultánea.

Asimismo, la clase `ThreadPool` proporciona funciones adicionales para
crear multiples colas de tareas con distinto nivel de concurrencia, permitiendo
la ejecución de tareas en diferentes niveles de concurrencia. Es decir, una cola
con un nivel de concurrencia de 2 significa que solo pueden ser ejecutadas dos
tareas al mismo tiempo.

## Uso

Para utilizar la clase `ThreadPool` en tu proyecto, sigue estos pasos:

1. Incluye el archivo de encabezado `ThreadPool.hpp` en tu código fuente:

```cpp
#include "ThreadPool.hpp"
```

2. Crea una instancia de `ThreadPool` especificando el número de hilos en el constructor, si es necesario:

```cpp
ThreadPool pool(4); // Crea un pool de hilos con 4 hilos
```

3. Agrega tareas al pool de hilos utilizando el método `add`, que acepta un puntero a función y un argumento opcional:

```cpp
void myFunction(void* arg) {
    // Implementación de la tarea
}

pool.add(myFunction, nullptr); // Agrega una tarea al pool de hilos
```

4. Inicia el pool de hilos con el método `init`:

```cpp
pool.init(); // Inicia el pool de hilos
```

5. Cuando ya no necesites el pool de hilos, puedes detenerlo con el método `stop`:

```cpp
pool.stop(); // Detiene el pool de hilos
```

Aquí tienes una posible entrada para la sección de API en el README en formato Markdown:

## API

La clase `ThreadPool` proporciona las siguientes funciones públicas para interactuar con el pool de hilos:

### Constructor

#### ThreadPool()

- **Descripción:** Constructor predeterminado que inicializa un pool de hilos con un tamaño predeterminado.
- **Parámetros:** Ninguno.
- **Uso:**
  ```cpp
  ThreadPool pool; // Crea un pool de hilos con tamaño predeterminado
  ```

#### ThreadPool(size_t num_threads)

- **Descripción:** Constructor que inicializa un pool de hilos con el número especificado de hilos.
- **Parámetros:**
  - `num_threads`: Número de hilos que se crearán en el pool.
- **Uso:**
  ```cpp
  ThreadPool pool(4); // Crea un pool de hilos con 4 hilos
  ```

### Métodos públicos

#### void init()

- **Descripción:** Inicia el pool de hilos, creando y lanzando los hilos.
- **Parámetros:** Ninguno.
- **Uso:**
  ```cpp
  pool.init(); // Inicia el pool de hilos
  ```

#### void stop()

- **Descripción:** Detiene el pool de hilos, esperando a que todas las tareas se completen y luego deteniendo los hilos.
- **Parámetros:** Ninguno.
- **Uso:**
  ```cpp
  pool.stop(); // Detiene el pool de hilos
  ```

#### void add(uint32_t qid, void (*nfn)(void*), void* arg)

- **Descripción:** Agrega una tarea al pool de hilos, especificando la identificación de la cola, la función de la tarea y un argumento opcional.
- **Parámetros:**
  - `qid`: Identificación de la cola a la que se agregará la tarea.
  - `nfn`: Puntero a la función que representa la tarea.
  - `arg`: Argumento opcional que se pasa a la función de la tarea.
- **Uso:**
  ```cpp
  pool.add(0, myFunction, nullptr); // Agrega una tarea al pool de hilos
  ```

#### void add(void (*nfn)(void*), void* arg)

- **Descripción:** Sobrecarga de la función `add` que agrega una tarea al pool de hilos a la cola predeterminada.
- **Parámetros:**
  - `nfn`: Puntero a la función que representa la tarea.
  - `arg`: Argumento opcional que se pasa a la función de la tarea.
- **Uso:**
  ```cpp
  pool.add(myFunction, nullptr); // Agrega una tarea al pool de hilos
  ```

#### void new_queue(uint32_t qid, uint32_t cnt)

- **Descripción:** Crea una nueva cola de tareas con la identificación y el nivel de concurrencia especificados.
- **Parámetros:**
  - `qid`: Identificación de la nueva cola de tareas.
  - `cnt`: Número de tareas que se pueden ejecutar simultáneamente en la cola.
- **Uso:**
  ```cpp
  pool.new_queue(1, 10); // Crea una nueva cola de tareas con identificación 1 y nivel de concurrencia 10
  ```

#### void remove_queue(uint32_t qid)

- **Descripción:** Elimina la cola de tareas con la identificación especificada.
- **Parámetros:**
  - `qid`: Identificación de la cola de tareas a eliminar.
- **Uso:**
  ```cpp
  pool.remove_queue(1); // Elimina la cola de tareas con identificación 1
  ```

#### bool empty()

- **Descripción:** Verifica si el pool de hilos está vacío, es decir, si no hay tareas pendientes en ninguna cola.
- **Parámetros:** Ninguno.
- **Retorno:** `true` si el pool de hilos está vacío, `false` de lo contrario.
- **Uso:**
  ```cpp
  if (pool.empty()) {
      // El pool de hilos está vacío
  }
  ```

#### void wait_closeable()

- **Descripción:** Espera hasta que el pool de hilos pueda cerrarse, es decir, hasta que todas las tareas pendientes se completen.
- **Parámetros:** Ninguno.
- **Uso:**
  ```cpp
  pool.wait_closeable(); // Espera hasta que el pool de hilos pueda cerrarse
  ```

#### void wait_empty()

- **Descripción:** Espera hasta que el pool de hilos esté vacío, es decir, hasta que no haya tareas pendientes en ninguna cola.
- **Parámetros:** Ninguno.
- **Uso:**
  ```cpp
  pool.wait_empty(); // Espera hasta que el pool de hilos esté vacío
  ```

## Requisitos del sistema

- C++11 o superior
- Biblioteca estándar de C++
- Soporte para hilos POSIX (pthread)

## Contribuciones

Las contribuciones son bienvenidas. Si encuentras algún error o tienes alguna sugerencia de mejora, no dudes en abrir un problema o enviar una solicitud de extracción.

## Autor

Este proyecto fue desarrollado por [Israel López](https://github.com/tu-usuario).

