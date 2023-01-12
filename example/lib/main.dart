import 'dart:ffi';
import 'dart:isolate';
import 'dart:typed_data';

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

class CppRequest {
  final SendPort? replyPort;
  final int? pendingCall;
  final String method;
  final Uint8List data;

  factory CppRequest.fromCppMessage(List message) {
    return CppRequest._(message[0], message[1], message[2], message[3]);
  }

  CppRequest._(this.replyPort, this.pendingCall, this.method, this.data);

  String toString() => 'CppRequest(method: $method, ${data.length} bytes)';
}

class CppResponse {
  final int pendingCall;
  final Uint8List data;

  CppResponse(this.pendingCall, this.data);

  List toCppMessage() => List.from([pendingCall, data], growable: false);

  String toString() => 'CppResponse(message: ${data.length})';
}

void handleCppRequests(dynamic message) {
  final CppRequest cppRequest = CppRequest.fromCppMessage(message);
  print('Got message: $cppRequest');

  http.get(Uri.parse('https://www.google.com')).then((response) {
    // Adding some additional delay.
    Future.delayed(Duration(seconds: 5), () {
      final Uint8List result = Uint8List.fromList(response.body.codeUnits);
      final CppResponse cppResponse =
          CppResponse(cppRequest.pendingCall!, result);
      print('Responding: $cppResponse');
      cppRequest.replyPort!.send(cppResponse.toCppMessage());
    });
  });
}

final dl = DynamicLibrary.process();

final registerSendPort = dl.lookupFunction<Void Function(Int64 sendPort),
    void Function(int sendPort)>('RegisterSendPort');

class Work extends Opaque {}
