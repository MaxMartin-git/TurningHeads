# TurningHeads

TurningHeads 是一个基于 WiFi 协调的 ESP32 项目，用于控制双轴舵机机构和直流电机。协调器运行 Web UI 和 WebSocket 服务器，而一个或多个节点通过 TCP 连接以接收电机指令。

## 项目结构

- `platformio.ini` - PlatformIO 环境和开发板配置。
- `src/main.cpp` - 作为 ESP32-C3 协调器固件。
- `src/main_node.cpp` - 节点固件，连接协调器并驱动 PWM 电机。
- `src/servo_id_setter.cpp` - 设置舵机 ID 的辅助构建环境。
- `src/servo_id_reader.cpp` - 读取舵机 ID 的辅助构建环境。
- `src/servo_center_setter.cpp` - 设置舵机中心偏移的辅助构建环境。
- `src/main_test.cpp` - 用于验证开发板是否上电正常的最小测试固件。

## 功能

- 协调器创建 WiFi 接入点：`ESP_TH` / `TurningHeads123`
- 具有摇杆风格舵机控制的 HTTP Web 界面
- 通过 WebSocket 更新舵机位置、电机 PWM、电机方向和节点连接状态
- 端口 `5000` 上的 TCP 服务器，用于节点电机指令
- 通过 UART 控制 SCServo 实现上下和横向舵机运动
- 节点端用于直流电机控制的 PWM 输出

## 硬件

- ESP32-C3 开发板
- 兼容 SCServo 的舵机控制器或直接 UART 舵机接口
- 带 PWM 输入的直流电机驱动器
- 节点设备连接到协调器的 WiFi AP

## 构建与上传

1. 在 VS Code 中安装 PlatformIO。
2. 在 VS Code 中打开此项目文件夹。
3. 在 `platformio.ini` 中选择所需环境：
   - `coordinator` - 带 Web UI 和舵机控制的协调器固件
   - `node1` - 节点电机控制固件（NODE_ID=1）
   - `servo_id_setter` - 舵机 ID 配置辅助程序
   - `servo_id_reader` - 舵机 ID 读取辅助程序
   - `servo_center_setter` - 舵机中心校准辅助程序
   - `main_test` - 基本板卡存活测试
4. 使用 PlatformIO 命令或状态栏按钮进行构建和上传。

## 协调器使用

1. 使用 `coordinator` 环境刷写 `src/main.cpp`。
2. 给开发板供电并连接到 WiFi AP `ESP_TH`，密码为 `TurningHeads123`。
3. 在浏览器中打开 `http://192.168.4.1/`。
4. 使用屏幕上的摇杆和电机控制，将舵机移动并将电机指令发送给节点。

## 节点使用

1. 使用 `node1`、`node2` 或 `node3` 环境刷写 `src/main_node.cpp`。
2. 将节点接线配置为 `GPIO3` 为电机驱动器提供 PWM。
3. 将节点连接到协调器 WiFi AP。
4. 节点将尝试连接到 `192.168.4.1:5000`，并从协调器接收电机指令。

## 注意事项

- 协调器使用 UART 引脚 `GPIO21`（TX）和 `GPIO20`（RX）进行舵机控制。
- 节点假定协调器接入点 SSID 和密码完全匹配。
- 如果你的机械结构需要不同的行程范围，请在 `src/main.cpp` 中调整舵机限位。

## 许可

可根据教育和原型设计需求改编或重用此代码。