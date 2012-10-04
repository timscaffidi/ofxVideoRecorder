#pragma once

#include "ofMain.h"
#include <queue>

class ofxVideoRecorder : public ofThread
{
public:
    ofxVideoRecorder()
    {
        bIsInitialized = false;
    }
    bool setup(string fname, int w, int h, int fps, ofImageQualityType quality = OF_IMAGE_QUALITY_BEST)
    {
        if(bIsInitialized)
        {
            close();
        }

        width = w;
        height = h;
        frameRate = fps;
        jpeg_quality = quality;

        fileName = fname;
        bIsInitialized = videoFile.open(fileName+".mjpg",ofFile::WriteOnly,true);
        bufferPath = videoFile.getAbsolutePath();
        moviePath = ofFilePath::getAbsolutePath(fileName);

        if(!isThreadRunning())
            startThread(true, false);
        return bIsInitialized;
    }

    void setQuality(ofImageQualityType q){
        jpeg_quality = q;
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
                ofBuffer data;
                ofSaveImage(*frame,data, OF_IMAGE_FORMAT_JPEG, jpeg_quality);
                videoFile.writeFromBuffer(data);
                data.clear();
                frame->clear();
                delete frame;
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
    ofImageQualityType jpeg_quality;
};
