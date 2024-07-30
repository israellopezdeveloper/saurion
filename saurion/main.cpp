// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

#include "low_saurion.h"

#define PORT 8080

// Variable para controlar la finalización del bucle de espera
std::atomic<bool> running(true);

// Manejador de señal para SIGINT
void handle_sigint(int) { running.store(false); }

int main() {
  printf("Hello, saurion!\n");
  struct saurion *saurion = saurion_create(3);
  if (!saurion) {
    return -1;
  }
  saurion->ss = set_socket(PORT);
  saurion->cb.on_connected = [](int sfd, void *) -> void { printf("<%d> <-> saurion\n", sfd); };
  saurion->cb.on_readed = [](int sfd, const char *const msg, const ssize_t size, void *) -> void {
    printf("<%d> <- %s[%zd]\n", sfd, msg, size);
  };
  saurion->cb.on_wrote = [](int sfd, void *) -> void { printf("<%d> -> saurion\n", sfd); };
  saurion->cb.on_closed = [](int sfd, void *) -> void { printf("<%d>   saurion\n", sfd); };
  saurion->cb.on_error = [](int sfd, const char *const msg, const ssize_t size, void *) -> void {
    printf("<%d> <!-- %s[%zd]\n", sfd, msg, size);
  };
  saurion_start(saurion);
  std::signal(SIGINT, handle_sigint);
  while (running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  saurion_stop(saurion);
  saurion_destroy(saurion);

  return 0;
}
