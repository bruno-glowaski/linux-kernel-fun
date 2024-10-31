obj-m += per-process-tags.o

all:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
compile_commands.json:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) compile_commands.json
