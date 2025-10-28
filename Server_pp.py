import socket
import datetime
import pyqtgraph as pg
import time
from pyqtgraph.Qt import QtCore, QtGui
import numpy as np
import threading
import queue
import csv
import sys

# 配置参数
HOST = '0.0.0.0'
PORT = 8080
MAX_DATA_POINTS = 1000  # 限制最大数据点数
CSV_FILE = 'respiratory_data.csv'
BUFFER_SIZE = 4096
RECV_TIMEOUT = 0.1

# 创建数据队列和事件
data_queue = queue.Queue(maxsize=1000)
stop_event = threading.Event()
header_received = False
connection_active = False

# 初始化数据存储
timestamps = np.array([])  # 使用numpy数组提高性能
pressures = np.array([])
temperatures = np.array([])
start_time = None

# 创建应用和窗口
app = QtGui.QGuiApplication([])
win = pg.GraphicsLayoutWidget(show=True, title="ESP32传感器数据实时监控")
win.resize(1000, 600)

# 创建图表
p1 = win.addPlot(title="气压监控")
p1.setLabel('left', '气压', units='kPa')
p1.addLegend()
curve1 = p1.plot(pen='b', name='气压(kPa)')
p1.showGrid(x=True, y=True)

win.nextRow()
p2 = win.addPlot(title="温度监控")
p2.setLabel('left', '温度', units='°C')
p2.setLabel('bottom', '时间', units='秒')
p2.addLegend()
curve2 = p2.plot(pen='r', name='温度(°C)')
p2.showGrid(x=True, y=True)


# 数据接收线程函数
def receive_data():
    global start_time, header_received, connection_active

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(RECV_TIMEOUT)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            s.bind((HOST, PORT))
            s.listen(1)
            print(f"监听 {PORT} 端口...")
        except OSError as e:
            print(f"套接字错误: {e}")
            return

        with open(CSV_FILE, 'a', newline='') as csvfile:
            writer = csv.writer(csvfile)
            if csvfile.tell() == 0:
                csvfile.write("时间戳,压力(kPa),温度(°C),气阀开度,呼吸状态\n")

            while not stop_event.is_set():
                try:
                    if not connection_active:
                        try:
                            conn, addr = s.accept()
                            conn.settimeout(RECV_TIMEOUT)
                            connection_active = True
                            print(f"{addr} 已连接")
                        except socket.timeout:
                            continue

                    try:
                        data = conn.recv(BUFFER_SIZE)
                        if not data:
                            connection_active = False
                            conn.close()
                            print("连接已关闭")
                            continue

                        decoded = data.decode('utf-8').strip()
                        if decoded:
                            process_data(decoded, writer)

                    except socket.timeout:
                        continue
                    except Exception as e:
                        connection_active = False
                        conn.close()
                        print(f"接收数据错误: {e}")

                except Exception as e:
                    if not stop_event.is_set():
                        print(f"通信错误: {e}")
                        time.sleep(1)


# 处理接收到的数据
def process_data(data, writer):
    global start_time, header_received

    parts = data.split(',')
    if len(parts) >= 3:
        try:
            pressure = float(parts[1])
            temperature = float(parts[2])

            current_time = datetime.datetime.now()
            if start_time is None:
                start_time = current_time
                relative_time = 0.0
            else:
                relative_time = (current_time - start_time).total_seconds()

            writer.writerow([datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")] + parts)

            if data_queue.full():
                data_queue.get_nowait()
            data_queue.put_nowait((relative_time, pressure, temperature))

            print(f"已接收: 气压={pressure}kPa, 温度={temperature}°C, 时间={relative_time:.1f}s")
        except ValueError as e:
            print(f"解析错误: {e}, 数据: {data}")


# 更新函数
def update():
    global timestamps, pressures, temperatures

    count = 0
    while not data_queue.empty() and count < 10:
        try:
            time, pressure, temp = data_queue.get_nowait()

            # 使用numpy数组追加数据
            timestamps = np.append(timestamps, time)
            pressures = np.append(pressures, pressure)
            temperatures = np.append(temperatures, temp)

            if len(timestamps) > MAX_DATA_POINTS:
                timestamps = timestamps[-MAX_DATA_POINTS:]
                pressures = pressures[-MAX_DATA_POINTS:]
                temperatures = temperatures[-MAX_DATA_POINTS:]

            count += 1
        except queue.Empty:
            break

    if count > 0:
        # 更新曲线数据
        curve1.setData(timestamps, pressures)
        curve2.setData(timestamps, temperatures)

        # 更新视图范围
        if len(timestamps) > 5:
            x_min = max(0, timestamps[-1] - 30)
            p1.setXRange(x_min, timestamps[-1] + 1)
            p2.setXRange(x_min, timestamps[-1] + 1)

            p1.setYRange(min(pressures) * 0.99, max(pressures) * 1.01)
            p2.setYRange(min(temperatures) * 0.99, max(temperatures) * 1.01)

# 创建并启动数据接收线程
receive_thread = threading.Thread(target=receive_data)
receive_thread.daemon = True
receive_thread.start()

# 设置定时器更新图表
timer = QtCore.QTimer()
timer.timeout.connect(update)
timer.start(50)  # 每50ms更新一次

# 启动应用
if __name__ == '__main__':
    if (sys.flags.interactive != 1) or not hasattr(QtCore, 'PYQT_VERSION'):
        QtGui.QGuiApplication.instance().exec_()

    # 程序结束时清理
    stop_event.set()
    if receive_thread.is_alive():
        receive_thread.join(timeout=2.0)
    print("程序已安全退出")