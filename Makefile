obj-m += driver.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -o grabber grabber.c -lavformat -lavcodec -lswscale -lz -lm `sdl-config --cflags --libs` -lavutil
	gcc -o feeder feeder.c -lavformat -lavcodec -lswscale -lz -lm `sdl-config --cflags --libs` -lavutil

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm feeder grabber daniel-cam
