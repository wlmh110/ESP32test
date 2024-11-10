#!/bin/bash
# Yu-SxQzxi|7ZdNCTa

# 定义版本号和容器名称变量
version="1.0"
container_name="aiserver"
echo "Version: $version"
echo "Container Name: $container_name"

# 构建Docker镜像
sudo docker build -t "$container_name:$version" .

# 停止并删除之前的容器
sudo docker rm -f "$container_name"

# 运行新的容器，假设你要将容器连接到名为 "my-net" 的网络
sudo docker run -d -p 31333:31333/tcp -p 31333:31333/udp --name "$container_name" --network my-net --restart always -v /volume2/homes/WangLin/AIassistant:/app/output:rw "$container_name:$version"
# sudo docker run -d -p 17305:17305/tcp -p 17305:17305/udp -p 17304:17304/tcp -p 17304:17304/udp --name "$container_name" --network my-net "$container_name:$version"
