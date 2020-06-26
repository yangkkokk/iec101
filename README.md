# MG301程序移植说明

## 程序移植需求

1. 将MG301.c中的AT指令从MG323-B上网模块移植到SIM7600CE上网模块；
2. 移植后所有功能保持与原来一致；
3. 移植后的程序可以在新的硬件平台上运行并正常收发数据。

## 提供资料

1. MG301.c和MG301.h程序源代码；
2. MG323-B上网模块AT指令手册；
3. SIM7600CE上网模块AT指令手册。

## 程序修改说明

### 上电时序

![1-1](images/1-1.png "1-1")

![1-2](images/1-2.png "1-2")

![1-3](images/1-3.png "1-3")

上电时序为低电平脉冲达到一定时间，参考典型值程序中设定为500ms。将GPIOB15拉高，延时500ms后，把GPIOB15拉低。

### 测试模式（DEBUG\_MODE）

![2-1](images/2-1.png "2-1")

![2-2](images/2-2.png "2-2")

测试模式采用透传模式，设置完模式以及网络连接之后，用UART通信直接发送读取数据，作为硬件通讯测试。

测试模式代码如下所示：

### MG301.c文件修改

![3-1](images/3-1.png "3-1")

正常使用时采用数据模式中的缓冲器模式，利用CIPSEND发送数据，CIPRXGET读取返回信息。

MG301.c文件的修改主要是AT指令的修改以及上电时序的修改。
