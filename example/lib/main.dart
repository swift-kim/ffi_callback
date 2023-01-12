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

  print('Dart: Setup.');
  final initializeApi = dl.lookupFunction<IntPtr Function(Pointer<Void>),
      int Function(Pointer<Void>)>('InitDartApiDL');
  initializeApi(NativeApi.initializeApiDLData);

  final interactiveCppRequests = ReceivePort()..listen(requestExecuteCallback);
  final int nativePort = interactiveCppRequests.sendPort.nativePort;
  registerCallback(nativePort, callbackFP);
}

bool ongoing = false;

int result = 0;

int callback() {
  if (!ongoing) {
    print('Sending a request.');
    http.get(Uri.parse('https://www.google.com')).then((response) {
      // Adding some additional delay.
      Future.delayed(Duration(seconds: 2), () {
        result = response.body.length;
        print('The result is $result.');
      });
    });
    ongoing = true;
  }
  return result;
}

void requestExecuteCallback(dynamic message) {
  final int work_address = message;
  final work = Pointer<Work>.fromAddress(work_address);
  print('Calling into C to execute callback.');
  executeCallback(work);
  print('Done with callback.');
}

final callbackFP = Pointer.fromFunction<IntPtr Function()>(callback, 0);

final dl = DynamicLibrary.process();

final registerCallback = dl.lookupFunction<
        Void Function(Int64 sendPort,
            Pointer<NativeFunction<IntPtr Function()>> functionPointer),
        void Function(int sendPort,
            Pointer<NativeFunction<IntPtr Function()>> functionPointer)>(
    'RegisterMyCallbackBlocking');

final executeCallback = dl.lookupFunction<Void Function(Pointer<Work>),
    void Function(Pointer<Work>)>('ExecuteCallback');

class Work extends Opaque {}
