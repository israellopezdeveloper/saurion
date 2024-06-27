// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include <stdexcept>
#include <thread>

#include "gtest/gtest.h"
#include "threadpool.hpp"

void taskWithError(void* /*unused*/) { throw std::runtime_error("Error dentro de la tarea"); }

void printPointerValue(void* arg) {
  if (arg == nullptr) {
    return;
  }
  int* ptr = static_cast<int*>(arg);
  std::cout << "Valor del puntero: " << *ptr << std::endl;
}

TEST(ThreadPoolTest, DefaultConstructor) {
  ThreadPool pool;
  EXPECT_EQ(pool.empty(), true);
}

TEST(ThreadPoolTest, ConstructorWithNumThreads) {
  ThreadPool pool(4);
  EXPECT_EQ(pool.empty(), true);
}

TEST(ThreadPoolTest, AddTask) {
  ThreadPool pool;
  pool.init();
  int x = 0;
  pool.add(
      [](void* arg) {
        int* ptr = static_cast<int*>(arg);
        (*ptr)++;
      },
      &x);
  pool.wait_empty();
  EXPECT_EQ(x, 1);
}

TEST(ThreadPoolTest, AddMultipleTasks) {
  ThreadPool pool(2);
  pool.init();
  int x = 0;
  pool.add(
      [](void* arg) {
        int* ptr = static_cast<int*>(arg);
        (*ptr)++;
      },
      &x);
  pool.add(
      [](void* arg) {
        int* ptr = static_cast<int*>(arg);
        (*ptr) += 10;
      },
      &x);
  pool.wait_empty();
  EXPECT_EQ(x, 11);
}

TEST(ThreadPoolTest, AddTaskToSpecificQueue) {
  ThreadPool pool;
  pool.init();
  int x = 0;
  pool.new_queue(1, 1);  // Crear una cola de tareas con ID 1 y capacidad 1
  pool.add(
      1,
      [](void* arg) {
        int* ptr = static_cast<int*>(arg);
        (*ptr)++;
      },
      &x);
  pool.wait_empty();
  EXPECT_EQ(x, 1);
}

TEST(ThreadPoolTest, RemoveQueue) {
  ThreadPool pool;
  pool.new_queue(1, 1);  // Crear una cola de tareas con ID 1 y capacidad 1
  pool.remove_queue(1);
  EXPECT_EQ(pool.empty(), true);
}

TEST(ThreadPoolTest, StopThreadPool) {
  ThreadPool pool(4);
  pool.stop();
  EXPECT_EQ(pool.empty(), true);
}

TEST(ThreadPoolTest, WaitForTasksBeforeStopping) {
  ThreadPool pool(2);
  pool.init();
  int x = 0;
  pool.add(
      [](void* arg) {
        int* ptr = static_cast<int*>(arg);
        (*ptr)++;
      },
      &x);
  pool.stop();
  EXPECT_EQ(x, 1);
}

TEST(ThreadPoolTest, WaitEmptyThreadPool) {
  ThreadPool pool(2);
  pool.init();
  int x = 0;
  pool.add(
      [](void* arg) {
        int* ptr = static_cast<int*>(arg);
        (*ptr)++;
      },
      &x);
  pool.wait_empty();
  EXPECT_EQ(pool.empty(), true);
}

TEST(ThreadPoolTest, MultipleQueues) {
  ThreadPool pool;
  pool.init();
  int x = 0;
  int y = 0;
  pool.new_queue(1, 1);  // Crear una cola de tareas con ID 1 y capacidad 1
  pool.add(
      1,
      [](void* arg) {
        int* ptr = static_cast<int*>(arg);
        (*ptr)++;
      },
      &x);
  pool.new_queue(2, 1);  // Crear una cola de tareas con ID 2 y capacidad 1
  pool.add(
      2,
      [](void* arg) {
        int* ptr = static_cast<int*>(arg);
        (*ptr) += 10;
      },
      &y);
  pool.wait_empty();
  EXPECT_EQ(x, 1);
  EXPECT_EQ(y, 10);
}

TEST(ThreadPoolTest, ZeroThreads) {
  ThreadPool pool(0);
  pool.init();
  EXPECT_EQ(pool.empty(), true);
}

TEST(ThreadPoolTest, AddExistantQueue) {
  ThreadPool pool;
  pool.init();
  pool.new_queue(1, 1);
  for (uint32_t i = 1; i < 100; i++) {
    EXPECT_THROW(pool.new_queue(1, 1), std::out_of_range);
  }
}

