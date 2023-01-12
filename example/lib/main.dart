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
  registerCallback(nativePort);
}

void requestExecuteCallback(dynamic message) {
  print('Calling into C to execute callback.');
  http.get(Uri.parse('https://www.google.com')).then((response) {
    // Adding some additional delay.
    Future.delayed(Duration(seconds: 2), () {
      int result = response.body.length;

      print('Calling into C to execute callback.');
      executeCallback(result);
      print('Done with callback.');
    });
  });
}

final dl = DynamicLibrary.process();

final registerCallback = dl.lookupFunction<Void Function(Int64 sendPort),
    void Function(int sendPort)>('RegisterMyCallbackBlocking');

final executeCallback =
    dl.lookupFunction<Void Function(IntPtr), void Function(int)>(
        'ExecuteCallback');

class Work extends Opaque {}
