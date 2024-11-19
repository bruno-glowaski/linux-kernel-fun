LINUX_SRC:=/lib/modules/$(shell uname -r)/build

obj-m += per-process-tags.o
obj-m += fibonacci-deferred.o

all:
		make -C $(LINUX_SRC) M=$(PWD) modules
clean:
		make -C $(LINUX_SRC) M=$(PWD) clean
compile_commands.json:
		make -C $(LINUX_SRC) M=$(PWD) compile_commands.json
