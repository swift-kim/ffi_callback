#include "ffi_callback_plugin.h"

#include <device/callback.h>
#include <flutter/plugin_registrar.h>
#include <unistd.h>

#include <condition_variable>
#include <memory>
#include <mutex>

#include "dart_api_dl.h"

namespace {

Dart_Port my_callback_blocking_send_port_;

// Notify Dart through a port that the C lib has pending async callbacks.
//
// The `send_port` should be from the isolate which registered the callback.
void NotifyDart(Dart_Port send_port) {
  printf("C   : Posting message.\n");

  Dart_CObject dart_object;
  dart_object.type = Dart_CObject_kNull;

  const bool result = Dart_PostCObject_DL(send_port, &dart_object);
  if (!result) {
    printf("C   : Posting message to port failed.\n");
  }
}

intptr_t result_ = 0;
std::condition_variable cv;

// Do a callback to Dart in a blocking way, being interested in the result.
intptr_t MyCallbackBlocking() {
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);
  printf("C   : Waiting for result.\n");
  NotifyDart(my_callback_blocking_send_port_);
  while (result_ == 0) {
    cv.wait(lock);
  }
  printf("C   : Received result: %d\n", result_);
  return result_;
}

}  // namespace

DART_EXPORT FLUTTER_PLUGIN_EXPORT intptr_t InitDartApiDL(void *data) {
  return Dart_InitializeApiDL(data);
}

DART_EXPORT FLUTTER_PLUGIN_EXPORT void RegisterMyCallbackBlocking(
    Dart_Port send_port) {
  my_callback_blocking_send_port_ = send_port;

  // Add a callback to be run by the platform thread when the battery charging
  // status changes. The status can be changed in the emulator control panel.
  int ret = device_add_callback(
      DEVICE_CALLBACK_BATTERY_CHARGING,
      [](device_callback_e type, void *value, void *user_data) {
        MyCallbackBlocking();
      },
      nullptr);
  if (ret != DEVICE_ERROR_NONE) {
    printf("C Da: Failed to add callback: %s\n", get_error_message(ret));
  }
}

DART_EXPORT FLUTTER_PLUGIN_EXPORT void ExecuteCallback(intptr_t result) {
  printf("C Da: ExecuteCallback.\n");
  result_ = result;
  cv.notify_one();
  printf("C Da: ExecuteCallback done.\n");
}

void FfiCallbackPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {}
