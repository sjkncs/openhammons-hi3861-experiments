"""
智能家居 Web 可视化控制面板
Smart Home Web Control Panel

使用Flask + MQTT实现的可视化控制界面

依赖安装:
    pip install flask paho-mqtt

运行方式:
    python web_control_panel.py

浏览器访问:
    http://localhost:5000
    或 http://<你的PC IP>:5000
"""

from flask import Flask, render_template_string, request, jsonify, send_from_directory
import paho.mqtt.client as mqtt
import json
import threading
import time
import os

app = Flask(__name__)

# ==================== MQTT 配置 ====================

# 本地 MQTT Broker 配置 (mosquitto)
LOCAL_MQTT_BROKER = "localhost"
LOCAL_MQTT_PORT = 1883
LOCAL_MQTT_KEEPALIVE = 60

# 订阅和发布的主题
TOPIC_LED_CONTROL = "smart_home/led/control"      # 控制LED
TOPIC_STATUS_REPORT = "smart_home/hi3861/status" # 设备状态上报
TOPIC_GAS_SENSOR = "gas_sensor/data"             # 燃气传感器数据

# 华为云 MQTT 配置 (占位符; 真实地址请从环境变量 HUAWEI_MQTT_BROKER 读取, 不要 commit 真实值)
HUAWEI_MQTT_BROKER = os.environ.get("HUAWEI_MQTT_BROKER", "YOUR_HUAWEI_CLOUD_MQTT_BROKER")
HUAWEI_MQTT_PORT = int(os.environ.get("HUAWEI_MQTT_PORT", "1883"))

# ==================== 全局状态 ====================

class DeviceState:
    """设备状态管理"""
    led_state = "OFF"
    temperature = 25.0
    humidity = 60.0
    gas_value = 20.5
    rssi = -50
    last_update = time.time()
    history = []  # 历史数据（用于图表）
    
    @classmethod
    def update_history(cls):
        """更新历史数据"""
        cls.history.append({
            "time": time.strftime("%H:%M:%S"),
            "gas": cls.gas_value,
            "temp": cls.temperature,
            "humidity": cls.humidity
        })
        # 保留最近100条数据
        if len(cls.history) > 100:
            cls.history = cls.history[-100:]

state = DeviceState()

# ==================== MQTT 回调 ====================

def on_connect(client, userdata, flags, rc):
    """MQTT连接回调"""
    if rc == 0:
        print(f"[MQTT] Connected to {userdata['broker']}")
        client.subscribe(TOPIC_STATUS_REPORT)
        client.subscribe(TOPIC_GAS_SENSOR)
        print(f"[MQTT] Subscribed to: {TOPIC_STATUS_REPORT}, {TOPIC_GAS_SENSOR}")
    else:
        print(f"[MQTT] Connection failed, return code: {rc}")

def on_message(client, userdata, msg):
    """MQTT消息回调"""
    try:
        topic = msg.topic
        payload = msg.payload.decode('utf-8')
        print(f"[MQTT] Received on {topic}: {payload}")
        
        if topic == TOPIC_STATUS_REPORT:
            # 解析设备状态
            data = json.loads(payload)
            state.led_state = data.get("led", "OFF")
            state.temperature = data.get("temperature", 25.0)
            state.humidity = data.get("humidity", 60.0)
            state.gas_value = data.get("gas_value", 20.5)
            state.rssi = data.get("rssi", -50)
            state.last_update = time.time()
            state.update_history()
            
        elif topic == TOPIC_GAS_SENSOR:
            # 解析燃气传感器数据
            data = json.loads(payload)
            if "services" in data:
                for service in data["services"]:
                    if service.get("service_id") == "GasValue":
                        props = service.get("properties", {})
                        state.gas_value = float(props.get("gas_value", 0))
                        state.last_update = time.time()
                        state.update_history()
                        
    except json.JSONDecodeError:
        print(f"[MQTT] JSON decode error: {payload}")
    except Exception as e:
        print(f"[MQTT] Error processing message: {e}")

def on_disconnect(client, userdata, rc):
    """MQTT断开回调"""
    print(f"[MQTT] Disconnected from {userdata['broker']}, reconnecting...")
    time.sleep(5)
    try:
        client.reconnect()
    except:
        pass

# 创建本地MQTT客户端
local_mqtt = mqtt.Client(
    client_id="WebControlPanel",
    userdata={"broker": LOCAL_MQTT_BROKER}
)
local_mqtt.on_connect = on_connect
local_mqtt.on_message = on_message
local_mqtt.on_disconnect = on_disconnect

