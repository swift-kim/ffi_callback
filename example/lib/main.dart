import 'dart:ffi';
import 'dart:isolate';

import 'package:flutter/material.dart';

Future<void> main() async {
  runApp(MaterialApp(
    home: Scaffold(
      appBar: AppBar(
        title: const Text('Plugin example app'),
      ),
      body: Center(),
    ),
  ));

  print("Dart: Setup.");
  final initializeApi = dl.lookupFunction<IntPtr Function(Pointer<Void>),
      int Function(Pointer<Void>)>("NativeInitializeDartApi");
  initializeApi(NativeApi.initializeApiDLData);

  final interactiveCppRequests = ReceivePort()..listen(requestExecuteCallback);
  final int nativePort = interactiveCppRequests.sendPort.nativePort;
  registerCallback1(nativePort, callback1FP);
  print("Dart: Tell C to start worker threads.");
  startWorkSimulator();

  // We need to yield control in order to be able to receive messages.
  while (numCallbacks1 < 1) {
    print("Dart: Yielding (able to receive messages on port).");
    await Future.delayed(Duration(seconds: 1));
  }
  print("Dart: Received expected number of callbacks.");

  interactiveCppRequests.close();
  print("Dart: Done.");
}

int numCallbacks1 = 0;

int callback1(int a) {
  print("Dart:     callback1($a).");
  numCallbacks1++;
  return a + 3;
}

void requestExecuteCallback(dynamic message) {
  final int work_address = message;
  final work = Pointer<Work>.fromAddress(work_address);
  print("Dart:   Calling into C to execute callback ($work).");
  executeCallback(work);
  print("Dart:   Done with callback.");
}

final callback1FP = Pointer.fromFunction<IntPtr Function(IntPtr)>(callback1, 0);

final dl = DynamicLibrary.process();

final registerCallback1 = dl.lookupFunction<
        Void Function(Int64 sendPort,
            Pointer<NativeFunction<IntPtr Function(IntPtr)>> functionPointer),
        void Function(int sendPort,
            Pointer<NativeFunction<IntPtr Function(IntPtr)>> functionPointer)>(
    'RegisterMyCallbackBlocking');

final startWorkSimulator =
    dl.lookupFunction<Void Function(), void Function()>('StartWorkSimulator');

final executeCallback = dl.lookupFunction<Void Function(Pointer<Work>),
    void Function(Pointer<Work>)>('ExecuteCallback');

class Work extends Opaque {}
