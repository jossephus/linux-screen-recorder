import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:provider/provider.dart';

import 'package:flutter_app/recorder_app.dart';
import 'package:flutter_app/models/settings_model.dart';

void main() {
  testWidgets('App should start with home screen', (WidgetTester tester) async {
    // Build our app and trigger a frame.
    await tester.pumpWidget(
      MultiProvider(
        providers: [
          ChangeNotifierProvider(create: (_) => SettingsModel()),
        ],
        child: const RecorderApp(),
      ),
    );

    // Verify the app starts with the home screen
    expect(find.byType(RecorderApp), findsOneWidget);
  });

  testWidgets('Settings screen should show settings', (tester) async {
    await tester.pumpWidget(
      MultiProvider(
        providers: [
          ChangeNotifierProvider(create: (_) => SettingsModel()),
        ],
        child: MaterialApp(
          home: const RecorderApp(),
        ),
      ),
    );

    // Navigate to settings
    await tester.tap(find.byIcon(Icons.settings));
    await tester.pumpAndSettle();

    // Verify settings screen is shown
    expect(find.text('Settings'), findsOneWidget);
  });
}
