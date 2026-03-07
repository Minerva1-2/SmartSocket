import 'dart:io';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import '../constants.dart';

class MqttService {
  MqttServerClient? _client;

  // 回调函数定义
  Function(String payload)? onDataReceived;
  Function()? onConnected;
  Function()? onDisconnected;

  // 【新增】提供给外部的主动状态查询属性
  bool get isConnected =>
      _client?.connectionStatus?.state == MqttConnectionState.connected;

  Future<void> connect() async {
    _client = MqttServerClient(AppConfig.broker, AppConfig.clientId);
    _client!.port = AppConfig.port;
    _client!.logging(on: false);
    _client!.setProtocolV311(); // 使用 MQTT 3.1.1 协议，提高兼容性和效率
    _client!.keepAlivePeriod = 30; // 调整心跳为 30秒，优化 NAT 保持
    _client!.autoReconnect = false; // 【修改】禁用库自带重连，由 AppProvider 统一接管重连逻辑

    _client!.secure = false;
    _client!.onConnected = _onConnected;
    _client!.onDisconnected = _onDisconnected;

    final connMessage = MqttConnectMessage()
        .authenticateAs(AppConfig.username, AppConfig.password)
        .withClientIdentifier(AppConfig.clientId)
        .startClean();
    _client!.connectionMessage = connMessage;

    try {
      print('MQTT: Connecting...');
      await _client!.connect();
    } on NoConnectionException catch (e) {
      print('MQTT: Connection failed - $e');
      _client!.disconnect();
    } on SocketException catch (e) {
      print('MQTT: Socket error - $e');
      _client!.disconnect();
    }

    if (_client!.connectionStatus!.state == MqttConnectionState.connected) {
      print('MQTT: Connected');
      _subscribe();
    }
  }

  void _subscribe() {
    // QoS 0 (atMostOnce) 降低确认延迟，加快数据流速
    _client!.subscribe(AppConfig.topic, MqttQos.atMostOnce);

    _client!.updates!.listen((List<MqttReceivedMessage<MqttMessage?>>? c) {
      if (c == null || c.isEmpty) return;

      // 优化策略: 只处理最新数据 (c.last)，消除排队延迟
      final MqttReceivedMessage<MqttMessage?> msg = c.last;

      final MqttPublishMessage recMess = msg.payload as MqttPublishMessage;
      final String payload = MqttPublishPayload.bytesToStringAsString(
        recMess.payload.message,
      );
      if (onDataReceived != null) {
        onDataReceived!(payload);
      }
    });
  }

  void publish(String topic, String message) {
    if (_client != null &&
        _client!.connectionStatus!.state == MqttConnectionState.connected) {
      final builder = MqttClientPayloadBuilder();
      builder.addString(message);
      // QoS 0 发送
      _client!.publishMessage(topic, MqttQos.atMostOnce, builder.payload!);
      // Removed print to reduce I/O delay
    } else {
      print("MQTT Error: Cannot publish, client not connected.");
    }
  }

  void _onConnected() {
    if (onConnected != null) onConnected!();
  }

  void _onDisconnected() {
    if (onDisconnected != null) onDisconnected!();
    print('MQTT: Disconnected');
  }

  void dispose() {
    _client?.disconnect();
  }
}
