import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'providers/app_provider.dart';
import 'screens/dashboard_screen.dart';
import 'constants.dart';

void main() {
  runApp(const SmartSocketApp());
}

class SmartSocketApp extends StatelessWidget {
  const SmartSocketApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [ChangeNotifierProvider(create: (_) => AppProvider())],
      child: MaterialApp(
        title: 'STM32 Monitor',
        debugShowCheckedModeBanner: false,
        theme: ThemeData(
          brightness: Brightness.dark,
          scaffoldBackgroundColor: AppColors.background,
          primaryColor: AppColors.background,
          fontFamily: 'Roboto', // 推荐使用等宽字体
          colorScheme: const ColorScheme.dark(
            primary: AppColors.accent,
            secondary: AppColors.accent,
          ),
        ),
        home: const DashboardScreen(),
      ),
    );
  }
}
