import 'package:flutter/material.dart';

class AppColors {
  static const Color background = Color(0xFF001F3F);
  static const Color cardBg = Color(0xFF003366);
  static const Color accent = Color(0xFF39CCCC);
  static const Color textPrimary = Colors.white;
  static const Color textSecondary = Color(0xFFAAAAAA);
  static const Color alert = Color(0xFFFF4136);
  static const Color success = Color(0xFF2ECC40);
}

class AppConfig {
  static const String broker = '8.137.168.209';
  static const int port = 1883;

  // 订阅数据的 Topic (STM32 -> App)
  static const String topic = 'home/sensor/data';

  // 【新增】发送命令的 Topic (App -> STM32)
  // 请确保 STM32 订阅了这个 Topic
  static const String topicCmd = 'home/sensor/cmd';

  static const String clientId = 'flutter_app_client_001';
  static const String username = 'admin';
  static const String password = '123';

  static const double defaultPowerLimit = 2000.0;
}
