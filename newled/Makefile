KERNELDIR := /home/han/linux/kernel/ebf_6ull_linux
CURRENT_PATH := $(shell pwd)
obj-m := newled.o

build: kernel_modules

kernel_modules: 
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
clean: 
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean