/*
 * Owner: Daniel Better
 *
 * grabber.c:
 * to compile use: gcc -o grabber grabber.c -lavformat -lavcodec -lswscale -lz -lm `sdl-config --cflags --libs`
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include "common.h"

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

//global variables deceleration
static int kerfd; 
static int DEBUG = -1;


#define PAL_WIDTH 640
#define PAL_HEIGHT 480


/*****************************************************************************/
/******************************** PROTOTYPES *********************************/
/*****************************************************************************/

int display_video();
void img_convert(AVPicture * target , int targetFmt, AVPicture * source ,int sourceFmt,int w, int h);

/*****************************************************************************/
/***************************    Methods       ********************************/
/*****************************************************************************/


void img_convert(AVPicture * target , int targetFmt, AVPicture * source ,int sourceFmt,int w, int h)
{
        static struct SwsContext *img_convert_ctx = NULL;

        if(img_convert_ctx == NULL)
        {
                img_convert_ctx = sws_getContext(w, h, sourceFmt, w, h, targetFmt, SWS_BICUBIC, NULL, NULL, NULL);
        }

        sws_scale(img_convert_ctx, (const uint8_t * const*)source->data, source->linesize, 0, h, target->data, target->linesize);
}


/*
 * Display video from driver
 */
int display_video()
{
    AVFrame *pFrame, *aFrame;
	  int  y;
	  uint8_t *buffer;
 	  struct SwsContext *sws_ctx; 
    
	  char *pKerFrame; 
    int   numBytes;
    int  *pWidth, *pHeight;
	  int   width, height;
          SDL_Rect  rect;
          AVPicture pict;
          AVPicture *frm;
          SDL_Overlay *bmp;
          SDL_Surface *screen;
	  int ret_val;
  	int endFlag = 0;
	  uint8_t *tmp;
	  static int sum = 0;



     //open driver		  
	  kerfd = open(DEVICE_FILE_NAME, O_RDWR); 
	  if (kerfd < 0)
	  {
		printf("Error, can't open %s device file\n", DEVICE_FILE_NAME);	
	  }

     //get frame width	
    ret_val = ioctl(kerfd, IOCTL_SEND_PIC_WIDTH, &width); 
	  if (ret_val < 0)
	  {
		printf("ioctl_set_pic_width failed %d\n", ret_val);
		return;
	  }
	
     //get frame height	  
	  ret_val = ioctl(kerfd, IOCTL_SEND_PIC_HEIGHT, &height);
	  if (ret_val < 0)
	  {
		printf("ioctl_set_pic_height failed %d\n", ret_val);	
		return;
	  }

    // Register all formats and codecs
    av_register_all();


    //initalize screen
          if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
          {
                printf("Could not initialize SDL %s\n", SDL_GetError());
                return;
          }
 
    //allocate frame memory in user-space
	  pKerFrame = (char*)malloc(width*height*3);
	  if (!pKerFrame)
	  {
		printf("Error allocationg memory, quiting\n");
		goto FreeLabel;
	  }

		
    //set the screen to display the video
          #ifndef __DARWIN__
	                screen = SDL_SetVideoMode(PAL_WIDTH, PAL_HEIGHT, 0, 0);
          #else
                        screen = SDL_SetVideoMode(PAL_WIDTH, PAL_HEIGHT, 24, 0);
          #endif

          if(!screen)
          {
                printf("SDL: could not set video mode - exiting\n");
                goto FreeLabel;
          }

	
          bmp = SDL_CreateYUVOverlay(width, height, SDL_YV12_OVERLAY, screen);



    //ioctl to check if there are no more frames
	  ret_val = ioctl(kerfd, IOCTL_SEND_END_OF_FRAMES, &endFlag);
	  if (ret_val <= 0)
	  {
		printf("IOCTL_SEND_END_OF_FRAMES %d\n", ret_val);	
		goto FreeLabel;
	  }		
	  if (DEBUG == 2) printf("end of frames returned %d\n", endFlag);

   
    //allocate frame memory 	
          pFrame=avcodec_alloc_frame();
          if(pFrame == NULL) return;
 

    //loop until there are no more frames to grab	
         while (endFlag != 0)
         {		

	   //read the frame data from the driver. pKerFrame will now point to a given frame
		ret_val = read(kerfd, pKerFrame, (width*height*3));
		sum++; 
		if (ret_val <= 0)
		{
			printf("overall read %d frames from the driver\n", sum);
			goto FreeLabel;
		}	


		avpicture_fill((AVPicture *)pFrame, pKerFrame, PIX_FMT_RGB24, width, height);

		SDL_LockYUVOverlay(bmp);

    pict.data[0] = bmp->pixels[0];
	  pict.data[1] = bmp->pixels[2];
    pict.data[2] = bmp->pixels[1];

    pict.linesize[0] = bmp->pitches[0];
    pict.linesize[1] = bmp->pitches[2];
    pict.linesize[2] = bmp->pitches[1];

    frm = (AVPicture *)pFrame;

 	  //convert frame from given RGB format to YUV format that SDL uses
    img_convert(&pict, PIX_FMT_YUV420P, frm, PIX_FMT_RGB24, width,height);

    SDL_UnlockYUVOverlay(bmp);
    rect.x = 0;
    rect.y = 0;
    rect.w = width;
    rect.h = height;
    SDL_DisplayYUVOverlay(bmp, &rect);

		ret_val = ioctl(kerfd, IOCTL_SEND_END_OF_FRAMES, &endFlag);
		if (ret_val < 0)
		{
			printf("IOCTL_SEND_END_OF_FRAMES %d\n", ret_val);	
		}

		if (DEBUG == 2)
		{
			printf("end_of_frames returnes %d\n", endFlag);
		}	
        }

	
   FreeLabel:
   // Free the kernel frame
	free(pKerFrame);

   // Close the device
	close(kerfd);

   //close SDL and free memory
	SDL_Quit();	
}


void main()
{
        display_video();
        return;
}


