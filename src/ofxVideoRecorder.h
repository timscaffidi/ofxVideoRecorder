#pragma once

#include "ofMain.h"
#include <queue>
#include "FreeImage.h"

FI_STRUCT (FIMEMORYHEADER)
{
    /// remember to delete the buffer
    bool delete_me;
    /// file length
    long filelen;
    /// buffer size
    long datalen;
    /// current position
    long curpos;
    /// start buffer address
    void *data;
};

class ofxVideoRecorder : public ofThread
{
public:
    ofxVideoRecorder()
    {
        bIsInitialized = false;
    }
    bool setup(string fname, int w, int h, int fps)
    {
        if(bIsInitialized)
        {
            close();
        }

        width = w;
        height = h;
        frameRate = fps;
        fileName = fname;
        bIsInitialized = videoFile.open(fileName+".mjpg",ofFile::WriteOnly,true);
        if(!isThreadRunning())
            startThread(true, false);
        return bIsInitialized;
    }

    void addFrame(const ofPixels pixels)
    {
        if(bIsInitialized)
        {
            ofPixels * buffer = new ofPixels(pixels);

            lock();
            frames.push(buffer);
            unlock();
        }
    }

    void close()
    {
        bIsInitialized = false;
        waitForThread(false);
        if(videoFile.is_open())
        {
            videoFile.close();
        }
    }
    int encodeVideo()
    {
        if(bIsInitialized) close();
        char cmd[256];
        sprintf(cmd,"ffmpeg -y -r %2$d -i data/%1$s.mjpg -r %2$d -vcodec copy data/%1$s; rm data/%1$s.mjpg",fileName.c_str(), frameRate);
        return system(cmd);
    }

    void threadedFunction()
    {
        while(isThreadRunning())
        {
            register unsigned char * pixels;
            register int index;
            register unsigned char temp;
            register int cnt = width*height;
            register int byteCount = 3;
            ofPixels * frame = NULL;
            if(!frames.empty()) {
                lock();
                frame = frames.front();
                frames.pop();
                unlock();

                if(frame)
                {
                    pixels = frame->getPixels();
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


                    long file_size = FreeImage_TellMemory(hmem);
                    videoFile.write((char *)(((FIMEMORYHEADER*)(hmem->data))->data), file_size);
                    FreeImage_Unload(bmp);
                    FreeImage_CloseMemory(hmem);

                    delete frame;
                }

            }
            else {
                if(bIsInitialized){
                    ofSleepMillis(1.0/(float)frameRate);
                }
                else {
                 break;
                }
            }
        }
    }

    int getNumFramesInQueue()
    {
        return frames.size();
    }
    bool isInitialized()
    {
        return bIsInitialized;
    }

private:
    string fileName;
    ofFile videoFile;
    int width, height, frameRate;
    bool bIsInitialized;
    queue<ofPixels *> frames;

};
