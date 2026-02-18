import 'dart:async';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:path/path.dart' as path;
import 'package:path_provider/path_provider.dart';

class RecordingsScreen extends StatefulWidget {
  const RecordingsScreen({Key? key}) : super(key: key);

  @override
  _RecordingsScreenState createState() => _RecordingsScreenState();
}

class _RecordingsScreenState extends State<RecordingsScreen> {
  List<FileSystemEntity> _recordings = [];
  final Map<String, String?> _thumbnailCache = {};
  bool _isLoading = true;
  String _error = '';
  File? _pendingDeletionFile;
  
  @override
  void dispose() {
    _pendingDeletionFile?.delete();
    // Clean up thumbnail cache
    for (final thumbnailPath in _thumbnailCache.values) {
      if (thumbnailPath != null) {
        File(thumbnailPath).delete().catchError((_) {});
      }
    }
    super.dispose();
  }
  
  Future<String?> _getVideoThumbnail(String videoPath) async {
    if (_thumbnailCache.containsKey(videoPath)) {
      return _thumbnailCache[videoPath];
    }

    try {
      final tempDir = await getTemporaryDirectory();
      final thumbnailDir = Directory('${tempDir.path}/thumbnails');
      await thumbnailDir.create(recursive: true);
      
      final thumbnailPath = '${thumbnailDir.path}/${path.basenameWithoutExtension(videoPath)}.jpg';
      
      // Use FFmpeg to generate thumbnail with proper format
      final result = await Process.run('ffmpeg', [
        '-i', videoPath,
        '-ss', '00:00:00.5',  // Take a frame at 0.5 seconds (earlier in case video is short)
        '-vframes', '1',      // Take only 1 frame
        '-q:v', '2',          // Quality (2-31, lower is better)
        '-f', 'image2',       // Force output format to image
        '-vcodec', 'mjpeg',   // Use MJPEG codec
        '-y',                 // Overwrite output file if it exists
        thumbnailPath,
      ]);
      
      if (result.exitCode == 0 && await File(thumbnailPath).exists()) {
        _thumbnailCache[videoPath] = thumbnailPath;
        return thumbnailPath;
      } else {
        debugPrint('FFmpeg error: ${result.stderr}');
        return null;
      }
    } catch (e) {
      debugPrint('Error generating thumbnail: $e');
      return null;
    }
  }

  @override
  void initState() {
    super.initState();
    _loadRecordings();
  }

  Future<void> _loadRecordings() async {
    setState(() {
      _isLoading = true;
      _error = '';
    });

    try {
      final directory = Directory('${Platform.environment['HOME']}/Videos/ScreenRecordings');
      if (await directory.exists()) {
        final files = await directory.list().toList();
        setState(() {
          _recordings = files.whereType<File>().toList()
            ..sort((a, b) => b.statSync().modified.compareTo(a.statSync().modified));
        });
      }
    } catch (e) {
      setState(() {
        _error = 'Error loading recordings: $e';
      });
    } finally {
      if (mounted) {
        setState(() {
          _isLoading = false;
        });
      }
    }
  }

