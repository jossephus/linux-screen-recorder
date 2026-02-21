import 'dart:io';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'models/settings_model.dart';
import 'recorder_controller.dart';
import 'screens/home_screen.dart';
import 'screens/recordings_screen.dart';
import 'screens/settings_screen.dart';

class RecorderApp extends StatefulWidget {
  final bool initialPathCheck;
  final String initialPath;
  
  const RecorderApp({
    super.key,
    this.initialPathCheck = false,
    required this.initialPath,
  });

  @override
  State<RecorderApp> createState() => _RecorderAppState();
}

class _RecorderAppState extends State<RecorderApp> {
  late final RecorderController _controller;
  int _selectedIndex = 0;
  bool _isInitialized = false;
  String? _error;
  bool _isExpanded = false; // Non-nullable with default value
  
  // Screens for navigation
  static const List<Widget> _screens = [
    HomeScreen(),
    RecordingsScreen(),
    SettingsScreen(),
  ];
  
  static const List<String> _screenTitles = [
    'Home',
    'Recordings',
    'Settings',
  ];
  
  @override
  void initState() {
    super.initState();
    _controller = context.read<RecorderController>();
    _controller.addListener(_onStatusChanged);
    _controller.refreshStatus();
    
    // Set initial expanded state based on platform
    _isExpanded = true; // Default to expanded for desktop
    
    if (widget.initialPathCheck) {
      _checkAndCreateDirectory();
    } else {
      _isInitialized = true;
    }
  }
  
  Future<void> _checkAndCreateDirectory() async {
    try {
      final dir = Directory(widget.initialPath);
      final exists = await dir.exists();
      
      if (!exists) {
        if (!mounted) return;
        
        final shouldCreate = await showDialog<bool>(
          context: context,
          builder: (context) => AlertDialog(
            title: const Text('Create Directory'),
            content: Text('The directory ${widget.initialPath} does not exist. Would you like to create it?'),
            actions: [
              TextButton(
                onPressed: () => Navigator.of(context).pop(false),
                child: const Text('Cancel'),
              ),
              FilledButton(
                onPressed: () => Navigator.of(context).pop(true),
                child: const Text('Create'),
              ),
            ],
          ),
        ) ?? false;
        
        if (shouldCreate) {
          await dir.create(recursive: true);
          if (mounted) {
            final settings = context.read<SettingsModel>();
            await settings.updateSavePath(dir.path);
          }
        } else {
          if (mounted) {
            setState(() {
              _error = 'Cannot continue without a valid save directory';
            });
            return;
          }
        }
      } else {
        if (mounted) {
          final settings = context.read<SettingsModel>();
          await settings.updateSavePath(dir.path);
        }
      }
      
      if (mounted) {
        setState(() {
          _isInitialized = true;
        });
      }
    } catch (e) {
      if (mounted) {
        setState(() {
          _error = 'Error setting up save directory: $e';
        });
      }
    }
  }

  @override
  void dispose() {
    _controller.removeListener(_onStatusChanged);
    _controller.dispose();
    super.dispose();
  }

  void _onStatusChanged() {
    if (mounted) {
      setState(() {});
    }
  }

