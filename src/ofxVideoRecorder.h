#pragma once

#include "ofMain.h"
#include <queue>

class execThread : public ofThread{
public:
    execThread();
    void setup(string command);
    void threadedFunction();
private:
    string execCommand;
};

class ofxVideoRecorder : public ofThread
{
public:
    ofxVideoRecorder();
    
    bool setup(string fname, int w, int h, int fps, ofImageQualityType quality, bool onTheFly);
    void setQuality(ofImageQualityType q);
    void addFrame(const ofPixels &pixels);
    void close();
    int encodeVideo();
    void threadedFunction();
    
    int getNumFramesInQueue(){ return frames.size(); }
    bool isInitialized(){ return bIsInitialized; }
    
    string getMoviePath(){ return moviePath; }
    int getWidth(){return width;}
    int getHeight(){return height;}

private:
    string fileName;
    string bufferPath;
    string moviePath;
    string pipePath;
    ofFile videoFile;
    int width, height, frameRate;
    bool bIsInitialized;
    queue<ofPixels *> frames;
    Poco::Condition condition;
    ofMutex conditionMutex;
    ofImageQualityType jpeg_quality;
    bool encodeOnTheFly;
    execThread ffmpegThread;
    int videoPipeFd[2];
    int fp;
    static int pipeNumber;
};