TEST(ThreadPoolTest, RemoveNonexistentQueue) {
  ThreadPool pool;
  pool.init();
  for (uint32_t i = 1; i < 100; i++) {
    EXPECT_THROW(pool.remove_queue(i), std::out_of_range);
  }
}

TEST(ThreadPoolTest, StopWithoutInit) {
  ThreadPool pool;
  EXPECT_NO_THROW(pool.stop());
}

TEST(ThreadPoolTest, UninitializedThreadPool) {
  ThreadPool pool;

  // Intentar agregar una tarea sin inicializar el pool
  EXPECT_THROW(pool.add([](void*) {}, nullptr), std::logic_error);
}

TEST(ThreadPoolTest, AddTaskToNonExistentQueue) {
  ThreadPool pool;
  pool.init();

  EXPECT_THROW(pool.add(
                   100, [](void*) {}, nullptr),
               std::out_of_range);
}

TEST(ThreadPoolTest, AddTaskAfterStop) {
  ThreadPool pool;
  pool.init();
  pool.stop();

  // Intentar agregar una tarea después de detener el pool
  EXPECT_THROW(pool.add([](void*) {}, nullptr), std::logic_error);
}

TEST(ThreadPoolTest, HandleTaskException) {
  ThreadPool pool;
  pool.init();

  // Agrega la tarea que lanza una excepción al pool de hilos
  for (int i = 0; i < 10; ++i) {
    ASSERT_NO_THROW(pool.add(taskWithError, nullptr));
  }

  // Espera hasta que el pool de hilos esté vacío
  pool.wait_empty();

  // Detiene el pool de hilos
  pool.stop();
}

TEST(ThreadPoolTest, NullPointerArgument) {
  ThreadPool pool;
  pool.init();

  // Agrega la tarea con un puntero nulo como argumento al pool de hilos
  int* nullPtr = nullptr;
  ASSERT_NO_THROW(pool.add(printPointerValue, nullPtr));

  // Espera hasta que el pool de hilos esté vacío
  pool.wait_empty();

  // Detiene el pool de hilos
  pool.stop();
}

TEST(ThreadPoolTest, ManyQueuesWithManyTasks) {
  ThreadPool pool;

  // Iniciar el pool de hilos
  pool.init();

  // Crear 20 colas con distintos niveles de concurrencia
  for (int i = 0; i < 20; ++i) {
    try {
      pool.new_queue(i, i + 1);  // Nivel de concurrencia de 1 a 20
    } catch (...) {
    }
  }

  // Agregar más de 1000 tareas a cada cola
  for (int i = 0; i < 20; ++i) {
    for (int j = 0; j < 1000; ++j) {
      pool.add(
          i, [](void*) {}, nullptr);
    }
  }

  // Esperar hasta que todas las tareas se completen
  pool.wait_empty();

  // Detener el pool de hilos
  pool.stop();

  // Verificar que todas las tareas se hayan completado correctamente
  EXPECT_TRUE(pool.empty());
}

TEST(ThreadPoolTest, AddNullFunctionPointer) {
  ThreadPool pool;

  // Iniciar el pool de hilos
  pool.init();

  // Intentar agregar una tarea con un puntero a función nulo
  for (int i = 0; i < 10; ++i) {
    ASSERT_THROW({ pool.add(nullptr, nullptr); }, std::logic_error);
  }
}

TEST(ThreadPoolTest, InitMultipleTimes) {
  ThreadPool pool;

  // Iniciar el pool de hilos
  for (int i = 0; i < 10; ++i) {
    ASSERT_NO_THROW({ pool.init(); });
  }

  // Detener el pool de hilos
  pool.stop();
}

TEST(ThreadPoolTest, AddTaskWithInvalidArgument) {
  ThreadPool pool;

  // Iniciar el pool de hilos
  pool.init();

  // Intentar agregar una tarea con un argumento inválido
  ASSERT_NO_THROW({ pool.add([](void*) {}, reinterpret_cast<void*>(-1)); });
}

TEST(ThreadPoolTest, AddTaskToDeletedQueue) {
  ThreadPool pool;

  // Iniciar el pool de hilos
  pool.init();

  // Crear una nueva cola de tareas
  pool.new_queue(1, 1);

  // Eliminar la cola de tareas
  pool.remove_queue(1);

  // Intentar agregar una tarea a la cola eliminada
  ASSERT_THROW(
      {
        pool.add(
            1, [](void*) {}, nullptr);
      },
      std::out_of_range);
}

