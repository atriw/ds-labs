# RDT lab

## 包头设计
为了方便发送方和接收方使用一样的包

不同的是发送方使用sequence number 接收方发回ack number

| payload size | sequence/ack number | checksum | payload |
| ------------ | ------------------- | -------- | ------- |
| 1 byte       | 4 byte              | 2 byte   | rest    |

## 策略

使用了Go-Back-N策略，发送方window大小为10，接收方window大小为1

具体流程如下：
- 发送方从上层接收到消息，进行装包（设置payload size，sequence number， checksum）
- 发送方查看window可用大小，若可用将包放入window中，设置定时器，发送到下层，
若不可用，则将此包放入缓冲区
- 接收方有一个全局变量表示当前应接受的sequence number，接收方从下层接收到包，
进行校验，若不通过直接舍弃，若通过再检查包的sequence number
    - 若大于应接收的数字则表示这是超前发送的包，将其放入缓冲区
    - 若等于则进行确认操作：封装消息发送到上层，增加应接受的sequence number，发送ack，检查缓冲区，若缓冲区有下一个应接受的sequence number，则对其进行确认操作，这是一个递归过程
    - 若小于，则表示这是发送方重发的包，说明发送方没有收到相应ack，重新发送ack
- 发送方有两个全局变量，一个是应发送的sequence number，一个是应接收的ack number。发送方发送消息后有两种可能
    - 收到ack：进行校验，不通过则舍弃，通过则检查收到的ack是否大于应接收的ack且小于应发送的sequence number，满足则说明[ack-expected, ack-actual]这个范围的包都被成功接收，进行确认操作：停止对应计时器，增加应接收的ack，增加window可用大小，读缓冲区，用缓冲区中的包填满window
    - 超时：停止所有计时器，重新发送window中未被ack的所有包，设定计时器

## 实现

### 虚拟计时器
接口提供了单个计时器，不同满足同时对多个包进行计时的需求，所以在这个计时器的基础上实现多个独立计时器。主要思路是建立一个链表，把所有虚拟计时器保存起来，虚拟计时器中保存了它们的开始时间和过期时间，需要计时时先查看接口计时器是否已经计时，若未计时则按照当前虚拟计时器的过期时间进行计时，若已计时则将虚拟计时器添加到链表尾。每当正在计时的虚拟计时器停止，接口计时器就会从链表找到下一个虚拟计时器，并计算出恰当的过期时间。这种方法只适用于所有计时器过期时间都相同，这样就不会有冲突，这里的需求正好是所有计时器都有统一的过期时间。

### 缓冲区
发送方和接收方都需要一个包缓冲区，发送方用一个单链表实现，接收方用一个带去重和插入时排序的单链表实现。

### 校验
使用checksum进行校验
生成算法如下
```c
static unsigned short gen_checksum(char *buf, int nword)
{
    unsigned long sum;
 
    for(sum = 0; nword > 0; nword--)
        sum += *buf++;             
 
    sum  = (sum>>16) + (sum&0xffff);
    sum += (sum>>16);

    return ~sum;
}
```
校验算法如下
```c
static bool verify_checksum(char *buf, int nword, unsigned short checksum)
{
    unsigned long sum;
 
    for(sum = 0; nword > 0; nword--)
        sum += *buf++;             
 
    sum  = (sum>>16) + (sum&0xffff);
    sum += (sum>>16);

    return (((sum + checksum) & 0xff) == 0xff);
}
```

## 测试
```shell
$ ./rdt_sim 1000 0.1 100 0.15 0.15 0.15 0
## Reliable data transfer simulation with:
    simulation time is 1000.000 seconds
    average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 15.00%
	average loss rate is 15.00%
	average corrupt rate is 15.00%
	tracing level is 0
Please review these inputs and press <enter> to proceed.
At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 1000.59s: sender finalizing ...
At 1000.59s: receiver finalizing ...

## Simulation completed at time 1000.59s with
	1000207 characters sent
	1000207 characters delivered
	43982 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.
```

```shell
$ ./rdt_sim 1000 0.1 100 0.3 0.3 0.3 0
## Reliable data transfer simulation with:
	simulation time is 1000.000 seconds
	average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 30.00%
	average loss rate is 30.00%
	average corrupt rate is 30.00%
	tracing level is 0
Please review these inputs and press <enter> to proceed.
At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 1416.04s: sender finalizing ...
At 1416.04s: receiver finalizing ...

## Simulation completed at time 1416.04s with
	988045 characters sent
	988045 characters delivered
	54075 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.
```