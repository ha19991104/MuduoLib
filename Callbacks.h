#pragma once 
#include <memory>
#include <functional>
#include "Timestamp.h"

class Buffer;
class TcpConnection;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnctionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
// 高水位， 水位线，越过水位线，数据会丢失？需要将水位控制在水位以下
// 发数据的时候，对端收的慢，你发的快，数据会丢失，可能出错，接收方和发送方两边的速率要接近，才是良好的网络状况
// 如果达到水位线了，可以暂停发送
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&,size_t)>;

using MessageCallback = std::function<void(const TcpConnectionPtr&,Buffer*,Timestamp)>;


