import 'dart:ffi';
import 'dart:isolate';

import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;

import 'video_player.dart';

Future<void> main() async {
  runApp(MaterialApp(
    home: Scaffold(
      appBar: AppBar(
        title: const Text('Plugin example app'),
      ),
      body: Center(child: ButterFlyAssetVideo()),
    ),
  ));

  print("Dart: Setup.");
  final initializeApi = dl.lookupFunction<IntPtr Function(Pointer<Void>),
      int Function(Pointer<Void>)>("InitDartApiDL");
  initializeApi(NativeApi.initializeApiDLData);

  final interactiveCppRequests = ReceivePort()..listen(requestExecuteCallback);
  final int nativePort = interactiveCppRequests.sendPort.nativePort;
  registerCallback1(nativePort, callback1FP);

  // We need to yield control in order to be able to receive messages.
  while (callbackResult < 10) {
    print("Dart: Yielding (able to receive messages on port).");
    await Future.delayed(Duration(seconds: 1));
  }
  print("Dart: Received expected number of callbacks.");

  interactiveCppRequests.close();
  print("Dart: Done.");
}

int callbackResult = 0;

bool flag = false;

int callback1(int a) {
  print("Dart:     callback1($a).");
  if (!flag) {
    http.get(Uri.parse('https://www.google.com')).then((value) async {
      await Future.delayed(Duration(seconds: 5));
      callbackResult = value.body.length;
      print("Dart:   Set result to $callbackResult");
    });
    flag = true;
  }
  return a + callbackResult;
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

final executeCallback = dl.lookupFunction<Void Function(Pointer<Work>),
    void Function(Pointer<Work>)>('ExecuteCallback');

class Work extends Opaque {}
