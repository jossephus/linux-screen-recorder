import 'package:flutter/foundation.dart';

import 'recorder_service.dart';

class RecorderController extends ChangeNotifier {
  RecorderController(this._service);

  final RecorderService _service;

  String _state = 'idle';
  String _message = '';
  bool _isRecording = false;
  bool _isBusy = false;
  String _error = '';

  String get state => _state;
  String get message => _message;
  bool get isRecording => _isRecording;
  bool get isBusy => _isBusy;
  String get error => _error;

  Future<void> start({
    required String path,
    int fps = 60,
    bool audio = false,
    String audioDevice = 'default',
    int outputHeight = 0,
  }) async {
    try {
      _isBusy = true;
      _error = '';
      notifyListeners();

      await _service.startRecording(
        path: path,
        fps: fps,
        audio: audio,
        audioDevice: audioDevice,
        outputHeight: outputHeight,
      );
      
      _isRecording = true;
      await refreshStatus();
    } catch (e) {
      _error = e.toString();
      rethrow;
    } finally {
      _isBusy = false;
      notifyListeners();
    }
  }

  Future<void> stop() async {
    try {
      _isBusy = true;
      _error = '';
      notifyListeners();

      await _service.stopRecording();
      _isRecording = false;
      await refreshStatus();
    } catch (e) {
      _error = e.toString();
      rethrow;
    } finally {
      _isBusy = false;
      notifyListeners();
    }
  }

  Future<void> refreshStatus() async {
    try {
      final status = await _service.getStatus();
      _state = status['state'] ?? 'unknown';
      _message = status['message'] ?? '';
      _isRecording = _state == 'recording';
    } catch (e) {
      _error = e.toString();
    } finally {
      notifyListeners();
    }
  }
}
