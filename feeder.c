/*
 * Owner: Daniel Better
 *
 * feeder.c:
 * to compile feeder.c use: gcc -o feeder feeder.c -lavformat -lavcodec -lswscale -lz -lm `sdl-config --cflags --libs`
 *
 */


#include <fcntl.h>
#include <unistd.h>
#include <libswscale/swscale.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
#include <libavutil/avstring.h>
#include <libavutil/mathematics.h>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <math.h>
#include "common.h"


/*****************************************************************************/
/********************************   METHODS  *********************************/
/*****************************************************************************/

void SaveFrame(AVFrame *pFrame, int width, int height, int fd)
{

	write(fd, pFrame->data[0], (width*height*3)); 
}



/******************************************************************************/
/********************************   Main    ***********************************/
/******************************************************************************/
int main(int argc, char *argv[])
{
	AVFormatContext *pFormatCtx = NULL;
	int i, videoStream;
	AVCodecContext *pCodecCtx;
	AVCodec *pCodec;
	AVFrame *pFrame;
	AVFrame *pFrameRGB;
	struct SwsContext * pSwsCtx;
	AVPacket packet;
	int frameFinished;
	int numBytes;
	uint8_t *buffer;
	int ret_val;
	int kerfd;		//file descriptor used to access the char device


	if (argc < 2)
	{
		printf("Please provide a proper video file:\n");
		return -1;
	}

   // Register all formats and codecs
	av_register_all();

   // Open video file
	if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
		return -1; // Couldn't open file

   // Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
		return -1; // Couldn't find stream information

   // Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, argv[1], 0);

   // Find the first video stream
	videoStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) 
		{
			videoStream = i;
			break;
		}
	if (videoStream == -1)
		return -1; // Didn't find a video stream

   // Get a pointer to the codec context for the video stream
	pCodecCtx = pFormatCtx->streams[videoStream]->codec;

   // Find the decoder for the video stream
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}
   // Open codec
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
		return -1; // Could not open codec

   // Allocate video frame
	pFrame = avcodec_alloc_frame();

   // Allocate an AVFrame structure
	pFrameRGB = avcodec_alloc_frame();
	if (pFrameRGB == NULL)
		return -1;

   // Determine required buffer size and allocate buffer to be filled with picture later-on
	numBytes = avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
	buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));


	kerfd = open(DEVICE_FILE_NAME, O_RDWR); 
	if (kerfd < 0)
	{
		printf("Error, can't open %s device file\n", DEVICE_FILE_NAME);	
		return -1;
	}

   // communicate with the kernel and pass the value of the picture's width
	ret_val = ioctl(kerfd, IOCTL_SET_PIC_WIDTH, &(pCodecCtx->width));
	if (ret_val < 0)
	{
		printf("ioctl_set_pic_width failed %d\n", ret_val);	
	}


   //communicate with the kernel and pass the value of the picture's height
	ret_val = ioctl(kerfd, IOCTL_SET_PIC_HEIGHT, &(pCodecCtx->height));
	if (ret_val < 0)
	{
		printf("ioctl_set_pic_height failed %d\n", ret_val);	
	}


   // Assign appropriate parts of buffer to image planes in pFrameRGB
   // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
   // of AVPicture
	avpicture_fill((AVPicture *) pFrameRGB, buffer, PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);

	pSwsCtx = sws_getContext(pCodecCtx->width,
			pCodecCtx->height, pCodecCtx->pix_fmt,
			pCodecCtx->width, pCodecCtx->height,
			PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL);

	if (pSwsCtx == NULL)
        {
		fprintf(stderr, "Cannot initialize the sws context\n");
		return -1;
	}

	while (av_read_frame(pFormatCtx, &packet) >= 0) 
	{
	    // Is this a packet from the video stream?
		if (packet.stream_index == videoStream) {
			// Decode video frame
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

			// Did we get a video frame?
			if (frameFinished)
			{
			   // Convert the image from its native format to RGB
				sws_scale(pSwsCtx, (const uint8_t * const *) pFrame->data,
							pFrame->linesize, 0, pCodecCtx->height,
							pFrameRGB->data,
							pFrameRGB->linesize);
			   // SaveFrame to kernel
				SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, kerfd);
			}
		}

	 // Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}
	
 //ioctl to announce we're finished with our video (Frames)
	int temp = 1;  
	ret_val = ioctl(kerfd, IOCTL_SET_END_OF_FRAMES, &temp);
	if (ret_val < 0)
	{
		printf("IOCTL_SET_END_OF_FRAMES failed %d\n", ret_val);	
	}
	else
	{
		printf("Finished transfaring all the frame data to the driver\n");
	}

 // Free the RGB image
	av_free(buffer);
	av_free(pFrameRGB);

 // Free the YUV frame
	av_free(pFrame);

 // Close the codec
	avcodec_close(pCodecCtx);

 // Close the video file
	avformat_close_input(&pFormatCtx);

 // Close the device
	close(kerfd);
	return 0;
}

