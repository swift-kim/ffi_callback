#include "ffi_callback_plugin.h"

#include <device/callback.h>
#include <flutter/plugin_registrar.h>
#include <unistd.h>

#include <condition_variable>
#include <memory>
#include <mutex>

#include "dart_api_dl.h"

namespace {

intptr_t (*my_callback_blocking_fp_)();
Dart_Port my_callback_blocking_send_port_;

typedef std::function<void()> Work;

// Notify Dart through a port that the C lib has pending async callbacks.
//
// Expects heap allocated `work` so delete can be called on it.
//
// The `send_port` should be from the isolate which registered the callback.
void NotifyDart(Dart_Port send_port, const Work *work) {
  const intptr_t work_addr = reinterpret_cast<intptr_t>(work);
  printf("C   : Posting message.\n");

  Dart_CObject dart_object;
  dart_object.type = Dart_CObject_kInt64;
  dart_object.value.as_int64 = work_addr;

  const bool result = Dart_PostCObject_DL(send_port, &dart_object);
  if (!result) {
    printf("C   : Posting message to port failed.\n");
  }
}

// Do a callback to Dart in a blocking way, being interested in the result.
intptr_t MyCallbackBlocking() {
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);
  intptr_t result = 0;
  auto callback = my_callback_blocking_fp_;
  std::condition_variable cv;
  bool done = false;
  const Work work = [&result, callback, &cv, &done]() {
    result = callback();
    printf("C Da: Notify result ready: %d\n", result);
    if (result > 0) {
      done = true;
    }
    cv.notify_one();
  };
  const Work *work_ptr = new Work(work);
  printf("C   : Waiting for result.\n");
  while (!done) {
    NotifyDart(my_callback_blocking_send_port_, work_ptr);
    cv.wait(lock);
  }
  printf("C   : Received result: %d\n", result);
  delete work_ptr;
  return result;
}

}  // namespace

DART_EXPORT FLUTTER_PLUGIN_EXPORT intptr_t InitDartApiDL(void *data) {
  return Dart_InitializeApiDL(data);
}

DART_EXPORT FLUTTER_PLUGIN_EXPORT void RegisterMyCallbackBlocking(
    Dart_Port send_port, intptr_t (*callback1)()) {
  my_callback_blocking_fp_ = callback1;
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
    printf("C Da: Failed to add callback: %s", get_error_message(ret));
  }
}

DART_EXPORT FLUTTER_PLUGIN_EXPORT void ExecuteCallback(Work *work_ptr) {
  printf("C Da: ExecuteCallback.\n");
  const Work work = *work_ptr;
  work();
  printf("C Da: ExecuteCallback done.\n");
}

void FfiCallbackPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {}
