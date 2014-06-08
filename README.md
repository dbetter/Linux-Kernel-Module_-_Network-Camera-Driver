
# Owner: Daniel Better
## Linux kernel module implementation - Display network camera frame captures as a movie 

##Assumptions:
 1. The feeder will wait until the grabber starts working and once the grabber has been started it'll start feeding frames to the driver who will then communicate the frames to the grabber for display. 
 2. In absence of a network camera, the code was modified to recieve as an input to the feeder a movie-file (as to demonstrate the feedback a net camera would give) 

 
##The program includes 3 files: 
1. feeder.c
2. driver.c 
3. grabber.c 

To run the programs, use the Makefile included by running:

	Make

in the folder which the files were extrected to.

after the three files are compiled, it's neccessrry to insert the kernel module by:

	insmod driver.ko

and afterwords: (Major number is 250)

	mknod /dev/daniel-cam c 250 0

to run the application it is neccesrry to be logged in as SU. 
then all that is needed is to run the following command from one terminal is:
	
	./feeder <video-file-path>" from one terminal 

and aftewords run the following command from another terminal: from another terminal.

	./grabber




