#include "ffi_callback_plugin.h"

#include <device/battery.h>
#include <device/callback.h>
#include <flutter/plugin_registrar.h>

#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include "dart_api_dl.h"

namespace {

intptr_t (*my_callback_blocking_fp_)(intptr_t);
Dart_Port my_callback_blocking_send_port_;

typedef std::function<void()> Work;

// Notify Dart through a port that the C lib has pending async callbacks.
//
// Expects heap allocated `work` so delete can be called on it.
//
// The `send_port` should be from the isolate which registered the callback.
void NotifyDart(Dart_Port send_port, const Work *work) {
  const intptr_t work_addr = reinterpret_cast<intptr_t>(work);
  printf("C   :  Posting message.\n");

  Dart_CObject dart_object;
  dart_object.type = Dart_CObject_kInt64;
  dart_object.value.as_int64 = work_addr;

  const bool result = Dart_PostCObject_DL(send_port, &dart_object);
  if (!result) {
    printf("C   :  Posting message to port failed.\n");
  }
}

// Do a callback to Dart in a blocking way, being interested in the result.
//
// Dart returns `a + 3`.
intptr_t MyCallbackBlocking(intptr_t a) {
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);
  intptr_t result;
  auto callback = my_callback_blocking_fp_;  // Define storage duration.
  std::condition_variable cv;
  bool notified = false;
  const Work work = [a, &result, callback, &cv, &notified]() {
    result = callback(a);
    printf("C Da:     Notify result ready: %d\n", result);
    if (result > 10) {
      notified = true;
    }
    cv.notify_one();
  };
  const Work *work_ptr = new Work(work);  // Copy to heap.
  printf("C   :  Waiting for result...\n");
  while (!notified) {
    NotifyDart(my_callback_blocking_send_port_, work_ptr);
    cv.wait(lock);
  }
  printf("C   :  Received result: %d\n", result);
  delete work_ptr;
  return result;
}

// Simulated work for Thread #1.
//
// Simulates heavy work with sleeps.
void Work1() {
  printf("C T1: Work1 Start.\n");
  const intptr_t val1 = 3;
  const intptr_t val2 = MyCallbackBlocking(val1);  // val2 = 6.
  printf("C T1: MyCallbackBlocking returned %d.\n", val2);
}

}  // namespace

DART_EXPORT FLUTTER_PLUGIN_EXPORT intptr_t InitDartApiDL(void *data) {
  return Dart_InitializeApiDL(data);
}

DART_EXPORT FLUTTER_PLUGIN_EXPORT void RegisterMyCallbackBlocking(
    Dart_Port send_port, intptr_t (*callback1)(intptr_t)) {
  my_callback_blocking_fp_ = callback1;
  my_callback_blocking_send_port_ = send_port;

  int ret = device_add_callback(
      DEVICE_CALLBACK_BATTERY_CHARGING,
      [](device_callback_e type, void *value, void *user_data) { Work1(); },
      nullptr);
  if (ret != DEVICE_ERROR_NONE) {
    printf("C Da: Failed to add callback: %s", get_error_message(ret));
  }
}

DART_EXPORT FLUTTER_PLUGIN_EXPORT void ExecuteCallback(Work *work_ptr) {
  printf("C Da:    ExecuteCallback.\n");
  const Work work = *work_ptr;
  work();
  printf("C Da:    ExecuteCallback done.\n");
}

void FfiCallbackPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  std::thread::id this_id = std::this_thread::get_id();
  std::cout << "C Pt:  Thread " << this_id << " ...\n";
}
