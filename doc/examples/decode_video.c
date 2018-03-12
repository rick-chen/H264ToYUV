/*
 * simple_example.c
 * Copyright (C) 2018 kychen <keyu.chen@yitu-inc.com>
 *
 * Distributed under terms of the MIT license.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <opencv/cv.h>
#include <opencv/cxcore.h>
#include "libavutil/avstring.h"
#include "libavutil/common.h"
#include "libavutil/file.h"
#include "libavutil/opt.h"
#include <libswscale/swscale.h>


#define INBUF_SIZE 4096


static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,char*filename)  
{  
    FILE *f;  
    int i;  
   
    f=fopen(filename,"a");  
    fprintf(f,"P5\n%d%d\n%d\n",xsize,ysize,255);  
    for(i=0;i<ysize;i++)  
        fwrite(buf + i * wrap,1,xsize,f);  
    fclose(f);  
}  
   
 //通过查找0x000001或者0x00000001找到下一个数据包的头部  
static int _find_head(unsigned char*buffer, int len)  
{  
    int i;  
   
    for(i=512;i<len;i++)  
    {  
        if(buffer[i] == 0 && buffer[i+1] == 0 && buffer[i+2] == 0&& buffer[i+3] == 1)  
            break;  
        if(buffer[i]== 0 && buffer[i+1] == 0 && buffer[i+2] == 1)  
            break;  
    }  
    if (i ==len)  
        return 0;  
    if (i ==512)  
        return 0;  
    return i;  
}  
   
   
 //将文件中的一个数据包转换成AVPacket类型以便ffmpeg进行解码  
#define FILE_READING_BUFFER (1*1024*1024)  
static void build_avpkt(AVPacket *avpkt, FILE *fp)  
{  
    static unsigned char buffer[1*1024*1024];  
    static int readptr = 0;  
    static int writeptr = 0;  
    int len,toread;  
   
    int nexthead;  
   
    if (writeptr- readptr < 200 * 1024)  
    {  
        memmove(buffer, &buffer[readptr],writeptr - readptr);  
        writeptr -= readptr;  
        readptr = 0;  
        toread = FILE_READING_BUFFER - writeptr;  
        len = fread(&buffer[writeptr], 1,toread, fp);  
        writeptr += len;  
    }  
   
    nexthead = _find_head(&buffer[readptr], writeptr-readptr);  
    if (nexthead== 0)  
    {  
        printf("failedfind next head...\n");  
        nexthead = writeptr - readptr;  
    }  
   
    avpkt->size = nexthead;  
    avpkt->data = &buffer[readptr];  
    readptr += nexthead;  
   
}  
   
static void video_decode_example(const char *outfilename, const char *filename)  
{  
    AVCodec *codec;  
    AVDictionary *opts = NULL;
    AVCodecContext *c= NULL;  
    int frame,got_picture, len;  
    FILE *f, *fout;  
    AVFrame *picture;  
    uint8_t inbuf[INBUF_SIZE +AV_INPUT_BUFFER_PADDING_SIZE];  
    char buf[1024];  
    AVPacket avpkt;  
   
    av_init_packet(&avpkt);  
   
    /* set end ofbuffer to 0 (this ensures that no overreading happens for damaged mpeg streams)*/  
    memset(inbuf + INBUF_SIZE, 0,AV_INPUT_BUFFER_PADDING_SIZE);  
   
    printf("Videodecoding\n");  
    opts = NULL;  
    //av_dict_set(&opts,"b", "2.5M", 0);  
    /* find the h264video decoder */  
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);  
    if (!codec){  
        fprintf(stderr, "codecnot found\n");  
        return ;  
    }  
   
    c = avcodec_alloc_context3(codec);  
    picture= av_frame_alloc();  
   
    if(codec->capabilities&AV_CODEC_CAP_TRUNCATED)  
    c->flags|= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */  
   
    /* For somecodecs, such as msmpeg4 and mpeg4, width and height 
    MUST be initialized there because thisinformation is not 
    available in the bitstream. */  
   
    /* open it */  
    if(avcodec_open2(c, codec, NULL) < 0) {  
        fprintf(stderr, "couldnot open codec\n");  
        exit(1);  
    }  
   
   
    fout=fopen(outfilename,"wb");  
    /* the codec givesus the frame size, in samples */  
   
    f = fopen(filename, "rb");  
    if (!f) {  
        fprintf(stderr, "couldnot open %s\n", filename);  
        exit(1);  
    }  
     //解码与显示需要的辅助的数据结构，需要注意的是，AVFrame必须经过alloc才能使用，不然其内存的缓存空间指针是空的，程序会崩溃  
    AVFrame frameRGB;  
    //IplImage *showImage =cvCreateImage(cvSize(352,288),8,3);  
    avpicture_alloc((AVPicture*)&frameRGB,AV_PIX_FMT_YUV420P,1920,1080);  
    //cvNamedWindow("decode");  
   
    frame = 0;  
    for(;;) {  
   
        build_avpkt(&avpkt, f);  
   
        if(avpkt.size == 0)  
            break;  
   
        while(avpkt.size > 0) {  
            len = avcodec_decode_video2(c,picture, &got_picture, &avpkt);//解码每一帧  
            if(len < 0) {  
                fprintf(stderr, "Error while decoding frame %d\n",frame);  
                break;  
            }  
            if(got_picture) {  
                printf("savingframe %3d\n", frame);  
                fflush(stdout);  
   
                /* thepicture is allocated by the decoder. no need to free it */  
               //将YUV420格式的图像转换成RGB格式所需要的转换上下文  
                /*
                struct SwsContext* scxt =sws_getContext(picture->width,picture->height,AV_PIX_FMT_YUV420P,  
                                                  picture->width,picture->height,AV_PIX_FMT_RGB24,  
                                                  2,NULL,NULL,NULL);  
                                                  */
                /*
                if(scxt != NULL)  
                {  
                    sws_scale(scxt,(const uint8_t * const *)picture->data,picture->linesize,0,c->height,frameRGB.data,frameRGB.linesize);//图像格式转换  
                    //showImage->imageSize =frameRGB.linesize[0];//指针赋值给要显示的图像  
                    //showImage->imageData = (char *)frameRGB.data[0];  
                    //cvShowImage("decode",showImage);//显示  
                    //cvWaitKey(0.5);//设置0.5s显示一帧，如果不设置由于这是个循环，会导致看不到显示出来的图像  
                }  
                */
   
                sprintf(buf,outfilename,frame);  
   
                printf("width :%d, height :%d ",c->width, c->height);
                printf("wrap1 :%d, wrap2 :%d , wrap3 : %d",picture->linesize[0], picture->linesize[1],  picture->linesize[2]);
                /*
                pgm_save(picture->data[0],picture->linesize[0],  
                c->width,c->height, buf);  
                printf("filename %s", buf);
                printf("filename %s", (char *)fout);
                pgm_save(picture->data[0],picture->linesize[0],  
                c->width,c->height, buf);  
                pgm_save(picture->data[0],picture->linesize[0],  
                c->width,c->height, buf);  
                */
                FILE* f = fopen((char*)buf, "a");
                fwrite(picture->data[0], 1, 1920* 1080, f);
                fwrite(picture->data[1], 1, 1920* 1080, f);
                fwrite(picture->data[2], 1, 1920* 1080, f);
                fclose(f);
                frame++;  
            }  
            avpkt.size -= len;  
            avpkt.data += len;  
        }  
    }  
   
    /* some codecs,such as MPEG, transmit the I and P frame with a 
    latency of one frame. You must do thefollowing to have a 
    M
    M
    M
    chance to get the last frame of the video */  
    /*
    avpkt.data = NULL;  
    avpkt.size = 0;  
    len = avcodec_decode_video2(c, picture,&got_picture, &avpkt);  
    if(got_picture) {  
        printf("savinglast frame %3d\n", frame);  
        fflush(stdout);  
        */
   
        /* the pictureis allocated by the decoder. no need to 
        free it */  
    /*
        sprintf(buf, outfilename, frame);  
        //pgm_save(picture->data[0],picture->linesize[0],  
        //       c->width, c->height, fout);  
        pgm_save(picture->data[0],picture->linesize[0],c->width, c->height, (char *)fout);  
        pgm_save(picture->data[1],picture->linesize[1],c->width/2, c->height/2, (char *)fout);  
        pgm_save(picture->data[2],picture->linesize[2],c->width/2, c->height/2, (char *)fout);  
   
        frame++;  
    }  
    */
   
    fclose(f);  
//  fclose(fout);  
   
    avcodec_close(c);  
    av_free(c);  
    av_free(picture);  
    printf("\n");  
}  
   
   
int main(int argc, char* argv[])  
{  
    avcodec_register_all();//注册所有的编解码器，一定要注意，如果没有这行代码则会出错，提示没有找不到编解码器  
    video_decode_example("%3d.pgm","test.264");//可以使用x264编码出来的264文件  
    system("pause");  
    return 0;  
}  
