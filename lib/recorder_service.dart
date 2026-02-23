import 'package:flutter/services.dart';

class RecorderService {
  static const MethodChannel _channel = MethodChannel('screen_recorder');

  Future<void> startRecording({
    required String path,
    int fps = 60,
    bool audio = false,
    String audioDevice = 'default',
    int outputHeight = 0,
  }) async {
    await _channel.invokeMethod<void>('startRecording', <String, dynamic>{
      'path': path,
      'fps': fps,
      'audio': audio,
      'audioDevice': audioDevice,
      'outputHeight': outputHeight,
    });
  }

  Future<void> stopRecording() async {
    await _channel.invokeMethod<void>('stopRecording');
  }

  Future<Map<String, dynamic>> getStatus() async {
    final dynamic result = await _channel.invokeMethod<dynamic>('getStatus');
    if (result is Map) {
      return Map<String, dynamic>.from(result);
    }
    return <String, dynamic>{'state': 'unknown', 'message': 'Invalid status response'};
  }

  Future<String> getRecommendedAudioDevice() async {
    final dynamic result = await _channel.invokeMethod<dynamic>('getRecommendedAudioDevice');
    if (result is String && result.trim().isNotEmpty) {
      return result.trim();
    }
    return 'default';
  }

  Future<Map<String, int>> getDisplayResolution() async {
    final dynamic result = await _channel.invokeMethod<dynamic>('getDisplayResolution');
    if (result is Map) {
      final map = Map<String, dynamic>.from(result);
      final width = map['width'];
      final height = map['height'];
      if (width is int && height is int && width > 0 && height > 0) {
        return <String, int>{'width': width, 'height': height};
      }
    }
    return const <String, int>{'width': 1920, 'height': 1080};
  }
}
