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

  final interactiveCppRequests = ReceivePort()..listen(handleCppRequests);
  final int nativePort = interactiveCppRequests.sendPort.nativePort;
  registerSendPort(nativePort);
}

void handleCppRequests(dynamic message) {
  print('Sending a request.');
  http.get(Uri.parse('https://www.google.com')).then((response) {
    // Adding some additional delay.
    Future.delayed(Duration(seconds: 5), () {
      print('Received a response.');
      int result = response.body.length;
      sendReply(result);
    });
  });
}

final dl = DynamicLibrary.process();

final registerSendPort = dl.lookupFunction<Void Function(Int64 sendPort),
    void Function(int sendPort)>('RegisterSendPort');

final sendReply =
    dl.lookupFunction<Void Function(IntPtr), void Function(int)>('SendReply');

class Work extends Opaque {}
