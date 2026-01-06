obj-m += hw_seg_driver.o 
obj-m += hw_btn_driver.o
APP = hw_app
PWD := $(shell pwd)

all: modules apps
modules: 
	make -C /home/linux123/working/kernel M=$(PWD) modules
apps:
	aarch64-linux-gnu-gcc -o $(APP) $(APP).c

clean:
	make -C /home/linux123/working/kernel M=$(PWD) clean
	rm -f $(APP)