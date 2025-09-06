  #include <Wire.h>
  #include <WiFi.h>
  #include <ArduinoJson.h>
  #include <PubSubClient.h>
  #include <Adafruit_NeoPixel.h>
  #include <Adafruit_Sensor.h>
  #include <DHT.h>
  #include <DHT_U.h>
  #include <WiFiManager.h> 
  #include <ESP32Servo.h>


  #define DHTPIN 5          // DHT 传感器连接到 GPIO 2
  #define DHTTYPE DHT11     // 使用 DHT11 传感器
  #define DHT_readtime  2000 // DHT11 读数时间间隔2000ms
  #define Light_time  1000   // 光照读取时间
  // 声明舵机对象
  Servo myservo;
  // 0度 与 180 度脉宽
  #define SERVO_MIN_PULSE_WIDTH 500 // 最小脉宽
  #define SERVO_MAX_PULSE_WIDTH 2500
  //舵机引脚3
  int servoPin = 3;

  //电机控制引脚
  int motorPin = 8;
  #define MOTOR_PWM_FREQ 5000
  #define MOTOR_PWM_CHANNEL 0
  //分辨率
  #define MOTOR_RESOLUTION 8

  //声明对象
  DHT_Unified dht(DHTPIN, DHTTYPE);

  //声明Adafruit_NeoPixel对象
  Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, 48, NEO_GRB + NEO_KHZ800);



  //MQTT服务器
  const char* mqttServer = "b7be77c3c5.st1.iotda-device.cn-north-4.myhuaweicloud.com";
  const int  mqttPort = 1883;
  const char* clientId = "68aaf089d582f20018488886_esp32_0_0_2025082411";
  const char* mqttUser = "68aaf089d582f20018488886_esp32";
  const char* mqttPassword = "38649b2c302e4ad27b2887b19d00a34eac0ffd7e4fa5bdc6dbdab075a3bd9a3f";

  //设备 上报 到 平台
  const char* PublishTopic = "$oc/devices/68aaf089d582f20018488886_esp32/sys/properties/report";
  //平台 下发 到 设备
  const char* SubscribeTopic = "$oc/devices/68aaf089d582f20018488886_esp32/sys/messages/down";

  float temperature = 0;
  float humidity = 0;
  uint  light_level;
  //RGB颜色
  String RGB_Color = "0 0 0";
  int RGB_red;
  int RGB_green;
  int RGB_blue;
  bool servo_auto = false;     // 舵机自动模式 true为自动模式 否则为手动模式
  bool servo_on = false;       // 舵机开启状态 ture为开启 否则为关闭
  int motor_speed = 0;         //电机速度

  WiFiClient espClient; 
  PubSubClient client(espClient);

  unsigned long previousMillis = 0;  // 存储上次发布数据的时间
  const long interval = 6000;        // 发布间隔为3秒


  void WIFI_Init();
  void connectMQTTserver();
  void publishAttributes();
  void subscribeTopic();
  void receiveCallback(char* topic, byte* payload, unsigned int length); 
  void read_dht11(float *temperature,float *humidity);
  void read_light_value(uint *light_level);
  void myservo_control();
  void motor_control(int speed);

  void setup() {
    //初始化strip
    strip.begin();
    strip.show();
    Serial.begin(9600);
    //初始化舵机
    ESP32PWM::allocateTimer(2);
    myservo.setPeriodHertz(50);
    myservo.attach(servoPin,SERVO_MIN_PULSE_WIDTH,SERVO_MAX_PULSE_WIDTH);
    myservo.write(0);

    //初始化电机
    ledcSetup(motorPin, MOTOR_PWM_FREQ, MOTOR_RESOLUTION);
    ledcAttachPin(motorPin, MOTOR_PWM_CHANNEL);
    ledcWrite(motorPin, 0);
    //初始化DHT11
    dht.begin();
    //初始化ADC
    //设置ADC读分辨率
    analogReadResolution(12);
    //配置输入衰减
    analogSetAttenuation(ADC_11db);

    WIFI_Init();
    //MQTT服务器连接部分
    client.setKeepAlive (60); //设置心跳时间
    client.setServer(mqttServer, mqttPort); //设置连接到MQTT服务器的参数
    client.setCallback(receiveCallback);
    connectMQTTserver();
  }
  
  void loop() {
    read_dht11(&temperature, &humidity);
    read_light_value(&light_level);
    myservo_control();
    motor_control(motor_speed);
    if (client.connected()) { // 如果开发板成功连接服务器
      client.loop();          // 处理信息以及心跳
      publishAttributes();   // 发布温度数据
    } else {                      // 如果开发板未能成功连接服务器
      connectMQTTserver();        // 则尝试连接服务器
    }
  }

  void WIFI_Init()
  {
    // 建立WiFiManager对象
    WiFiManager wifiManager;
    // 自动连接WiFi。以下语句的参数是连接ESP8266时的WiFi名称
    wifiManager.autoConnect("AutoConnectAP");
    //打印连接wifi名称
    Serial.println("Connected to ");
    //打印wifi名称
    Serial.println(WiFi.SSID());
  }

  void connectMQTTserver(){
    if (client.connect(clientId, mqttUser, mqttPassword )) {
      Serial.println("SUCCESS connected huaweiyun mqtt");  
      subscribeTopic();
    } else {
      Serial.print("FAILED with state ");
      Serial.print(client.state());
      delay(6000);
    }
  }

  //订阅下发主题
  void subscribeTopic()
  {
    if (client.subscribe(SubscribeTopic)) {
      Serial.println("SUCCESS subscribe topic");
    } else {
      Serial.println("FAILED subscribe topic");
    }
  }
  //发布属性信息
  void publishAttributes()
  {
    unsigned long currentMillis = millis();
    
    // 每3秒执行一次
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      // 创建JSON文档
      JsonDocument    doc;  
      JsonObject root = doc.to<JsonObject>();
      // 创建services数组
      JsonArray services = root["services"].to<JsonArray>();   
      // 创建service对象
      JsonObject service = services.add<JsonObject>(); 
      service["service_id"] = "esp32";    
      // 创建properties对象
      JsonObject properties = service["properties"].to<JsonObject>();
      properties["temperature"] = temperature;
      properties["humidity"] = humidity;
      properties["RGB_Color"] = RGB_Color;
      properties["Light_level"] = light_level;
      properties["servo_auto"] = servo_auto;
      properties["servo_on"] = servo_on;
      properties["motor_speed"] = motor_speed;
      // 序列化JSON
      char jsonBuffer[1024];
      serializeJson(doc, jsonBuffer);    
      // 发布到MQTT主题
      client.publish(PublishTopic, jsonBuffer);    
      // 打印日志
      Serial.print("JSON: ");
      Serial.println(jsonBuffer);
    }
  }

  // 收到信息后的回调函数

  void receiveCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message Received [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println("");
    Serial.print("Message Length(Bytes) ");
    Serial.println(length);

    // 将接收到的消息转换为字符串
    char message[length + 1];
    for (int i = 0; i < length; i++) {
      message[i] = (char)payload[i];
    }
    message[length] = '\0';
    JsonDocument    doc;  
    deserializeJson(doc, message);
    String RGB_message = doc["content"]["RGB"].as<String>();
    servo_auto = doc["content"]["servo_auto"].as<bool>();
    servo_on = doc["content"]["servo_on"].as<bool>();
    motor_speed = doc["content"]["motor_speed"].as<int>();

    //数据解析
    if (sscanf(RGB_message.c_str(), "%d %d %d", &RGB_red, &RGB_green, &RGB_blue) == 3)
    {
      strip.setPixelColor(0, strip.Color(RGB_red, RGB_green, RGB_blue));
      char rgbString[20];
      sprintf(rgbString, "%d %d %d", RGB_red, RGB_green, RGB_blue);
      RGB_Color = rgbString;
      strip.show();
    }
  }

  void read_dht11(float *temperature, float *humidity)
  {
    unsigned long currentMillis = millis();
    static unsigned long previousMillis = 0;
    if(previousMillis == 0 || currentMillis - previousMillis >= DHT_readtime)
    {
      // 创建事件对象，用于存储读数
      sensors_event_t event;
      // --- 读取温度 ---
      dht.temperature().getEvent(&event);
      if (isnan(event.temperature)) {
        *temperature = -999;  // 用 -999 表示读取失败
      } else {
        *temperature = event.temperature;
      }
      // --- 读取湿度 ---
      dht.humidity().getEvent(&event);
      if (isnan(event.relative_humidity)) {
        *humidity = -999;  // 用 -999 表示读取失败
      } else {
        *humidity = event.relative_humidity;
      }
      previousMillis = currentMillis;
    }
  }
  void read_light_value(uint *light_level)
  {

    unsigned long currentMillis = millis();
    static unsigned long previousMillis = 0;
    if(previousMillis == 0 || currentMillis - previousMillis >= Light_time)
    {  
      int light_value = analogRead(6);
      *light_level = 100 -(light_value * 100 / 4095);
      previousMillis = currentMillis;
    }
  }

  //舵机控制 自动模式与手动模式
  void myservo_control()
  {
    if(servo_auto == true)
    {
      if(light_level < 20)
      {
        servo_on = true;  
      }
      else if(light_level > 80)
      {
        servo_on = false;
      }
    }
    if(servo_on == true)
    {
      myservo.write(180);
    }
    else
    {
      myservo.write(0);
    }
  }
