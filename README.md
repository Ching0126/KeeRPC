# KeeRPC

一个 C++ RPC 框架。服务端用 muduo 接收请求，客户端通过 Protobuf 发起调用，ZooKeeper 负责服务注册，一致性哈希在多个提供者之间选择地址，连接池复用已经建立的 TCP 连接。

当前示例是 `UserServiceRpc.Login`。调用流程是：

```text
Kclient
  → UserServiceRpc_Stub
  → KeeRPCChannel
  → ZooKeeper 查询提供者
  → 一致性哈希选择 ip:port
  → 连接池借出 fd
  → KeeRPCProvider
  → UserService::Login
```

## 环境

- C++14
- Protobuf
- muduo（`muduo_net`、`muduo_base`）
- glog
- ZooKeeper 多线程 C 客户端（`zookeeper_mt`）
- pthread

ZooKeeper 服务需要先启动。示例配置使用 `127.0.0.1:2181`。

## 构建

```bash
cmake -S . -B build
cmake --build build
```

生成两个程序：

- `build/example/callee/server`
- `build/example/caller/client`

## 配置

程序通过 `-i` 读取配置文件。`bin/test.conf` 的内容是：

```text
rpcserverip=127.0.0.1
rpcserverport=8000
zookeeperip=127.0.0.1
zookeeperport=2181
```

`rpcserverip` 和 `rpcserverport` 是服务端监听地址，也是客户端预热连接时使用的地址。正式 RPC 的目标地址来自 ZooKeeper，不直接读这两个配置项。

## 运行

先启动服务端：

```bash
./build/example/callee/server -i bin/test.conf
```

服务端监听 `127.0.0.1:8000`，然后在 ZooKeeper 上创建：

- `/UserServiceRpc`：持久节点，不保存数据
- `/UserServiceRpc/127.0.0.1:8000`：临时节点，数据是 `127.0.0.1:8000`

会话结束后，临时节点消失。

再启动客户端：

```bash
./build/example/caller/client -i bin/test.conf
```

客户端先按配置地址预热连接，再用 8 个线程各发送 200 次 `Login`，最后输出成功数、失败数、耗时和 QPS。

## 协议

请求由四段组成，两个长度都是网络字节序的 `uint32`：

```text
| total_len | header_len | RpcHeader | args |
```

`total_len` 不包含自己的 4 字节，只覆盖后面的 `header_len`、`RpcHeader` 和参数。`RpcHeader` 定义在 `src/KeeRPCheader.proto`：

- `service_name`
- `method_name`
- `args_size`

响应是：

```text
| response_len | Response |
```

服务端在 `OnMessage` 里按长度取完整的一帧。数据还没到齐时留在 muduo 的 `Buffer` 中，等待后续字节。

## 目录

```text
bin/test.conf                  示例配置
example/user.proto             Login、Register 的服务定义
example/callee/Kserver.cc      服务端示例，实现 Login
example/caller/Kclient.cc      多线程调用示例
src/KeeRPCApplication.*        解析 -i，保存全局配置
src/KeeRPCconfig.*             读取 key=value 配置
src/KeeRPCProvider.*           发布服务、监听端口、解析请求、注册 ZooKeeper
src/KeeRPCChannel.*            序列化请求、发现节点、收发响应
src/KeeRPCcontroller.*         保存一次调用的失败原因
src/include/KeeRPCLogger.h     初始化 glog
src/zookeeperutil.*            ZooKeeper 连接、创建节点、读取节点
src/ServiceDiscovery.*         按服务名查询提供者
src/ConsistentHash.*           按请求 key 选择一台机器
src/KeeRPCConnectPool.*        按 ip:port 复用空闲连接
src/include/LockFreeQueue.h    连接池使用的有界多生产者多消费者队列
```

`example/user.proto` 里还声明了 `Register`。示例服务端目前只实现了 `Login`。

## 现在的行为

服务发现每次调用都会向 ZooKeeper 读取该服务的全部子节点，并用这些地址重建哈希环。每台机器放 10 个虚拟节点，哈希函数是 CRC32。请求 key 是进程内递增的编号，因此连续请求会分散到当前存在的机器上；节点列表不变时，同一个 key 会落到同一台机器。

连接池按 `ip:port` 分组。每个地址最多 32 条连接，空闲的文件描述符放在无锁队列里。`BorrowConnection` 优先取空闲连接，没有时才新建。`ReturnConnection` 把可用连接放回队列；队列已满或连接已损坏时关闭它。查找地址表时使用互斥锁，取出和归还文件描述符时不再持有这把锁。

服务端在 muduo 的事件循环线程中完成协议解析和 `CallMethod`。