  Future<void> _confirmDelete(FileSystemEntity file) async {
    if (_pendingDeletionFile != null) return; // Prevent multiple deletions
    
    final shouldDelete = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Delete Recording'),
        content: const Text('Are you sure you want to delete this recording?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('CANCEL'),
          ),
          TextButton(
            onPressed: () => Navigator.of(context).pop(true),
            style: TextButton.styleFrom(foregroundColor: Colors.red),
            child: const Text('DELETE'),
          ),
        ],
      ),
    );

    if (shouldDelete == true && mounted) {
      setState(() => _pendingDeletionFile = file as File?);
      await _deleteRecording(file);
    }
  }

  Future<void> _deleteRecording(FileSystemEntity file, {bool permanent = false}) async {
    if (!permanent) {
      // Move to temporary location first
      final tempDir = await getTemporaryDirectory();
      final tempFile = File(path.join(tempDir.path, path.basename(file.path)));
      
      try {
        // Clear any existing snackbars
        if (mounted) {
          ScaffoldMessenger.of(context).clearSnackBars();
        }
        
        // If file exists in temp, delete it first
        if (await tempFile.exists()) {
          await tempFile.delete();
        }
        
        // Move to temp location
        await File(file.path).copy(tempFile.path);
        await file.delete();
        
        if (!mounted) return;
        
        debugPrint('Preparing to show undo snackbar');
        final scaffoldMessenger = ScaffoldMessenger.of(context);
        
        // Clear any existing snackbars first
        scaffoldMessenger.clearSnackBars();
        
        // Start a timer for the snackbar
        bool isUndoPressed = false;
        final timer = Timer(const Duration(seconds: 5), () async {
          if (isUndoPressed) return;
          debugPrint('5 seconds passed, removing snackbar');
          scaffoldMessenger.hideCurrentSnackBar();
          
          if (await tempFile.exists()) {
            debugPrint('Deleting temp file after timeout: ${tempFile.path}');
            await tempFile.delete();
            
            if (mounted) {
              debugPrint('Showing deletion confirmation');
              scaffoldMessenger.showSnackBar(
                const SnackBar(
                  content: Text('Recording permanently deleted'),
                  duration: Duration(seconds: 2),
                  behavior: SnackBarBehavior.floating,
                  margin: EdgeInsets.all(8),
                ),
              );
              await _loadRecordings();
            }
          }
          
          if (mounted) {
            setState(() => _pendingDeletionFile = null);
          }
        });
        
        // Show the undo snackbar
        debugPrint('Showing undo snackbar at: ${DateTime.now()}');
        scaffoldMessenger.showSnackBar(
          SnackBar(
            content: const Text('Recording moved to trash'),
            action: SnackBarAction(
              label: 'UNDO',
              textColor: Colors.amber,
              onPressed: () async {
                isUndoPressed = true;
                timer.cancel();
                debugPrint('UNDO button pressed');
                
                if (await tempFile.exists()) {
                  debugPrint('Restoring file from: ${tempFile.path} to ${file.path}');
                  await tempFile.copy(file.path);
                  await tempFile.delete();
                  debugPrint('File restored successfully');
                  if (mounted) {
                    await _loadRecordings();
                  }
                } else {
                  debugPrint('Temp file does not exist, cannot restore');
                }
              },
            ),
            duration: const Duration(seconds: 10), // Longer duration as fallback
            behavior: SnackBarBehavior.floating,
            margin: const EdgeInsets.all(8),
          ),
        );
        
        // Log when the snackbar is shown
        debugPrint('Snackbar shown at: ${DateTime.now()}');
        debugPrint('Snackbar should auto-dismiss at: ${DateTime.now().add(const Duration(seconds: 5))}');
      } catch (e) {
        // If anything fails, try to restore the original file
        if (await tempFile.exists() && !await file.exists()) {
          await tempFile.copy(file.path);
          await tempFile.delete();
        }
        
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Error deleting recording: $e')),
          );
        }
      }
    } else {
      // Permanent deletion (from temp)
      try {
        await file.delete();
      } catch (e) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Error deleting recording: $e')),
          );
        }
      }
    }
  }

  String _formatFileSize(int bytes) {
    if (bytes < 1024) return '$bytes B';
    if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(1)} KB';
    return '${(bytes / (1024 * 1024)).toStringAsFixed(1)} MB';
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Recordings'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: _loadRecordings,
          ),
        ],
      ),
      body: _isLoading
          ? const Center(child: CircularProgressIndicator())
          : _error.isNotEmpty
              ? Center(child: Text(_error))
              : _recordings.isEmpty
                  ? const Center(child: Text('No recordings found'))
                  : ListView.builder(
                      itemCount: _recordings.length,
                      itemBuilder: (context, index) {
                        final file = _recordings[index];
                        final stat = file.statSync();
                        final size = _formatFileSize(stat.size);
                        final date = stat.modified.toString().substring(0, 19);
                        final fileName = path.basename(file.path);

                        return Card(
                          margin: const EdgeInsets.symmetric(
                            horizontal: 8,
                            vertical: 4,
                          ),
                          child: ListTile(
                            leading: FutureBuilder<String?>(
                              future: _getVideoThumbnail(file.path),
                              builder: (context, snapshot) {
                                if (snapshot.connectionState == ConnectionState.done && 
                                    snapshot.hasData && 
                                    snapshot.data != null) {
                                  return Container(
                                    width: 60,
                                    height: 60,
                                    decoration: BoxDecoration(
                                      borderRadius: BorderRadius.circular(4),
                                      image: DecorationImage(
                                        image: FileImage(File(snapshot.data!)),
                                        fit: BoxFit.cover,
                                      ),
                                    ),
                                  );
                                }
                                return Container(
                                  width: 60,
                                  height: 60,
                                  decoration: BoxDecoration(
                                    color: Colors.grey[300],
                                    borderRadius: BorderRadius.circular(4),
                                  ),
                                  child: const Icon(Icons.videocam, size: 30, color: Colors.grey),
                                );
                              },
                            ),
                            title: Text(
                              fileName,
                              overflow: TextOverflow.ellipsis,
                            ),
                            subtitle: Text('$date • $size'),
                            onTap: () {
                              Process.run('xdg-open', [file.path]);
                            },
                            trailing: IconButton(
                              icon: const Icon(Icons.delete, color: Colors.red),
                              onPressed: _pendingDeletionFile == null 
                                  ? () => _confirmDelete(file)
                                  : null,
                            ),
                          ),
                        );
                      },
                    ),
    );
  }
}
