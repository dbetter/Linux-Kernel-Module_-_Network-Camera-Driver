#ifndef COMMON_H
#define COMMON_H

#define DEVICE_FILE_NAME "/dev/daniel-cam"
#define MAJOR_NUM 250

//ioctl definitions
#define IOCTL_SET_PIC_WIDTH _IOR(MAJOR_NUM, 1, int)
/*
this ioctl is used to communicate picture width from user-space to kernel driver (feeder)
*/

#define IOCTL_SET_PIC_HEIGHT _IOR(MAJOR_NUM, 2, int)
/*
this ioctl is used to communicate picture height from user-space to kernel driver (feeder)
*/

#define IOCTL_SET_END_OF_FRAMES _IOR(MAJOR_NUM, 3, int)
/*
this ioctl is used to communicate end of frames from user-space to kernel driver (feeder)
*/

#define IOCTL_SEND_PIC_WIDTH _IOW(MAJOR_NUM, 4, int)
/*
this ioctl is used to communicate picture width from kernel driver to user-space (grabber)
*/

#define IOCTL_SEND_PIC_HEIGHT _IOW(MAJOR_NUM, 5, int)
/*
this ioctl is used to communicate picture height from kernel driver to user-space (grabber)
*/

#define IOCTL_SEND_END_OF_FRAMES _IOW(MAJOR_NUM, 6, int)
/*
this ioctl is used to communicate end of frames from kernel driver to user-space (grabber)
*/

#endif