# 尝试连接本地MQTT
try:
    local_mqtt.connect(LOCAL_MQTT_BROKER, LOCAL_MQTT_PORT, LOCAL_MQTT_KEEPALIVE)
    local_mqtt.loop_start()
except:
    print("[MQTT] Cannot connect to local broker, running in demo mode")

# ==================== Web 页面模板 ====================

HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>智能家居控制面板 - Hi3861</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        
        h1 {
            text-align: center;
            color: white;
            margin-bottom: 30px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.2);
        }
        
        .card {
            background: white;
            border-radius: 20px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.1);
        }
        
        .card h2 {
            color: #333;
            margin-bottom: 20px;
            padding-bottom: 10px;
            border-bottom: 2px solid #667eea;
        }
        
        /* 设备状态网格 */
        .status-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin-bottom: 20px;
        }
        
        .status-item {
            background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
            padding: 20px;
            border-radius: 15px;
            text-align: center;
        }
        
        .status-item .icon {
            font-size: 40px;
            margin-bottom: 10px;
        }
        
        .status-item .label {
            color: #666;
            font-size: 14px;
            margin-bottom: 5px;
        }
        
        .status-item .value {
            font-size: 28px;
            font-weight: bold;
            color: #333;
        }
        
        .status-item .unit {
            font-size: 14px;
            color: #666;
        }
        
        /* LED 控制按钮 */
        .control-buttons {
            display: flex;
            gap: 15px;
            flex-wrap: wrap;
            justify-content: center;
            margin: 20px 0;
        }
        
        .btn {
            padding: 15px 30px;
            border: none;
            border-radius: 10px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.3s ease;
            display: flex;
            align-items: center;
            gap: 8px;
        }
        
        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 20px rgba(0,0,0,0.2);
        }
        
        .btn-on {
            background: linear-gradient(135deg, #4CAF50 0%, #45a049 100%);
            color: white;
        }
        
        .btn-off {
            background: linear-gradient(135deg, #f44336 0%, #e53935 100%);
            color: white;
        }
        
        .btn-toggle {
            background: linear-gradient(135deg, #2196F3 0%, #1976D2 100%);
            color: white;
        }
        
        .btn-alert {
            background: linear-gradient(135deg, #FF9800 0%, #F57C00 100%);
            color: white;
        }
        
        /* 图表区域 */
        .chart-container {
            position: relative;
            height: 300px;
            margin-top: 20px;
        }
        
        /* 告警状态 */
        .alert-banner {
            display: none;
            background: linear-gradient(135deg, #f44336 0%, #e53935 100%);
            color: white;
            padding: 20px;
            border-radius: 15px;
            margin-bottom: 20px;
            text-align: center;
            animation: pulse 2s infinite;
        }
        
        .alert-banner.active {
            display: block;
        }
        
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.7; }
        }
        
        .alert-banner .icon {
            font-size: 50px;
            margin-bottom: 10px;
        }
        
        /* 连接状态 */
        .connection-status {
            display: flex;
            align-items: center;
            gap: 10px;
            padding: 10px 20px;
            background: #f5f5f5;
            border-radius: 10px;
            margin-bottom: 20px;
        }
        
        .status-dot {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: #999;
        }
        
        .status-dot.connected {
            background: #4CAF50;
            animation: blink 2s infinite;
        }
        
        @keyframes blink {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        
        /* 实时数据表格 */
        .data-table {
            width: 100%;
            border-collapse: collapse;
        }
        
        .data-table th, .data-table td {
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #eee;
        }
        
        .data-table th {
            background: #f5f7fa;
            font-weight: 600;
        }
        
        /* 设备信息 */
        .device-info {
            background: #f5f7fa;
            padding: 15px;
            border-radius: 10px;
            margin-top: 20px;
        }
        
        .device-info p {
            margin: 5px 0;
            color: #666;
        }
        
        /* 响应时间指示器 */
        .response-time {
            font-size: 12px;
            color: #999;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🏠 智能家居控制面板</h1>
        
        <!-- 告警横幅 -->
        <div id="alertBanner" class="alert-banner">
            <div class="icon">🚨</div>
            <h2>燃气浓度异常！</h2>
            <p id="alertMessage">检测到环境中燃气浓度超标，请检查！</p>
        </div>
        
        <!-- 连接状态 -->
        <div class="connection-status">
            <div id="statusDot" class="status-dot"></div>
            <span id="statusText">正在连接...</span>
            <span class="response-time">| 最后更新: <span id="lastUpdate">--:--:--</span></span>
        </div>
        
        <!-- 设备状态卡片 -->
        <div class="card">
            <h2>📊 设备状态</h2>
            <div class="status-grid">
                <div class="status-item">
                    <div class="icon">💡</div>
                    <div class="label">LED 状态</div>
                    <div class="value" id="ledValue">--</div>
                </div>
                <div class="status-item">
                    <div class="icon">🌡️</div>
                    <div class="label">温度</div>
                    <div class="value"><span id="tempValue">--</span><span class="unit">°C</span></div>
                </div>
                <div class="status-item">
                    <div class="icon">💧</div>
                    <div class="label">湿度</div>
                    <div class="value"><span id="humidityValue">--</span><span class="unit">%</span></div>
                </div>
                <div class="status-item">
                    <div class="icon">🔥</div>
                    <div class="label">燃气浓度</div>
                    <div class="value"><span id="gasValue">--</span><span class="unit">kΩ</span></div>
                </div>
                <div class="status-item">
                    <div class="icon">📶</div>
                    <div class="label">WiFi信号</div>
                    <div class="value"><span id="rssiValue">--</span><span class="unit">dBm</span></div>
                </div>
                <div class="status-item">
                    <div class="icon">📈</div>
                    <div class="label">上报次数</div>
                    <div class="value"><span id="reportCount">--</span><span class="unit">次</span></div>
                </div>
            </div>
        </div>
        
        <!-- LED 控制卡片 -->
        <div class="card">
            <h2>🎮 设备控制</h2>
            <div class="control-buttons">
                <button class="btn btn-on" onclick="sendCommand('led_on')">
                    💡 开灯
                </button>
                <button class="btn btn-off" onclick="sendCommand('led_off')">
                    💡 关灯
                </button>
                <button class="btn btn-toggle" onclick="sendCommand('toggle')">
                    🔄 切换
                </button>
                <button class="btn btn-alert" onclick="sendCommand('blink')">
                    ⚡ 测试告警
                </button>
            </div>
        </div>
        
        <!-- 历史数据卡片 -->
        <div class="card">
            <h2>📈 历史数据</h2>
            <div id="historyTable">
                <table class="data-table">
                    <thead>
                        <tr>
                            <th>时间</th>
                            <th>燃气浓度 (kΩ)</th>
                            <th>温度 (°C)</th>
                            <th>湿度 (%)</th>
                        </tr>
                    </thead>
                    <tbody id="historyBody">
                        <tr><td colspan="4" style="text-align:center;color:#999;">暂无数据</td></tr>
                    </tbody>
                </table>
            </div>
        </div>
        
        <!-- 设备信息 -->
        <div class="device-info">
            <h3>📋 设备信息</h3>
            <p><strong>设备型号:</strong> HiSpark Pegasus (Hi3861)</p>
            <p><strong>固件版本:</strong> v1.0.0-smart-scene</p>
            <p><strong>MQTT Broker:</strong> <span id="mqttBroker">localhost:1883</span></p>
        </div>
    </div>
    
    <script>
        // 发送控制命令
        function sendCommand(cmd) {
            fetch('/control?cmd=' + cmd)
                .then(response => response.json())
                .then(data => {
                    console.log('Command sent:', data);
                    showNotification('命令已发送: ' + cmd);
                })
                .catch(error => {
                    console.error('Error:', error);
                    showNotification('命令发送失败', 'error');
                });
        }
        
        // 显示通知
        function showNotification(message, type = 'success') {
            const notification = document.createElement('div');
            notification.style.cssText = `
                position: fixed;
                top: 20px;
                right: 20px;
                padding: 15px 25px;
                background: ${type === 'error' ? '#f44336' : '#4CAF50'};
                color: white;
                border-radius: 10px;
                z-index: 1000;
                animation: slideIn 0.3s ease;
            `;
            notification.textContent = message;
            document.body.appendChild(notification);
            setTimeout(() => {
                notification.style.animation = 'slideOut 0.3s ease';
                setTimeout(() => notification.remove(), 300);
            }, 2000);
        }
        
        // 更新状态显示
        function updateStatus() {
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    // 更新连接状态
                    document.getElementById('statusDot').className = 'status-dot connected';
                    document.getElementById('statusText').textContent = '已连接';
                    
                    // 更新设备状态
                    document.getElementById('ledValue').textContent = data.led_state;
                    document.getElementById('tempValue').textContent = data.temperature.toFixed(1);
                    document.getElementById('humidityValue').textContent = data.humidity.toFixed(1);
                    document.getElementById('gasValue').textContent = data.gas_value.toFixed(2);
                    document.getElementById('rssiValue').textContent = data.rssi;
                    document.getElementById('lastUpdate').textContent = data.last_update;
                    
                    // 检查告警
                    const alertBanner = document.getElementById('alertBanner');
                    if (data.gas_value < 30) {
                        alertBanner.classList.add('active');
                        document.getElementById('alertMessage').textContent = 
                            '当前燃气浓度: ' + data.gas_value.toFixed(2) + ' kΩ';
                    } else {
                        alertBanner.classList.remove('active');
                    }
                    
                    // 更新历史数据
                    updateHistory(data.history);
                })
                .catch(error => {
                    console.error('Status update error:', error);
                    document.getElementById('statusDot').className = 'status-dot';
                    document.getElementById('statusText').textContent = '未连接';
                });
        }
        
        // 更新历史数据表格
        function updateHistory(history) {
            const tbody = document.getElementById('historyBody');
            if (!history || history.length === 0) return;
            
            tbody.innerHTML = history.slice(-10).reverse().map(item => `
                <tr>
                    <td>${item.time}</td>
                    <td>${item.gas.toFixed(2)}</td>
                    <td>${item.temp.toFixed(1)}</td>
                    <td>${item.humidity.toFixed(1)}</td>
                </tr>
            `).join('');
        }
        
        // 定期更新状态
        setInterval(updateStatus, 2000);
        updateStatus();
        
        // 添加动画样式
        const style = document.createElement('style');
        style.textContent = `
            @keyframes slideIn {
                from { transform: translateX(100%); opacity: 0; }
                to { transform: translateX(0); opacity: 1; }
            }
            @keyframes slideOut {
                from { transform: translateX(0); opacity: 1; }
                to { transform: translateX(100%); opacity: 0; }
            }
        `;
        document.head.appendChild(style);
    </script>
</body>
</html>
"""

# ==================== Flask 路由 ====================

@app.route('/')
def index():
    """主页"""
    return render_template_string(HTML_TEMPLATE)

@app.route('/control')
def control():
    """处理控制命令"""
    cmd = request.args.get('cmd', '')
    
    if cmd == 'led_on':
        local_mqtt.publish(TOPIC_LED_CONTROL, '{"cmd":"led_on"}')
        state.led_state = "ON"
    elif cmd == 'led_off':
        local_mqtt.publish(TOPIC_LED_CONTROL, '{"cmd":"led_off"}')
        state.led_state = "OFF"
    elif cmd == 'toggle':
        state.led_state = "OFF" if state.led_state == "ON" else "ON"
        local_mqtt.publish(TOPIC_LED_CONTROL, 
                          f'{{"cmd":"led_{"off" if state.led_state == "OFF" else "on"}"}}')
    elif cmd == 'blink':
        # 测试告警闪烁
        for _ in range(5):
            local_mqtt.publish(TOPIC_LED_CONTROL, '{"cmd":"led_on"}')
            time.sleep(0.2)
            local_mqtt.publish(TOPIC_LED_CONTROL, '{"cmd":"led_off"}')
            time.sleep(0.2)
    
    return jsonify({
        "status": "ok",
        "cmd": cmd,
        "led_state": state.led_state
    })

@app.route('/api/status')
def api_status():
    """获取设备状态API"""
    return jsonify({
        "led_state": state.led_state,
        "temperature": state.temperature,
        "humidity": state.humidity,
        "gas_value": state.gas_value,
        "rssi": state.rssi,
        "last_update": time.strftime("%H:%M:%S", time.localtime(state.last_update)),
        "history": state.history[-20:] if state.history else []
    })

@app.route('/api/history')
def api_history():
    """获取历史数据API"""
    return jsonify({
        "history": state.history
    })

# ==================== 启动服务器 ====================

if __name__ == '__main__':
    print("=" * 50)
    print("智能家居 Web 控制面板")
    print("=" * 50)
    print("本地MQTT Broker:", LOCAL_MQTT_BROKER, LOCAL_MQTT_PORT)
    print("访问地址: http://localhost:5000")
    print("=" * 50)
    
    # 启动Flask服务器
    app.run(host='0.0.0.0', port=5000, debug=True, threaded=True)
