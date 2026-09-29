#include "services/shared.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace services {

namespace {

SemaphoreHandle_t s_mutex = nullptr;

}  // namespace

void sharedInit() {
  if (s_mutex == nullptr) {
    s_mutex = xSemaphoreCreateRecursiveMutex();
  }
}

SharedLock::SharedLock() {
  sharedInit();
  xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
}

SharedLock::~SharedLock() { xSemaphoreGiveRecursive(s_mutex); }

}  // namespace services
