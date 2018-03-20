PCIV 相关样例程序包含以下几部分：

1、PCIV MSG ：PCI业务层的消息通讯封装。基于MCC模块提供的ioctl接口，提供消息端口的打开关闭、消息发送、消息接收等接口。
	相关代码为pciv_msg.c、pciv_msg.h。

2、PCIV Trans：PCI业务层的数据传输封装。基于PCI DMA传输接口、PCI消息交互及一套基本的读写指针Buffer，实现业务层通用数据传输接口。
	可以用于主从片之间的任何类型的数据的传输。相关代码为pciv_trans.c、pciv_trans.h。

3、PCIV Sample：PCI业务层的接口使用样例。基于PCIV的MPI接口和以上两个模块提供的接口。
	相关代码为sample_pciv_host.c、sample_pciv_slave.c、sample_pciv_comm.h。
	目前演示的业务包括：
	1）从片将实时预览图像通过PCI传输到主片并显示
	2）从片将编码码流通过PCI传输到主片并存储
	3）主片从文件中读取待解码码流，通过PCI传输到从片，从片解码后将图像数据再发送到主片显示。


编译方法：
主片：
    make host HIARCH=hi3531
从片：
    make slave HIARCH=hi3532
清除（主从片）：
    make clean


PCIV Sample提供的仅仅是样例代码，部分功能可能实现不全面或有缺陷，请参考使用。