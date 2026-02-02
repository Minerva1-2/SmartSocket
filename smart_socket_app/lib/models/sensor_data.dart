class SensorData {
  // PZEM-004T 数据
  double voltage;
  double current;
  double power;
  double energy;

  // DHT11 数据
  double temperature;
  double humidity;

  // DS18B20 数据
  double waterTemp;

  SensorData({
    this.voltage = 0.0,
    this.current = 0.0,
    this.power = 0.0,
    this.energy = 0.0,
    this.temperature = 0.0,
    this.humidity = 0.0,
    this.waterTemp = 0.0,
  });

  // 核心逻辑：增量更新
  // 根据 freertos.c 中 sprintf 生成的 JSON Key 进行匹配
  void updateFromMap(Map<String, dynamic> json) {
    // 1. 解析 PZEM 数据
    if (json.containsKey('Voltage')) voltage = _toDouble(json['Voltage']);
    if (json.containsKey('Current')) current = _toDouble(json['Current']);
    if (json.containsKey('Power')) power = _toDouble(json['Power']);
    if (json.containsKey('Energy')) energy = _toDouble(json['Energy']);

    // 2. 解析 DHT11 数据
    if (json.containsKey('Temperature'))
      temperature = _toDouble(json['Temperature']);
    if (json.containsKey('Humidity')) humidity = _toDouble(json['Humidity']);

    // 3. 解析 DS18B20 数据
    if (json.containsKey('WaterTemp')) waterTemp = _toDouble(json['WaterTemp']);
  }

  // 辅助函数：安全转 double (防止 int 转 double 报错)
  double _toDouble(dynamic val) {
    if (val is int) return val.toDouble();
    if (val is double) return val;
    return 0.0;
  }
}
