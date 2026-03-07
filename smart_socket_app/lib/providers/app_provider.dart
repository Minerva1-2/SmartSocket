import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:flutter_ringtone_player/flutter_ringtone_player.dart';
import '../models/sensor_data.dart';
import '../services/mqtt_service.dart';
import '../constants.dart';

enum AppStatus { disconnected, connecting, connected }

class AppProvider with ChangeNotifier {
  final MqttService _mqttService = MqttService();
  SensorData data = SensorData();
  AppStatus status = AppStatus.disconnected;

  // 默认功率限制
  double powerLimit = AppConfig.defaultPowerLimit;

  // 【新增】继电器状态 (默认通电)
  bool isRelayOn = true;

  // 报警状态标志
  bool _isPowerAlertShowing = false;

  // 断线计数器，用于控制重连频率
  int _disconnectTicks = 0;

  // 定时检查连接状态的 Timer
  Timer? _statusCheckTimer;

  BuildContext? _context;

  AppProvider() {
    _loadSettings();
    _initMqtt();
    _startStatusCheck();
  }

  @override
  void dispose() {
    _statusCheckTimer?.cancel();
    super.dispose();
  }

  void setContext(BuildContext context) {
    _context = context;
  }

  // 加载设置
  Future<void> _loadSettings() async {
    final prefs = await SharedPreferences.getInstance();
    powerLimit = prefs.getDouble('powerLimit') ?? AppConfig.defaultPowerLimit;
    notifyListeners();
  }

  // 保存功率限制
  Future<void> setPowerLimit(double value) async {
    powerLimit = value;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setDouble('powerLimit', value);
    notifyListeners();
  }

  // 初始化 MQTT
  void _initMqtt() {
    status = AppStatus.connecting;
    notifyListeners();

    _mqttService.onConnected = () {
      status = AppStatus.connected;
      print("APP: MQTT Connected to ${AppConfig.broker}");

      // 可选：连接后同步一次状态，或者默认保持 App 端的 isRelayOn
      notifyListeners();
    };

    _mqttService.onDisconnected = () {
      status = AppStatus.disconnected;
      notifyListeners();
      _handleDisconnection();
    };

    _mqttService.onDataReceived = (payload) {
      try {
        Map<String, dynamic> jsonMap = jsonDecode(payload);

        // 更新数据模型
        data.updateFromMap(jsonMap);

        // 检查功率限制 (含自动断电逻辑)
        _checkPowerLimit();

        // 刷新 UI
        notifyListeners();
      } catch (e) {
        print('JSON Parse Error: $e');
      }
    };

    _mqttService.connect();
  }

  // 启动周期性状态检查 (双重保障 + 自动重连心跳)
  void _startStatusCheck() {
    _statusCheckTimer = Timer.periodic(const Duration(seconds: 2), (timer) {
      final bool realConnected = _mqttService.isConnected;

      // 0. 如果当前正在连接中 (Connecting)，暂时跳过检查，给它一点时间
      if (status == AppStatus.connecting) {
        // 可以加个超时判断，防止一直卡在 connecting，这里暂且简单处理
        return;
      }

      // 1. 如果实际已连接，但 App 状态不是 Connected -> 修正为 Connected
      if (realConnected && status != AppStatus.connected) {
        print(
          "APP Check: Detected Connected but status was $status. Fixing...",
        );
        status = AppStatus.connected;
        _disconnectTicks = 0; // 重置计数器
        notifyListeners();
      }

      // 2. 如果实际已断开，但 App 状态是 Connected -> 立即修正为 Disconnected
      if (!realConnected && status == AppStatus.connected) {
        print(
          "APP Check: Detected Disconnected but status was Connected. Fixing...",
        );
        status = AppStatus.disconnected;
        notifyListeners();
        _handleDisconnection();
      }

      // 3. 【核心修复】如果处于断开状态，定期尝试重连 (接管 autoReconnect)
      if (!realConnected && status == AppStatus.disconnected) {
        _disconnectTicks++;
        // 每 3 个 tick (约 6秒) 尝试一次重连
        if (_disconnectTicks >= 3) {
          print("APP: Auto-reconnecting...");
          _disconnectTicks = 0;
          _mqttService.connect(); // 发起重连
          // 注意：connect 是 async 的，但我们这里不需要 await，让它后台跑即可
          status = AppStatus.connecting; // 切换状态 UI 为连接中
          notifyListeners();
        }
      }
    });
  }

  // 【新增】控制继电器开关
  void toggleRelay(bool targetState) {
    isRelayOn = targetState;
    notifyListeners();

    // 组装 JSON 指令: {"relay": 1} 或 {"relay": 0}
    Map<String, dynamic> cmd = {"relay": targetState ? 1 : 0};

    // 通过 MQTT 发送给 STM32 (Topic: home/sensor/cmd)
    // 注意：需要确保你的 mqtt_service.dart 中已经添加了 publish 方法
    _mqttService.publish(AppConfig.topicCmd, jsonEncode(cmd));
  }

  // 核心检测逻辑
  void _checkPowerLimit() {
    // 1. 如果功率恢复正常，直接返回
    if (data.power <= powerLimit) {
      return;
    }

    // --- 功率超标处理 ---

    // 逻辑变更: 只有在【当前是通电状态】时，才触发保护和报警
    if (isRelayOn) {
      print("APP: 严重警告！功率超标，触发自动断电保护！");

      // 1. 强制断电
      toggleRelay(false);

      // 2. 触发报警弹窗 (只在从通电->断电的瞬间触发一次)
      // 检查是否已经在显示警告，防止重复叠加
      if (!_isPowerAlertShowing && _context != null) {
        _isPowerAlertShowing = true;
        _triggerAlarm(
          "过载保护触发",
          "检测到功率 ${data.power}W 超过限制 ($powerLimit W)！\n\n为了安全，已自动切断插座电源。\n请检查电器后手动开启。",
          isPower: true,
        );
      }
    }

    // 如果当前已经是【断电状态】，则不进行任何操作 (即不报警)
  }

  // 断线处理
  void _handleDisconnection() {
    // 根据用户需求，不论切换为何种状态都不要出现弹窗
    // 这里仅打印日志，重连逻辑主要由 _startStatusCheck 周期性任务负责
    print("APP: MQTT Disconnected. Silent mode (no popup).");
  }

  // 通用报警弹窗
  void _triggerAlarm(String title, String content, {required bool isPower}) {
    FlutterRingtonePlayer.playAlarm();

    showDialog(
      context: _context!,
      barrierDismissible: false,
      builder: (ctx) => AlertDialog(
        backgroundColor: AppColors.cardBg,
        title: Row(
          children: [
            const Icon(Icons.warning, color: AppColors.alert),
            const SizedBox(width: 10),
            Text(
              title,
              style: const TextStyle(color: Colors.white, fontSize: 18),
            ),
          ],
        ),
        content: Text(content, style: const TextStyle(color: Colors.white70)),
        actions: [
          TextButton(
            onPressed: () {
              FlutterRingtonePlayer.stop();
              Navigator.of(ctx).pop();

              if (isPower) {
                _isPowerAlertShowing = false;
              }
            },
            child: const Text(
              "我知道了",
              style: TextStyle(color: AppColors.accent),
            ),
          ),
        ],
      ),
    );
  }
}
