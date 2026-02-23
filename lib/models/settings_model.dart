import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

class SettingsModel extends ChangeNotifier {
  static const String _fpsKey = 'fps';
  static const String _audioEnabledKey = 'audio_enabled';
  static const String _savePathKey = 'save_path';
  static const String _legacyResolutionScaleKey = 'resolution_scale_percent';
  static const String _outputResolutionHeightKey = 'output_resolution_height';
  static const String _audioDeviceKey = 'audio_device';

  int _fps = 60;
  bool _audioEnabled = true;
  String _savePath = '';
  int _displayWidth = 1920;
  int _displayHeight = 1080;
  int _outputResolutionHeight = 0; // 0 means native.
  String _audioDevice = 'auto';
  late final Future<void> ready;

  int get fps => _fps;
  bool get audioEnabled => _audioEnabled;
  String get savePath => _savePath;
  int get displayWidth => _displayWidth;
  int get displayHeight => _displayHeight;
  int get outputResolutionHeight => _outputResolutionHeight;
  String get audioDevice => _audioDevice;

  List<int> get availableResolutionHeights {
    const knownHeights = <int>[2160, 1080, 720, 480];
    final filtered = knownHeights.where((h) => h <= _displayHeight).toList();
    if (_displayHeight < 480) {
      return const <int>[];
    }
    return filtered;
  }

  String get outputResolutionLabel {
    if (_outputResolutionHeight <= 0 || _outputResolutionHeight >= _displayHeight) {
      return 'Native (${_displayWidth}x$_displayHeight)';
    }
    final width = ((_displayWidth * _outputResolutionHeight) / _displayHeight).round();
    final evenWidth = (width ~/ 2) * 2;
    return '${evenWidth}x$_outputResolutionHeight (${_outputResolutionHeight}p)';
  }

  double get outputToDisplayRatio {
    if (_outputResolutionHeight <= 0 || _displayHeight <= 0) {
      return 1.0;
    }
    return _outputResolutionHeight / _displayHeight;
  }

  SettingsModel() {
    ready = _initSettings();
  }

  Future<void> _initSettings() async {
    // Set default save path if not set
    final videosDir = Directory('${Platform.environment['HOME']}/Videos/ScreenRec');
    if (!await videosDir.exists()) {
      await videosDir.create(recursive: true);
    }
    
    await _loadSettings();
    
    // If save path is not set, use the default one
    if (_savePath.isEmpty) {
      _savePath = videosDir.path;
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString(_savePathKey, _savePath);
      notifyListeners();
    }
  }

  Future<void> _loadSettings() async {
    final prefs = await SharedPreferences.getInstance();
    _fps = prefs.getInt(_fpsKey) ?? 30;
    _audioEnabled = prefs.getBool(_audioEnabledKey) ?? true;
    _savePath = prefs.getString(_savePathKey) ?? '';
    _outputResolutionHeight = prefs.getInt(_outputResolutionHeightKey) ?? 0;
    final legacyScale = prefs.getInt(_legacyResolutionScaleKey);
    if (_outputResolutionHeight == 0 && legacyScale != null && legacyScale > 0 && legacyScale < 100) {
      _outputResolutionHeight = ((1080 * legacyScale) / 100).round();
      if (_outputResolutionHeight > 0) {
        await prefs.setInt(_outputResolutionHeightKey, _outputResolutionHeight);
      }
    }
    _audioDevice = prefs.getString(_audioDeviceKey) ?? 'auto';
    _normalizeOutputResolutionHeight();
    notifyListeners();
  }

  Future<void> updateFps(int value) async {
    _fps = value;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setInt(_fpsKey, value);
    notifyListeners();
  }

  Future<void> updateAudioEnabled(bool value) async {
    _audioEnabled = value;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setBool(_audioEnabledKey, value);
    notifyListeners();
  }

  Future<void> updateSavePath(String path) async {
    _savePath = path;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_savePathKey, path);
    notifyListeners();
  }

  Future<void> updateOutputResolutionHeight(int value) async {
    _outputResolutionHeight = value;
    _normalizeOutputResolutionHeight();
    final prefs = await SharedPreferences.getInstance();
    await prefs.setInt(_outputResolutionHeightKey, _outputResolutionHeight);
    notifyListeners();
  }

  Future<void> updateAudioDevice(String value) async {
    _audioDevice = value.trim().isEmpty ? 'auto' : value.trim();
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_audioDeviceKey, _audioDevice);
    notifyListeners();
  }

  void updateDisplayResolution(int width, int height) {
    if (width <= 0 || height <= 0) {
      return;
    }
    _displayWidth = width;
    _displayHeight = height;
    _normalizeOutputResolutionHeight();
    notifyListeners();
  }

  void _normalizeOutputResolutionHeight() {
    if (_displayHeight <= 0) {
      _outputResolutionHeight = 0;
      return;
    }
    if (_outputResolutionHeight >= _displayHeight) {
      _outputResolutionHeight = 0;
      return;
    }
    if (_displayHeight >= 480 && _outputResolutionHeight > 0 && _outputResolutionHeight < 480) {
      _outputResolutionHeight = 480;
      return;
    }
    if (_displayHeight < 480 && _outputResolutionHeight > 0) {
      _outputResolutionHeight = 0;
      return;
    }
    if (_outputResolutionHeight <= 0) {
      return;
    }
    final allowed = availableResolutionHeights;
    if (allowed.contains(_outputResolutionHeight)) {
      return;
    }
    if (allowed.isEmpty) {
      _outputResolutionHeight = 0;
      return;
    }
    int best = allowed.first;
    int bestDistance = (_outputResolutionHeight - best).abs();
    for (final h in allowed.skip(1)) {
      final distance = (_outputResolutionHeight - h).abs();
      if (distance < bestDistance) {
        best = h;
        bestDistance = distance;
      }
    }
    _outputResolutionHeight = best;
  }
}
