#pragma once

#include "ofMain.h"
#include <unistd.h>
#include "FreeImage.h"

#define exchange(a,b)   __asm mov eax, a \
                        __asm xchg eax, b \
                        __asm mov a, eax

class ImageListNode
{
public:
    ImageListNode * next;
    int channel;
    bool used;
    unsigned char * pixels;
    ImageListNode()
    {
        next = NULL;
        used = false;
        channel = 0;
    }
};

class ImageListQueue
{
private:
    ImageListNode * head;
    ImageListNode * tail;
    int w, h, nPixels;

    bool bLock;
    bool lock()
    {
        if(bLock)
        {
            return false;
        }
        else return bLock = true;
    }

    void unlock()
    {
        bLock = false;
    }

public:
    ImageListQueue()
    {
        head = NULL;
        tail = NULL;
    }

    ImageListQueue(int width, int height)
    {
        w = width;
        h = height;
        head = NULL;
        tail = NULL;
        bLock = false;
        nPixels = width * height * 3;
    }

    void addImage(unsigned char * pixels, int channel)
    {

        ImageListNode * r = new ImageListNode();
        r->pixels = new unsigned char [nPixels];
        memcpy((void*)(r->pixels),(void*)pixels,sizeof(unsigned char) * nPixels);
        r->channel = channel;
        while(!lock());
        if(tail == NULL && head == NULL)
        {
            tail = r;
            head = r;
        }
        else
        {
            tail->next = r;
            tail = tail->next;
        }
        unlock();
    }

    ImageListNode * removeImage()
    {
        ImageListNode * r = NULL;
        if(lock())
        {
            r = head;
            if(head != NULL)
            {
                head = head->next;
            }
            if(tail == r) tail = NULL;
            unlock();
        }
        return r;
    }

    int getHeight()
    {
        return h;
    }
    int getWidth()
    {
        return w;
    }
};

class threadedVidSaver : public ofThread
{
private:
    bool                bInited;
    ImageListQueue      imageQueue;
    int                 width, height, frameRate;
    ImageListNode     * frameToSave;
    string fileName;
    int                 * framesRecorded;
    int                 framesInQueue;
    ofFile              videoFile;
public:
    threadedVidSaver()
    {
        bInited = false;
    }

    void setup(string fname, int w, int h, int fps)
    {
        framesRecorded = 0;

        fileName = fname;

        width = w;
        height = h;
        frameRate = fps;

        framesInQueue = 0;
        imageQueue = ImageListQueue(width, height);

        videoFile.open(fname+".mjpg",ofFile::WriteOnly,true);

        bInited = true;
    }

    void threadedFunction()
    {
        while(threadRunning && (frameToSave = imageQueue.removeImage()) != NULL)
        {

            //swapRGB because bit reordering seemsbroken in FI_convertFromRawBits
                register unsigned char * pixels = frameToSave->pixels;
                register int index = 0;
                register unsigned char temp;
                register int cnt = width*height;
                register int byteCount = 3;
                while (cnt >0 )
                {
                    index               = cnt*byteCount;
                    temp				= pixels[index];
                    pixels[index]		= pixels[index+2];
                    pixels[index+2]		= temp;
                    cnt--;
                }

            FIBITMAP * bmp = FreeImage_ConvertFromRawBits(pixels, width, height, width*3, 24, FI_RGBA_RED_MASK,FI_RGBA_GREEN_MASK,FI_RGBA_BLUE_MASK, true);
            FIMEMORY *hmem = FreeImage_OpenMemory();

            FreeImage_SaveToMemory(FIF_JPEG, bmp, hmem, 100);

            FreeImage_Unload(bmp);

            long file_size = FreeImage_TellMemory(hmem);
            FreeImage_SeekMemory(hmem, 0L, SEEK_SET);
            char * jpeg_data = new char[file_size];
            FreeImage_ReadMemory(jpeg_data,file_size,1,hmem);
            FreeImage_CloseMemory(hmem);

            videoFile.write(jpeg_data, file_size);

            delete frameToSave->pixels;
            delete frameToSave;
            delete jpeg_data;
            frameToSave = NULL;
            framesInQueue--;
            framesRecorded++;

        }
        stopThread();
    }

    void addFrame(const ofPixels pixels) {
        addFrame((unsigned char *)pixels.getPixels());
    }

    void addFrame(unsigned char * pixels)
    {
        if (!bInited)
        {
            fprintf(stderr,"ofxThreadedVideoSaver: init has not been called!\n");
            return;
        }

        if (framesInQueue*width*height*3 < 1073741824) //only allow up to 1 gig of memory
        {
            imageQueue.addImage(pixels, 0);
            framesInQueue++;
        }

        if (!isThreadRunning())
        {
            startThread(false, false);   // blocking, verbose
        }

    }

    void saveFrames()
    {
        startThread(false, false) ;
    }

    int getNumFramesInQueue()
    {
        return framesInQueue;
    }

    void close() {
        waitForThread(false);
        videoFile.close();
        bInited =false;
    }

    int encodeVideo()
    {
        close();
        char cmd[256];
        sprintf(cmd,"ffmpeg -y -r %2$d -i data/%1$s.mjpg -r %2$d -vcodec copy data/%1$s; rm data/%1$s.mjpg",fileName.c_str(), frameRate);
        //sprintf(cmd,"ffmpeg -y -r 15 -i data/%1$s%%d%2$s -r 15 -vcodec copy data/%1$s.mov; rm data/%1$s*.%2$s",getFilePrefix(i).c_str(), getFileExt().c_str());
        return system(cmd);
    }

};
