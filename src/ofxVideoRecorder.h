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
    void setFfmpegLocation(string loc) { ffmpegLocation = loc; }
    bool setup(string fname, int w, int h, int fps);
    bool setupCustomOutput(int w, int h, int fps, string outputString);
    void setQuality(ofImageQualityType q);
    void addFrame(const ofPixels &pixels);
    void close();
    void threadedFunction();
    
    int getNumFramesInQueue(){ return frames.size(); }
    bool isInitialized(){ return bIsInitialized; }
    
    string getMoviePath(){ return moviePath; }
    int getWidth(){return width;}
    int getHeight(){return height;}

private:
    string fileName;
    string moviePath;
    string pipePath;
    string ffmpegLocation;
    int width, height, frameRate;
    bool bIsInitialized;
    queue<ofPixels *> frames;
    Poco::Condition condition;
    ofMutex conditionMutex;
    execThread ffmpegThread;
    int videoPipeFd[2];
    int fp;
    static int pipeNumber;
};
