import socket
import threading
import wave

import datetime

def handle_client(conn, addr):
    print(f"Connected by {addr}")

    # 处理客户端发送的参数
    param_data = conn.recv(1024).decode('utf-8')
    if param_data.startswith("format:"):
        print(f"Received format data: {param_data}")
        # 发送 OK 给客户端确认
        conn.sendall(b"OK") 
    else:
        print("Invalid format data received")
        conn.close()
        return

    # 解析格式
    try:
        param_data = param_data.replace("\x00", "").strip()
        format_params = param_data[7:].split('channel')
        samplerate = format_params[0].replace("samplerate", "").strip()
        channels = int(format_params[1].split('bit')[0].strip())
        bitdepth = int(format_params[1].split('bit')[1].strip())

        samplerate = int(samplerate)
        print(f"Samplerate: {samplerate}, Channels: {channels}, Bitdepth: {bitdepth}")

        # 创建一个字典来保存分批次的数据
        audio_data_parts = {}
        expected_part = 1
        
        # 缓存接收到的数据
        buffer = b""

           # 缓存接收到的数据
        buffer = b""
        total_data_size = None
        receiving_data = False

        while True:
            data = conn.recv(1024)  # 每次最多接收1024字节
            if not data:
                break  # 客户端关闭连接，结束接收数据

            # 将收到的数据添加到缓冲区
            buffer += data

            # 检查是否包含 totalcount
            if b"totalcount:" in buffer:
                # 处理 totalcount，提取数据总长度
                totalcount_index = buffer.find(b"totalcount:") + len(b"totalcount:")
                total_data_size = int(buffer[totalcount_index:].strip())  # 获取数据总长度
                print(f"Total data size expected: {total_data_size} bytes")
                receiving_data = False
                break  # 收到 totalcount 后停止接收数据
            else:
                receiving_data = True

        if total_data_size is not None:
            # 用正则提取实际数据：找出 data: 到 totalcount: 之间的数据
            # 假设数据格式中始终包含 data: 和 totalcount:
            # 去掉 data: 和 totalcount: 标签后的实际数据
            # 找到 "data:" 标签的位置
            data_start_index = buffer.find(b"data:") + len(b"data:")

            # 找到 "totalcount:" 标签的位置
            totalcount_index = buffer.find(b"totalcount:")

            # 提取 data: 和 totalcount: 之间的数据
            actual_data = buffer[data_start_index:totalcount_index].strip()
            actual_data_size = len(actual_data)
                # 校验接收到的数据长度
            if actual_data_size == total_data_size:
                print(f"Data received successfully, total size: {actual_data_size} bytes")
                conn.sendall(b"OK")  # 数据完整，返回 OK
                save_audio_to_wav(actual_data, samplerate, channels, bitdepth)
            else:
                print(f"Error: Received data size {actual_data_size} does not match expected size {total_data_size}")
                conn.sendall(b"Error: Data size mismatch")  # 数据大小不一致，返回错误信息

        else:
            print("Error: No totalcount received")
            conn.sendall(b"Error: No totalcount received")

        print(f"Connection from {addr} closed")
        conn.close()

                  

    except Exception as e:
        print(f"Error: {e}")
        # conn.sendall(b"Error: Invalid format or data")
    
    print(f"Connection from {addr} closed")
    conn.close()

def save_audio_to_wav(audio_data, samplerate, channels, bitdepth):
    filename = f"received_audio_{datetime.datetime.now().strftime("%Y%m%d_%H%M%S")}.wav"
    try:
        with wave.open(filename, 'wb') as wf:
            wf.setnchannels(channels)
            wf.setsampwidth(bitdepth // 8)  # 每个样本的字节数
            wf.setframerate(samplerate)
            wf.writeframes(audio_data)
        print(f"Audio data saved as {filename}")
    except Exception as e:
        print(f"Failed to save WAV file: {e}")

# 创建一个TCP socket并绑定端口
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
host = '0.0.0.0'
port = 31333
s.bind((host, port))

# 监听连接
s.listen(5)
print('服务器已启动，监听端口', port)

while True:
    conn, addr = s.accept()
    # 创建新线程处理客户端连接
    client_thread = threading.Thread(target=handle_client, args=(conn, addr))
    client_thread.start()
