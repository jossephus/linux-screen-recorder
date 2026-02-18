import 'package:flutter/services.dart';

class RecorderService {
  static const MethodChannel _channel = MethodChannel('screen_recorder');

  Future<void> startRecording({
    required String path,
    int fps = 60,
    bool audio = false,
    String audioDevice = 'default',
  }) async {
    await _channel.invokeMethod<void>('startRecording', <String, dynamic>{
      'path': path,
      'fps': fps,
      'audio': audio,
      'audioDevice': audioDevice,
    });
  }

  Future<void> stopRecording() async {
    await _channel.invokeMethod<void>('stopRecording');
  }

  Future<Map<String, dynamic>> getStatus() async {
    final dynamic result = await _channel.invokeMethod<dynamic>('getStatus');
    if (result is Map) {
      return Map<String, dynamic>.from(result as Map<dynamic, dynamic>);
    }
    return <String, dynamic>{'state': 'unknown', 'message': 'Invalid status response'};
  }
}
