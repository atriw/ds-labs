# RDT lab

## 包头设计
为了方便发送方和接收方使用一样的包

不同的是发送方使用sequence number 接收方发回ack number

| checksum | payload size | sequence/ack number | payload |
| -------- | ------------ | ------------------- | ------- |
| 2 byte   |  1 byte      |    4 byte           | rest    |

- 因为是单向传输，所以发送方只发sequence number，接收方只发ack number，固用同一个位置储存
- checksum放开头，这样包剩下的数据是连续地址，方便生成checksum

## 策略

分别实现了Go-Back-N策略和Selective Repeat策略

策略具体不赘述，主要写一下他们之间具体的相同和不同

发送方：

都有窗口，都有两个变量分别记录期望收到的ack和下一个发送的seq，同样有发送、超时和确认三个逻辑

| 策略 | 发送 | 超时 | 确认 |
| --- | --- | --- | --- |
| GBN | 相同 | 停止所有计时器， 把窗口里未确认的包全部重发 | 仅确认ack包，不确认nak包 |
| SR | 相同 | 仅重发触发超时的seq的包 | 对ack包和nak包分别确认 |

接收方：

都有一个变量记录期望seq，同样有一个接收窗口大小，
都要处理四个逻辑，收到的包的seq分别在一个窗口的距离内小于、大于和等于期望的seq，还有一种情况是收到的seq和期望seq超出一个窗口的距离

不同的是SR需要小心处理乱序的情况，要有一个队列把超前的包排列起来

| 策略 | 小于期望 | 大于 | 等于 | 超出窗口 |
| --- | --- | --- | --- | --- |
| GBN | 重新发送收到的seq的ack包 | 丢弃 | 发送ack，提交包至上层 | 丢弃 |
| SR | 重新发送收到的seq的ack包 | 把包加入队列，发送当前期望seq的nak包 | 把队列里连续的包全部发送至上层，同时ack | 丢弃 |

### 窗口大小

窗口大小和sequence number最大值有几点限制

- max_seq为发送方窗口大小的倍数，因为sequence number要取模以后放到窗口里
- GBN的接收窗口为1，SR的接收窗口需要小于max_seq-发送方窗口，否则会造成一个bug，见下述

上面提到的收到seq“小于”，“大于”和“等于”，并不是算术意义，而是时间顺序意义，因为sequence number到达最大值后需要从0开始循环，
比较数值的大小不能判断包的顺序，需要在一个窗口内比较才有意义，
当SR的接收窗口大于等于max_seq-发送方窗口时，会发生一些陈旧的包被接收后被判断为新包的情况，

比如max_seq=16, 双方窗口为8，接收方期望seq为10的时候，由于某种原因发送方重发了7，接收方在期望seq进展到0的时候才收到了7，
由于0和7在一个大小为8的窗口里，7被判断为比0新，然后这是一个陈旧的包，于是产生bug。

当然最简单的解决办法是把max_seq设为一个很大的值，只要是发送方窗口的倍数就行

## 实现

### 虚拟计时器
使用一个物理计时器模拟多个虚拟计时器，因为这里所有过期时间都相同，用了一种简化的方法，把所有虚拟计时器放在一个单链表里，保存开始时间和过期时间，
物理计时器停止时从链表头弹出虚拟计时器，计算下一个虚拟计时器的物理过期时间。

如果需要不同的过期时间，可以用优先队列，以过期时刻进行排序。

### 缓冲区
发送方为了不block上层，把所有接收的消息装包放到缓冲区中，当窗口可用时再进行发送。

SR中，接收方为了重排包，需要一个unique和sorted的容器保存超前的包，c++里的set是用红黑树实现的，保留了顺序，同时满足unique。

### 类和接口
作为现代人，我们应该使用类来抽象对象，而不是c的struct和一堆helper function

类：Vtimer（虚拟计时器），Window（发送方窗口），Handler（接收方处理器）

接口：

Vtimer: Start, Stop, Reset, First, 其中First是为了满足SR超时重发，需要知道造成超时的seq

Window：Push，Acknowledge，Timeout

Handler：Handle，对前述四种情况进行分发的逻辑

同时实现两种策略只需要把Acknowledge，Timeout，Handle设成虚函数，子类分别实现就行了

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

### 包损坏处理

直接丢掉

## 测试
```shell
$ make run-gbn-0.15
g++ -Wall -g -DGBN=1 -o rdt_sim  rdt_sim.cc util.cc rdt_receiver.cc rdt_sender.cc
./rdt_sim  1000 0.1 100 0.15 0.15 0.15 0
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
At 2179.37s: sender finalizing ...
At 2179.37s: receiver finalizing ...

## Simulation completed at time 2179.37s with
        981698 characters sent
        981698 characters delivered
        73505 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.
```

```shell
$ make run-gbn-0.3
g++ -Wall -g -DGBN=1 -o rdt_sim  rdt_sim.cc util.cc rdt_receiver.cc rdt_sender.cc
./rdt_sim  1000 0.1 100 0.3 0.3 0.3 0
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
At 5508.01s: sender finalizing ...
At 5508.01s: receiver finalizing ...

## Simulation completed at time 5508.01s with
        995766 characters sent
        995766 characters delivered
        125576 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.
```

```shell
$ make run-sr-0.15
g++ -Wall -g -DGBN=0 -o rdt_sim  rdt_sim.cc util.cc rdt_receiver.cc rdt_sender.cc

./rdt_sim  1000 0.1 100 0.15 0.15 0.15 0
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
At 1247.86s: sender finalizing ...
At 1247.86s: receiver finalizing ...

## Simulation completed at time 1247.86s with
        996483 characters sent
        996483 characters delivered
        53369 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.
```

```shell
$ make run-sr-0.3
g++ -Wall -g -DGBN=0 -o rdt_sim  rdt_sim.cc util.cc rdt_receiver.cc rdt_sender.cc

./rdt_sim  1000 0.1 100 0.3 0.3 0.3 0
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
At 2387.69s: sender finalizing ...
At 2387.69s: receiver finalizing ...

## Simulation completed at time 2387.69s with
        987778 characters sent
        987778 characters delivered
        66440 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.
```

## Future work

GBN没什么优化空间

SR目前nak发送得有点多，接收方也可以有个计时器记录nak时间，没到时间就不继续nak了。还可以调调窗口大小和timeout大小之类的。