import 'dart:io';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'models/settings_model.dart';
import 'recorder_controller.dart';
import 'recorder_service.dart';
import 'recorder_app.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  
  // Initialize settings model
  final settings = SettingsModel();
  
  // Set up default save path
  final videosDir = Directory('${Platform.environment['HOME']}/Videos/ScreenRecordings');
  
  // Check if directory exists, if not, we'll handle it in the app
  final bool dirExists = await videosDir.exists();
  if (dirExists) {
    await settings.updateSavePath(videosDir.path);
  }
  
  runApp(
    MultiProvider(
      providers: [
        ChangeNotifierProvider.value(value: settings),
        ChangeNotifierProvider(create: (_) => RecorderController(RecorderService())),
      ],
      child: RecorderApp(
        initialPathCheck: true, 
        initialPath: '${Platform.environment['HOME']}/Videos/ScreenRecordings'
      ),
    ),
  );
}