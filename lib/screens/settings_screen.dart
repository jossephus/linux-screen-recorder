import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';
import 'package:provider/provider.dart';
import '../models/settings_model.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({Key? key}) : super(key: key);

  @override
  _SettingsScreenState createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  late TextEditingController _fpsController;
  late TextEditingController _pathController;
  bool _isSaving = false;
  bool _isLoading = true;

  @override
  void initState() {
    super.initState();
    _initializeControllers();
  }

  Future<void> _initializeControllers() async {
    final settings = context.read<SettingsModel>();
    _fpsController = TextEditingController(text: settings.fps.toString());
    _pathController = TextEditingController(text: settings.savePath);
    setState(() => _isLoading = false);
  }

  @override
  void dispose() {
    _fpsController.dispose();
    _pathController.dispose();
    super.dispose();
  }

  Future<void> _applyChanges(SettingsModel settings) async {
    setState(() => _isSaving = true);
    
    try {
      // Update FPS if changed
      final newFps = int.tryParse(_fpsController.text) ?? settings.fps;
      if (newFps != settings.fps) {
        await settings.updateFps(newFps);
      }
      
      // Update save path if changed
      if (_pathController.text != settings.savePath) {
        await settings.updateSavePath(_pathController.text);
      }
      
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Settings saved')),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Error saving settings: $e')),
        );
      }
    } finally {
      if (mounted) {
        setState(() => _isSaving = false);
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    if (_isLoading) {
      return const Scaffold(
        body: Center(child: CircularProgressIndicator()),
      );
    }

    return Scaffold(
      appBar: AppBar(
        title: const Text('Settings'),
      ),
      body: Stack(
        children: [
          // Main content
          Consumer<SettingsModel>(
            builder: (context, settings, _) {
              return ListView(
                padding: const EdgeInsets.all(16.0),
                children: [
                  Card(
                    child: Padding(
                      padding: const EdgeInsets.all(16.0),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text(
                            'Recording Settings',
                            style: TextStyle(
                              fontSize: 18,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                          const SizedBox(height: 16),
                          TextField(
                            controller: _fpsController,
                            decoration: const InputDecoration(
                              labelText: 'FPS',
                              border: OutlineInputBorder(),
                              helperText: 'Frames per second for recording',
                            ),
                            keyboardType: TextInputType.number,
                          ),
                          const SizedBox(height: 16),
                          Row(
                            children: [
                              Expanded(
                                child: TextField(
                                  controller: _pathController,
                                  decoration: const InputDecoration(
                                    labelText: 'Save Location',
                                    border: OutlineInputBorder(),
                                  ),
                                  readOnly: true,
                                ),
                              ),
                              IconButton(
                                icon: const Icon(Icons.folder_open),
                                onPressed: () async {
                                  final directory = await getApplicationDocumentsDirectory();
                                  if (mounted) {
                                    setState(() {
                                      _pathController.text = directory.path;
                                    });
                                  }
                                },
                              ),
                            ],
                          ),
                          const SizedBox(height: 16),
                          SwitchListTile(
                            title: const Text('Enable Audio Recording'),
                            value: settings.audioEnabled,
                            onChanged: (value) {
                              settings.updateAudioEnabled(value);
                            },
                            secondary: Icon(
                              settings.audioEnabled ? Icons.mic : Icons.mic_off,
                              color: settings.audioEnabled ? Colors.green : Colors.grey,
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ],
              );
            },
          ),
          
          // Apply button in bottom right
          Positioned(
            right: 16,
            bottom: 16,
            child: ElevatedButton(
              onPressed: _isSaving 
                  ? null 
                  : () => _applyChanges(context.read<SettingsModel>()),
              style: ElevatedButton.styleFrom(
                foregroundColor: Colors.white,
                backgroundColor: Theme.of(context).primaryColor,
                padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(4),
                ),
                elevation: 2,
              ),
              child: _isSaving
                  ? const SizedBox(
                      width: 20,
                      height: 20,
                      child: CircularProgressIndicator(
                        strokeWidth: 2,
                        valueColor: AlwaysStoppedAnimation<Color>(Colors.white),
                      ),
                    )
                  : const Text(
                      'Apply Changes',
                      style: TextStyle(fontWeight: FontWeight.bold),
                    ),
            ),
          ),
        ],
      ),
    );
  }
}