TEST(ThreadPoolTest, AddTaskToRecreatedQueue) {
  ThreadPool pool;

  // Iniciar el pool de hilos
  pool.init();

  // Crear una nueva cola de tareas
  pool.new_queue(1, 1);

  // Eliminar la cola de tareas
  pool.remove_queue(1);

  // Crear nuevamente la cola de tareas
  pool.new_queue(1, 1);

  // Intentar agregar una tarea a la cola recreada
  ASSERT_NO_THROW({
    pool.add(
        1, [](void*) {}, nullptr);
  });
}

TEST(ThreadPoolTest, RemoveQueueAfterRecreate) {
  ThreadPool pool;

  // Crear una nueva cola de tareas
  pool.new_queue(1, 1);

  // Eliminar la cola de tareas
  pool.remove_queue(1);

  // Crear nuevamente la cola de tareas
  pool.new_queue(1, 1);

  // Intentar eliminar la cola recreada
  ASSERT_NO_THROW({ pool.remove_queue(1); });
}

TEST(ThreadPoolTest, WaitEmptyWithoutInit) {
  ThreadPool pool;

  // Intentar esperar hasta que el pool de hilos esté vacío sin iniciarlo
  ASSERT_NO_THROW({ pool.wait_empty(); });
}

TEST(ThreadPoolTest, AddTaskAfterStopAndInit) {
  ThreadPool pool;

  // Iniciar el pool de hilos
  pool.init();

  // Detener el pool de hilos
  pool.stop();

  // Intentar agregar una tarea después de detener y volver a iniciar el pool de hilos
  ASSERT_NO_THROW({
    pool.init();
    pool.add([](void*) {}, nullptr);
  });

  // Detener el pool de hilos nuevamente
  pool.stop();
}

TEST(ThreadPoolTest, AddTaskWithNullFunctionToNonexistentQueue) {
  ThreadPool pool;

  // Iniciar el pool de hilos
  pool.init();

  // Intentar agregar una tarea con función nula a una cola inexistente
  for (int i = 0; i < 10; ++i) {
    ASSERT_THROW({ pool.add(1, nullptr, nullptr); }, std::logic_error);
  }
}

TEST(ThreadPoolTest, AddAndRemoveMultipleQueues) {
  ThreadPool pool;

  // Agregar y eliminar múltiples colas de tareas
  ASSERT_NO_THROW({
    for (int i = 0; i < 20; ++i) {
      try {
        pool.new_queue(i, i + 1);
        pool.remove_queue(i);
      } catch (...) {
      }
    }
  });
}

TEST(ThreadPoolTest, AddTasksFromMultipleThreads) {
  ThreadPool pool;

  // Iniciar el pool de hilos
  pool.init();

  // Función para agregar tareas desde múltiples hilos
  auto addTasks = [&pool]() {
    for (int i = 0; i < 1000; ++i) {
      pool.add([](void*) {}, nullptr);
    }
  };

  // Crear múltiples hilos y agregar tareas desde cada hilo
  std::thread threads[10];
  for (int i = 0; i < 10; ++i) {
    threads[i] = std::thread(addTasks);
  }

  // Esperar a que todos los hilos terminen
  for (int i = 0; i < 10; ++i) {
    threads[i].join();
  }
}

TEST(ThreadPoolTest, AddTaskWithValidArgument) {
  ThreadPool pool;

  // Iniciar el pool de hilos para permitir que la tarea se ejecute
  pool.init();

  // Agregar una tarea con un argumento válido
  ASSERT_NO_THROW({
    int argument = 42;
    pool.add(
        [](void* arg) {
          int value = *static_cast<int*>(arg);
          ASSERT_EQ(value, 42);
        },
        &argument);
  });

  pool.stop();
}

TEST(ThreadPoolTest, AddTaskToStoppedPool) {
  ThreadPool pool;

  // Agregar una tarea con un argumento inválido
  for (int i = 0; i < 100; ++i) {
    ASSERT_THROW({ pool.add([](void*) {}, nullptr); }, std::logic_error);
  }
}

TEST(ThreadPoolTest, ClosePoolWithActiveTasks) {
  ThreadPool pool;

  // Iniciar el pool de hilos
  pool.init();

  pool.new_queue(1, 1);

  // Agregar una tarea con un argumento inválido
  for (int i = 0; i < 10000; ++i) {
    pool.add(
        1, [](void*) {}, nullptr);
  }
}

TEST(ThreadPoolTest, TryToCloseDefaultQueue) {
  ThreadPool pool;
  pool.init();
  for (int i = 0; i < 1000; ++i) {
    ASSERT_NO_THROW({ pool.remove_queue(0); });  // Intentar eliminar la cola de tareas predeterminada
  }
}
