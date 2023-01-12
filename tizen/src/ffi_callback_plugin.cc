#include "ffi_callback_plugin.h"

#include <device/callback.h>
#include <flutter/plugin_registrar.h>
#include <unistd.h>

#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>

#include "dart_api_dl.h"

namespace {

Dart_Port send_port_;

static void FreeFinalizer(void*, void* value) { free(value); }

class PendingCall {
 public:
  PendingCall(void** buffer, size_t* length)
      : response_buffer_(buffer), response_length_(length) {
    receive_port_ =
        Dart_NewNativePort_DL("cpp-response", &PendingCall::HandleResponse,
                              /*handle_concurrently=*/false);
  }
  ~PendingCall() { Dart_CloseNativePort_DL(receive_port_); }

  Dart_Port port() const { return receive_port_; }

  void PostAndWait(Dart_Port port, Dart_CObject* object) {
    std::unique_lock<std::mutex> lock(mutex);
    const bool success = Dart_PostCObject_DL(send_port_, object);
    if (!success) {
      printf("C   : Failed to send message, invalid port or isolate died.\n");
    }

    printf("C   : Waiting for result.\n");
    while (!notified) {
      cv.wait(lock);
    }
  }

  static void HandleResponse(Dart_Port p, Dart_CObject* message) {
    if (message->type != Dart_CObject_kArray) {
      printf("C   : Wrong Data: message->type != Dart_CObject_kArray.\n");
    }
    Dart_CObject** c_response_args = message->value.as_array.values;
    Dart_CObject* c_pending_call = c_response_args[0];
    Dart_CObject* c_message = c_response_args[1];
    printf("C   : HandleResponse (call: %d).\n",
           reinterpret_cast<intptr_t>(c_pending_call));

    auto pending_call = reinterpret_cast<PendingCall*>(
        c_pending_call->type == Dart_CObject_kInt64
            ? c_pending_call->value.as_int64
            : c_pending_call->value.as_int32);

    pending_call->ResolveCall(c_message);
  }

 private:
  static bool NonEmptyBuffer(void** value) { return *value != nullptr; }

  void ResolveCall(Dart_CObject* bytes) {
    assert(bytes->type == Dart_CObject_kTypedData);
    if (bytes->type != Dart_CObject_kTypedData) {
      printf("C   : Wrong Data: bytes->type != Dart_CObject_kTypedData.\n");
    }
    const intptr_t response_length = bytes->value.as_typed_data.length;
    const uint8_t* response_buffer = bytes->value.as_typed_data.values;
    printf("C   : ResolveCall(length: %d).\n", response_length);

    void* buffer = malloc(response_length);
    memmove(buffer, response_buffer, response_length);

    *response_buffer_ = buffer;
    *response_length_ = response_length;

    printf("C   : Notify result ready.\n");
    notified = true;
    cv.notify_one();
  }

  std::mutex mutex;
  std::condition_variable cv;
  bool notified = false;

  Dart_Port receive_port_;
  void** response_buffer_;
  size_t* response_length_;
};

// Do a callback to Dart in a blocking way, being interested in the result.
intptr_t MyCallback1(uint8_t a) {
  const char* methodname = "myCallback1";

  size_t request_length = sizeof(uint8_t) * 1;
  void* request_buffer = malloc(request_length);      // FreeFinalizer.
  reinterpret_cast<uint8_t*>(request_buffer)[0] = a;  // Populate buffer.
  void* response_buffer = nullptr;
  size_t response_length = 0;

  PendingCall pending_call(&response_buffer, &response_length);

  Dart_CObject c_send_port;
  c_send_port.type = Dart_CObject_kSendPort;
  c_send_port.value.as_send_port.id = pending_call.port();
  c_send_port.value.as_send_port.origin_id = ILLEGAL_PORT;

  Dart_CObject c_pending_call;
  c_pending_call.type = Dart_CObject_kInt64;
  c_pending_call.value.as_int64 = reinterpret_cast<int64_t>(&pending_call);

  Dart_CObject c_method_name;
  c_method_name.type = Dart_CObject_kString;
  c_method_name.value.as_string = const_cast<char*>(methodname);

  Dart_CObject c_request_data;
  c_request_data.type = Dart_CObject_kExternalTypedData;
  c_request_data.value.as_external_typed_data.type = Dart_TypedData_kUint8;
  c_request_data.value.as_external_typed_data.length = request_length;
  c_request_data.value.as_external_typed_data.data =
      static_cast<uint8_t*>(request_buffer);
  c_request_data.value.as_external_typed_data.peer = request_buffer;
  c_request_data.value.as_external_typed_data.callback = FreeFinalizer;

  Dart_CObject* c_request_arr[] = {&c_send_port, &c_pending_call,
                                   &c_method_name, &c_request_data};
  Dart_CObject c_request;
  c_request.type = Dart_CObject_kArray;
  c_request.value.as_array.values = c_request_arr;
  c_request.value.as_array.length =
      sizeof(c_request_arr) / sizeof(c_request_arr[0]);

  printf("C   : Dart_PostCObject_(request: %d).\n",
         reinterpret_cast<intptr_t>(&c_request));
  pending_call.PostAndWait(send_port_, &c_request);
  printf("C   : Received result.\n");

  // Returns the length of the response body.
  const intptr_t result = response_length;
  free(response_buffer);

  return result;
}

}  // namespace

DART_EXPORT FLUTTER_PLUGIN_EXPORT intptr_t InitDartApiDL(void* data) {
  return Dart_InitializeApiDL(data);
}

DART_EXPORT FLUTTER_PLUGIN_EXPORT void RegisterSendPort(Dart_Port send_port) {
  send_port_ = send_port;

  // Add a callback to be run by the platform thread when the battery charging
  // status changes. The status can be changed in the emulator control panel.
  int ret = device_add_callback(
      DEVICE_CALLBACK_BATTERY_CHARGING,
      [](device_callback_e type, void* value, void* user_data) {
        const intptr_t val1 = 3;
        const intptr_t val2 = MyCallback1(val1);
        printf("C T1: MyCallback1 returned %d.\n", val2);
      },
      nullptr);
  if (ret != DEVICE_ERROR_NONE) {
    printf("C Da: Failed to add callback: %s\n", get_error_message(ret));
  }
}

void FfiCallbackPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {}