  @override
  Widget build(BuildContext context) {
    if (_error != null) {
      return MaterialApp(
        home: Scaffold(
          body: Center(
            child: Padding(
              padding: const EdgeInsets.all(16.0),
              child: Text(
                _error!,
                style: const TextStyle(color: Colors.red),
                textAlign: TextAlign.center,
              ),
            ),
          ),
        ),
      );
    }
    
    if (!_isInitialized) {
      return const MaterialApp(
        home: Scaffold(
          body: Center(
            child: CircularProgressIndicator(),
          ),
        ),
      );
    }
    
    // Use the current _isExpanded state (set in initState or by user toggle)

    return MaterialApp(
      title: 'Screen Recorder',
      theme: ThemeData(
        useMaterial3: true,
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.blue),
        navigationRailTheme: NavigationRailThemeData(
          backgroundColor: Theme.of(context).colorScheme.surface,
          indicatorColor: Theme.of(context).colorScheme.primaryContainer,
          selectedIconTheme: IconThemeData(
            color: Theme.of(context).colorScheme.onPrimaryContainer,
          ),
          selectedLabelTextStyle: TextStyle(
            color: Theme.of(context).colorScheme.onSurface,
            fontWeight: FontWeight.w500,
          ),
        ),
      ),
      home: Scaffold(
        body: MouseRegion(
          cursor: SystemMouseCursors.basic,
          child: LayoutBuilder(
            builder: (context, constraints) {
              return Row(
                children: [
                  // Navigation Rail with enhanced theming and hover effects
                  Theme(
                    data: Theme.of(context).copyWith(
                      highlightColor: Colors.transparent,
                      splashFactory: NoSplash.splashFactory,
                      hoverColor: Colors.grey[200],
                    ),
                    child: NavigationRail(
                      selectedIndex: _selectedIndex,
                      extended: _isExpanded,
                      minExtendedWidth: 180,
                      minWidth: 72,
                      elevation: 2,
                      backgroundColor: Theme.of(context).colorScheme.surface,
                      labelType: NavigationRailLabelType.none,
                      leading: _isExpanded 
                          ? Padding(
                              padding: const EdgeInsets.all(16.0),
                              child: Column(
                                children: [
                                  Container(
                                    padding: const EdgeInsets.all(8),
                                    decoration: BoxDecoration(
                                      color: Theme.of(context).colorScheme.primaryContainer,
                                      borderRadius: BorderRadius.circular(12),
                                    ),
                                    child: Icon(
                                      Icons.videocam,
                                      size: 32,
                                      color: Theme.of(context).colorScheme.onPrimaryContainer,
                                    ),
                                  ),
                                  const SizedBox(height: 12),
                                  Text(
                                    'Screen Recorder',
                                    style: Theme.of(context).textTheme.titleMedium?.copyWith(
                                      fontWeight: FontWeight.bold,
                                      color: Theme.of(context).colorScheme.onSurface,
                                    ),
                                    textAlign: TextAlign.center,
                                  ),
                                ],
                              ),
                            )
                          : null,
                      destinations: [
                        NavigationRailDestination(
                          icon: const Icon(Icons.home_outlined),
                          selectedIcon: const Icon(Icons.home),
                          label: const Text('Home'),
                        ),
                        NavigationRailDestination(
                          icon: const Icon(Icons.video_library_outlined),
                          selectedIcon: const Icon(Icons.video_library_rounded),
                          label: const Text('Recordings'),
                        ),
                        NavigationRailDestination(
                          icon: const Icon(Icons.settings_outlined),
                          selectedIcon: const Icon(Icons.settings),
                          label: const Text('Settings'),
                        ),
                      ],
                      trailing: _isExpanded 
                        ? IconButton(
                            icon: const Icon(Icons.arrow_back_ios_new_rounded, size: 16),
                            onPressed: () {
                              if (mounted) {
                                setState(() {
                                  _isExpanded = false;
                                });
                              }
                            },
                            tooltip: 'Collapse sidebar',
                          )
                        : IconButton(
                            icon: const Icon(Icons.arrow_forward_ios_rounded, size: 16),
                            onPressed: () {
                              if (mounted) {
                                setState(() {
                                  _isExpanded = true;
                                });
                              }
                            },
                            tooltip: 'Expand sidebar',
                          ),
                      onDestinationSelected: (index) {
                        if (mounted) {
                          setState(() {
                            _selectedIndex = index;
                          });
                        }
                      },
                    ),
                  ),
                  
                  // Vertical divider between rail and content
                  const VerticalDivider(thickness: 1, width: 1),
                  
                  // Main content area
                  Expanded(
                    child: Column(
                      children: [
                        // App Bar
                        Container(
                          height: kToolbarHeight,
                          padding: const EdgeInsets.symmetric(horizontal: 16),
                          decoration: BoxDecoration(
                            color: Theme.of(context).colorScheme.surface,
                            border: Border(
                              bottom: BorderSide(
                                color: Theme.of(context).dividerColor,
                                width: 1,
                              ),
                            ),
                          ),
                          child: Row(
                            children: [
                              // Toggle button for mobile
                              if (constraints.maxWidth < 600)
                                IconButton(
                                  icon: const Icon(Icons.menu),
                                  onPressed: () {
                                    if (mounted) {
                                      setState(() => _isExpanded = !_isExpanded);
                                    }
                                  },
                                  tooltip: _isExpanded ? 'Hide sidebar' : 'Show sidebar',
                                ),
                              
                              // Title
                              Text(
                                _screenTitles[_selectedIndex],
                                style: Theme.of(context).textTheme.titleLarge?.copyWith(
                                  fontWeight: FontWeight.w600,
                                ),
                              ),
                              
                              const Spacer(),
                            ],
                          ),
                        ),
                        
                        // Main content
                        Expanded(
                          child: _screens[_selectedIndex],
                        ),
                      ],
                    ),
                  ),
                ],
              );
            },
          ),
        ),
      ),
    );
  }
}
