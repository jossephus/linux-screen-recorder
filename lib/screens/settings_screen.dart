import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';
import 'package:provider/provider.dart';
import '../models/settings_model.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  late TextEditingController _fpsController;
  late TextEditingController _pathController;
  int _outputResolutionHeight = 0;
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
    _outputResolutionHeight = settings.outputResolutionHeight;
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

      if (_outputResolutionHeight != settings.outputResolutionHeight) {
        await settings.updateOutputResolutionHeight(_outputResolutionHeight);
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
                        spacing: 16,
                        children: [
                          const Text(
                            'Recording Settings',
                            style: TextStyle(
                              fontSize: 18,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                          TextField(
                            controller: _fpsController,
                            decoration: const InputDecoration(
                              labelText: 'FPS',
                              border: OutlineInputBorder(),
                              helperText: 'Frames per second for recording',
                            ),
                            keyboardType: TextInputType.number,
                          ),
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
                          DropdownButtonFormField<int>(
                            initialValue: _outputResolutionHeight,
                            decoration: InputDecoration(
                              labelText: 'Output Resolution',
                              border: OutlineInputBorder(),
                              helperText:
                                  'Presets from native display (${settings.displayWidth}x${settings.displayHeight}) down to 480p',
                            ),
                            items: [
                              DropdownMenuItem<int>(
                                value: 0,
                                child: Text('Native (${settings.displayWidth}x${settings.displayHeight})'),
                              ),
                              ...settings.availableResolutionHeights.map(
                                (height) => DropdownMenuItem<int>(
                                  value: height,
                                  child: Text(_formatPresetLabel(height, settings)),
                                ),
                              ),
                            ],
                            onChanged: (value) {
                              if (value == null) {
                                return;
                              }
                              setState(() {
                                _outputResolutionHeight = value;
                              });
                            },
                          ),
                          const Text(
                            'When capturing a rectangular region smaller than the preset, the recorder will keep the region size and will not upscale.',
                            style: TextStyle(
                              color: Colors.blueGrey,
                              fontSize: 14,
                              fontWeight: FontWeight.w600,
                            ),
                          ),
                          if (_showLowQualityWarning(settings))
                            const Text(
                              'Warning: this output is much smaller than your screen (<=60%). Small text and UI details will look pixelated and blurred.',
                              style: TextStyle(color: Colors.orange, fontWeight: FontWeight.w600),
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

  String _formatPresetLabel(int targetHeight, SettingsModel settings) {
    final width = ((settings.displayWidth * targetHeight) / settings.displayHeight).round();
    final evenWidth = (width ~/ 2) * 2;
    return '${evenWidth}x$targetHeight (${targetHeight}p)';
  }

  bool _showLowQualityWarning(SettingsModel settings) {
    if (_outputResolutionHeight <= 0 || settings.displayHeight <= 0) {
      return false;
    }
    final ratio = _outputResolutionHeight / settings.displayHeight;
    return ratio <= 0.6;
  }
}
