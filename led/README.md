Linux驱动学习

LED驱动

编译驱动

make

编译APP

arm-linux-gnueabihf-gcc led_APP.c -o led_APP

使用步骤

1、加载驱动

insmod led.ko

2、创建节点

mkmod /dev/led c 200 0

创建完成后在dev目录可看到加载的led驱动

3、运行APP测试

APP       驱动名称   第一盏灯  灯状态（1打开，0关闭）

./led_APP /dev/led 1 0



