import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

class SettingsModel extends ChangeNotifier {
  static const String _fpsKey = 'fps';
  static const String _audioEnabledKey = 'audio_enabled';
  static const String _savePathKey = 'save_path';

  int _fps = 60;
  bool _audioEnabled = true;
  String _savePath = '';

  int get fps => _fps;
  bool get audioEnabled => _audioEnabled;
  String get savePath => _savePath;

  SettingsModel() {
    _initSettings();
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
}
