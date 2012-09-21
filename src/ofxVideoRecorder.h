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
    bool setup(string fname, int w, int h, int fps, int quality = JPEG_QUALITYGOOD)
    {
        if(bIsInitialized)
        {
            close();
        }

        width = w;
        height = h;
        frameRate = fps;

        switch(quality) {
            case JPEG_QUALITYSUPERB:
            case JPEG_QUALITYGOOD:
            case JPEG_QUALITYNORMAL:
            case JPEG_QUALITYAVERAGE:
            case JPEG_QUALITYBAD:
                jpeg_quality = quality;
                break;
            default:
                jpeg_quality = MAX(MIN(quality,100),0);
                break;
        }

        fileName = fname;
        bIsInitialized = videoFile.open(fileName+".mjpg",ofFile::WriteOnly,true);
        bufferPath = videoFile.getAbsolutePath();
        moviePath = ofFilePath::getAbsolutePath(fileName);

        if(!isThreadRunning())
            startThread(true, false);
        return bIsInitialized;
    }

    void addFrame(const ofPixels &pixels)
    {
        if(bIsInitialized)
        {
            ofPixels * buffer = new ofPixels(pixels);

            lock();
            frames.push(buffer);
            unlock();
            condition.signal();
        }
    }
    string getMoviePath(){
        return moviePath;
    }

    void close()
    {

        condition.signal();
        while(frames.size() > 0) ofSleepMillis(100);

        bIsInitialized = false;
        condition.signal();

        stopThread();


        if(videoFile.is_open())
        {
            videoFile.close();
        }
    }
    int encodeVideo()
    {
        if(bIsInitialized) close();
        char cmd[8192];
        sprintf(cmd,"bash --login -c 'ffmpeg -y -r %3$d -i %1$s -r %3$d -vcodec copy %2$s; rm %1$s'", bufferPath.c_str(), moviePath.c_str(), frameRate);
        return system(cmd);
    }

    void threadedFunction()
    {
        while(isThreadRunning())
        {
            ofPixels * frame = NULL;
			lock();
			if(!frames.empty()) {
				frame = frames.front();
				frames.pop();
			}
			unlock();

			if(frame){
			    if(frame->getBytesPerPixel() >=3)
                    frame->swapRgb();

				FIBITMAP * bmp = FreeImage_ConvertFromRawBits(frame->getPixels(), width, height, width*frame->getBytesPerPixel(), frame->getBitsPerPixel(), FI_RGBA_RED_MASK,FI_RGBA_GREEN_MASK,FI_RGBA_BLUE_MASK, true);
				FIMEMORY *hmem = FreeImage_OpenMemory();

				FreeImage_SaveToMemory(FIF_JPEG, bmp, hmem, jpeg_quality);
				FreeImage_Unload(bmp);
				bmp = NULL;

				long file_size = FreeImage_TellMemory(hmem);
				videoFile.write((char *)(((FIMEMORYHEADER*)(hmem->data))->data), file_size);
				FreeImage_CloseMemory(hmem);
                hmem = NULL;

				delete frame;
				frame = NULL;
			} else {
				if(bIsInitialized){
				    ofLog(OF_LOG_VERBOSE, "ofxVideoRecorder: threaded function: waiting for mutex condition");
					condition.wait(conditionMutex);
				}else{
				    ofLog(OF_LOG_VERBOSE, "ofxVideoRecorder: threaded function: exiting");
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

    int getWidth(){return width;}
    int getHeight(){return height;}

private:
    string fileName;
    string bufferPath;
    string moviePath;
    ofFile videoFile;
    int width, height, frameRate;
    bool bIsInitialized;
    queue<ofPixels *> frames;
    Poco::Condition condition;
    ofMutex conditionMutex;
    int jpeg_quality;
};