//电机控制挡位
void motor_control(int speed)
{
  switch(speed)
  {
    case 0:
      ledcWrite(motorPin, 0);
      break;
    case 1:
      ledcWrite(motorPin, 30 * 255 / 100);
      break;
    case 2:
      ledcWrite(motorPin, 60 * 255 / 100);
      break;
    case 3:
      ledcWrite(motorPin,  90 * 255 / 100);
      break;
  }
}


/*
// ... existing code ...

String getHuaweiToken() {
  // 构建请求体（JSON 格式）
  String payload = "{\"auth\":{\"identity\":{\"methods\":[\"password\"],\"password\":{\"user\":{\"domain\":{\"name\":\"" + 
                   String(domain_name) + "\"},\"name\":\"" + String(iam_user) + "\",\"password\":\"" + String(iam_password) + "\"}}}}}";

  Serial.println("Request payload:");
  Serial.println(payload);

  WiFiClientSecure client;
  client.setInsecure(); // 跳过SSL证书验证  
  if (!client.connect("iam.myhuaweicloud.com", 443)) {
    Serial.println("Connection failed");
    return "";
  }
  
  // 构建HTTP请求
  String request = "POST /v3/auth/tokens HTTP/1.1\r\n";
  request += "Host: iam.myhuaweicloud.com\r\n";
  request += "Content-Type: application/json;charset=utf8\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "\r\n";
  request += payload;
    
  // 发送请求
  client.print(request);
  
  // 等待响应
  long timeout = millis() + 10000; // 10秒超时
  while (!client.available() && millis() < timeout) {
    delay(1);
  }
  
  if (!client.available()) {
    Serial.println("Response timeout");
    client.stop();
    return "";
  }
  
  // 读取状态行
  String statusLine = client.readStringUntil('\r');
  client.read(); // 读取\n
  Serial.println("Status: " + statusLine);
  
  // 只读取需要的 X-Subject-Token 头部
  String token = "";
  while (client.available()) {
    String line = client.readStringUntil('\r');
    client.read(); // 读取\n
    
    // 检查是否是X-Subject-Token头部
    if (line.startsWith("X-Subject-Token: ")) {
      token = line.substring(17); // 提取token值
      token.trim(); // 去除首尾空格
    }
    
    
    // 如果遇到空行，表示头部结束
    if (line.length() <= 1) {
      Serial.println("Headers end");
      break;
    }
  }
  client.stop();
  return token;
}
// ... existing code ...


void getDeviceShadow()
{

  
}
*/