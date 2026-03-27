CONFIG_MODULE_SIG := n
obj-m += tarakernel.o
PWD := $(CURDIR)

ccflags-y += -g

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean