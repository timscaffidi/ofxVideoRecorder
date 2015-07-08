#pragma once

#include "ofMain.h"
#include "Poco/Condition.h"
#include <set>

template <typename T>
struct lockFreeQueue {
    lockFreeQueue(){
        list.push_back(T());
        iHead = list.begin();
        iTail = list.end();
    }
    void Produce(const T& t){
        list.push_back(t);
        iTail = list.end();
        list.erase(list.begin(), iHead);
    }
    bool Consume(T& t){
        typename TList::iterator iNext = iHead;
        ++iNext;
        if (iNext != iTail)
        {
            iHead = iNext;
            t = *iHead;
            return true;
        }
        return false;
    }
    int size() { return distance(iHead,iTail)-1; }
    typename std::list<T>::iterator getHead() {return iHead; }
    typename std::list<T>::iterator getTail() {return iTail; }


private:
    typedef std::list<T> TList;
    TList list;
    typename TList::iterator iHead, iTail;
};

class execThread : public ofThread{
public:
    execThread();
    void setup(string command);
    void threadedFunction();
private:
    string execCommand;
};

struct audioFrameShort {
    short * data;
    int size;
};

class ofxVideoDataWriterThread : public ofThread {
public:
    ofxVideoDataWriterThread();
//    void setup(ofFile *file, lockFreeQueue<ofPixels *> * q);
    void setup(string filePath, lockFreeQueue<ofPixels *> * q);
    void threadedFunction();
    void signal();
    bool isWriting() { return bIsWriting; }
    void close() { bClose = true; stopThread(); signal(); }
private:
    ofMutex conditionMutex;
    Poco::Condition condition;
//    ofFile * writer;
    string filePath;
    int fd;
    lockFreeQueue<ofPixels *> * queue;
    bool bIsWriting;
    bool bClose;
};

class ofxAudioDataWriterThread : public ofThread {
public:
    ofxAudioDataWriterThread();
//    void setup(ofFile *file, lockFreeQueue<audioFrameShort *> * q);
    void setup(string filePath, lockFreeQueue<audioFrameShort *> * q);
    void threadedFunction();
    void signal();
    bool isWriting() { return bIsWriting; }
    void close() { bClose = true; stopThread(); signal();  }
private:
    ofMutex conditionMutex;
    Poco::Condition condition;
//    ofFile * writer;
    string filePath;
    int fd;
    lockFreeQueue<audioFrameShort *> * queue;
    bool bIsWriting;
    bool bClose;
};

class ofxVideoRecorder
{
public:
    ofxVideoRecorder();
    bool setup(string fname, int w, int h, float fps, int sampleRate=0, int channels=0, bool sysClockSync=false, bool silent=false);
    bool setupCustomOutput(int w, int h, float fps, string outputString, bool sysClockSync=false, bool silent=false);
    bool setupCustomOutput(int w, int h, float fps, int sampleRate, int channels, string outputString, bool sysClockSync=false, bool silent=false);
    void setQuality(ofImageQualityType q);
    void addFrame(const ofPixels &pixels);
    void addAudioSamples(float * samples, int bufferSize, int numChannels);

    void start();
    void close();
    void setPaused(bool bPause);

    void setFfmpegLocation(string loc) { ffmpegLocation = loc; }
    void setVideoCodec(string codec) { videoCodec = codec; }
    void setAudioCodec(string codec) { audioCodec = codec; }
    void setVideoBitrate(string bitrate) { videoBitrate = bitrate; }
    void setAudioBitrate(string bitrate) { audioBitrate = bitrate; }

    void setPixelFormat( string pixelF){ //rgb24 || gray, default is rgb24
        pixelFormat = pixelF;
    };

    unsigned long long getNumVideoFramesRecorded() { return videoFramesRecorded; }
    unsigned long long getNumAudioSamplesRecorded() { return audioSamplesRecorded; }

    int getVideoQueueSize(){ return frames.size(); }
    int getAudioQueueSize(){ return audioFrames.size(); }

    bool isInitialized(){ return bIsInitialized; }
    bool isRecording() { return bIsRecording; };
    bool isPaused() { return bIsPaused; };
    bool isSyncAgainstSysClock() { return bSysClockSync; };

    string getMoviePath(){ return moviePath; }
    int getWidth(){return width;}
    int getHeight(){return height;}

private:
    string fileName;
    string moviePath;
    string videoPipePath, audioPipePath;
    string ffmpegLocation;
    string videoCodec, audioCodec, videoBitrate, audioBitrate, pixelFormat;
    int width, height, sampleRate, audioChannels;
    float frameRate;

    bool bIsInitialized;
    bool bRecordAudio;
    bool bRecordVideo;
    bool bIsRecording;
    bool bIsPaused;
    bool bFinishing;
    bool bIsSilent;

    bool bSysClockSync;
    float startTime;
    float recordingDuration;
    float totalRecordingDuration;
    float systemClock();

    lockFreeQueue<ofPixels *> frames;
    lockFreeQueue<audioFrameShort *> audioFrames;
    unsigned long long audioSamplesRecorded;
    unsigned long long videoFramesRecorded;
    ofxVideoDataWriterThread videoThread;
    ofxAudioDataWriterThread audioThread;
    execThread ffmpegThread;
//    ofFile videoPipe, audioPipe;
    int videoPipeFd, audioPipeFd;
    int pipeNumber;

    static set<int> openPipes;
    static int requestPipeNumber();
    static void retirePipeNumber(int num);
};
