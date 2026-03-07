import 'package:flutter/material.dart';
import 'package:flutter/cupertino.dart'; // 用于 iOS 风格的顺滑开关
import 'package:provider/provider.dart';
import 'package:font_awesome_flutter/font_awesome_flutter.dart';
import '../providers/app_provider.dart';
import '../widgets/sensor_card.dart';
import '../constants.dart';

class DashboardScreen extends StatefulWidget {
  const DashboardScreen({super.key});

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  @override
  void initState() {
    super.initState();
    // 绑定 Context 给 Provider，用于显示全局报警弹窗
    WidgetsBinding.instance.addPostFrameCallback((_) {
      Provider.of<AppProvider>(context, listen: false).setContext(context);
    });
  }

  @override
  Widget build(BuildContext context) {
    // 获取状态源
    final appProvider = Provider.of<AppProvider>(context);
    final data = appProvider.data;

    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        backgroundColor: AppColors.background,
        elevation: 0,
        title: const Text(
          "智能插座",
          style: TextStyle(fontWeight: FontWeight.bold, letterSpacing: 1.5),
        ),
        actions: [
          // 顶部：在线状态指示灯
          Container(
            margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
            decoration: BoxDecoration(
              color: appProvider.status == AppStatus.connected
                  ? AppColors.success.withOpacity(0.2)
                  : AppColors.alert.withOpacity(0.2),
              borderRadius: BorderRadius.circular(20),
              border: Border.all(
                color: appProvider.status == AppStatus.connected
                    ? AppColors.success
                    : AppColors.alert,
              ),
            ),
            child: Row(
              children: [
                Icon(
                  Icons.circle,
                  size: 10,
                  color: appProvider.status == AppStatus.connected
                      ? AppColors.success
                      : AppColors.alert,
                ),
                const SizedBox(width: 6),
                Text(
                  appProvider.status == AppStatus.connected
                      ? "ONLINE"
                      : "OFFLINE",
                  style: TextStyle(
                    fontSize: 12,
                    color: appProvider.status == AppStatus.connected
                        ? AppColors.success
                        : AppColors.alert,
                  ),
                ),
              ],
            ),
          ),
          // 顶部：设置按钮
          IconButton(
            icon: const Icon(Icons.settings, color: AppColors.textSecondary),
            onPressed: () => _showLimitSetting(context, appProvider),
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            // ===============================================
            // 顶部大卡片：总功率 + 继电器开关
            // ===============================================
            Container(
              padding: const EdgeInsets.all(20),
              decoration: BoxDecoration(
                // 动态背景：通电时亮蓝渐变，断电时深灰渐变
                gradient: LinearGradient(
                  colors: appProvider.isRelayOn
                      ? [AppColors.accent.withOpacity(0.2), AppColors.cardBg]
                      : [Colors.grey.withOpacity(0.2), const Color(0xFF111111)],
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                ),
                borderRadius: BorderRadius.circular(20),
                border: Border.all(
                  color: appProvider.isRelayOn
                      ? AppColors.accent.withOpacity(0.3)
                      : Colors.grey.withOpacity(0.3),
                ),
              ),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  // 左侧：功率数值显示
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const Text(
                        "当前总功率",
                        style: TextStyle(color: AppColors.textSecondary),
                      ),
                      const SizedBox(height: 5),
                      Text(
                        "${data.power.toStringAsFixed(1)} W",
                        style: TextStyle(
                          fontSize: 36,
                          fontWeight: FontWeight.bold,
                          // 断电时数字变灰，通电时亮白
                          color: appProvider.isRelayOn
                              ? Colors.white
                              : Colors.grey,
                        ),
                      ),
                    ],
                  ),

                  // 右侧：继电器控制开关
                  Column(
                    children: [
                      Transform.scale(
                        scale: 1.2, // 放大开关组件
                        child: CupertinoSwitch(
                          value: appProvider.isRelayOn,
                          activeColor: AppColors.accent,
                          trackColor: Colors.black26,
                          onChanged: (bool value) {
                            // 调用 Provider 中的方法发送 MQTT 指令
                            appProvider.toggleRelay(value);
                          },
                        ),
                      ),
                      const SizedBox(height: 5),
                      Text(
                        appProvider.isRelayOn ? "已通电" : "已断电",
                        style: TextStyle(
                          fontSize: 12,
                          color: appProvider.isRelayOn
                              ? AppColors.accent
                              : Colors.grey,
                        ),
                      ),
                    ],
                  ),
                ],
              ),
            ),
            const SizedBox(height: 20),

            // ===============================================
            // 下方：详细传感器数据网格
            // ===============================================
            Expanded(
              child: GridView.count(
                crossAxisCount: 2,
                crossAxisSpacing: 16,
                mainAxisSpacing: 16,
                childAspectRatio: 1.5, // 卡片宽高比
                children: [
                  // 1. 电压
                  SensorCard(
                    title: "电压",
                    value: data.voltage.toStringAsFixed(1),
                    unit: "V",
                    icon: FontAwesomeIcons.plug,
                    iconColor: Colors.orangeAccent,
                  ),
                  // 2. 电流
                  SensorCard(
                    title: "电流",
                    value: data.current.toStringAsFixed(3),
                    unit: "A",
                    icon: FontAwesomeIcons.waveSquare,
                    iconColor: Colors.yellowAccent,
                  ),
                  // 3. 环境温度
                  SensorCard(
                    title: "环境温度",
                    value: data.temperature.toStringAsFixed(1),
                    unit: "°C",
                    icon: FontAwesomeIcons.temperatureHalf,
                    iconColor: Colors.redAccent,
                  ),
                  // 4. 环境湿度
                  SensorCard(
                    title: "环境湿度",
                    value: data.humidity.toStringAsFixed(1),
                    unit: "%",
                    icon: FontAwesomeIcons.droplet,
                    iconColor: Colors.blueAccent,
                  ),
                  // 5. 插座温度 (原 DS18B20 水温)
                  SensorCard(
                    title: "插座温度",
                    value: data.waterTemp.toStringAsFixed(2),
                    unit: "°C",
                    icon: FontAwesomeIcons.temperatureHigh, // 换成高温图标
                    iconColor: Colors.deepOrangeAccent, // 换成深橙色
                  ),
                  // 6. 累计用电
                  SensorCard(
                    title: "累计用电",
                    value: (data.energy / 1000).toStringAsFixed(2), // Wh -> kWh
                    unit: "kWh",
                    icon: FontAwesomeIcons.leaf,
                    iconColor: Colors.greenAccent,
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  // 功率限制设置弹窗
  void _showLimitSetting(BuildContext context, AppProvider provider) {
    final controller = TextEditingController(
      text: provider.powerLimit.toString(),
    );
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: AppColors.cardBg,
        title: const Text("设置功率限制", style: TextStyle(color: Colors.white)),
        content: TextField(
          controller: controller,
          keyboardType: TextInputType.number,
          style: const TextStyle(color: Colors.white),
          decoration: const InputDecoration(
            suffixText: "W",
            enabledBorder: UnderlineInputBorder(
              borderSide: BorderSide(color: AppColors.accent),
            ),
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text("取消"),
          ),
          TextButton(
            onPressed: () {
              final val = double.tryParse(controller.text);
              if (val != null) {
                provider.setPowerLimit(val);
                Navigator.pop(ctx);
              }
            },
            child: const Text("保存", style: TextStyle(color: AppColors.accent)),
          ),
        ],
      ),
    );
  }
}
