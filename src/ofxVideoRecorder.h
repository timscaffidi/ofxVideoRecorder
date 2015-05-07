#pragma once

#include "ofMain.h"
#include <set>

#ifdef TARGET_WIN32
#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#endif

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
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
    void setup(string filePath, lockFreeQueue<ofPixels *> * q);
#endif
#ifdef TARGET_WIN32
	void setup(HANDLE pipeHandle, lockFreeQueue<ofPixels *> * q);
	HANDLE videoHandle;
	HANDLE fileHandle;
#endif
    void threadedFunction();
    void signal();
    bool isWriting() { return bIsWriting; }
    void close() { bClose = true; stopThread(); signal(); }
private:
    ofMutex conditionMutex;
    Poco::Condition condition;
    string filePath;
    int fd;
    lockFreeQueue<ofPixels *> * queue;
    bool bIsWriting;
    bool bClose;
};

class ofxAudioDataWriterThread : public ofThread {
public:
    ofxAudioDataWriterThread();
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
    void setup(string filePath, lockFreeQueue<audioFrameShort *> * q);
#endif
#ifdef TARGET_WIN32
	void setup(HANDLE pipeHandle, lockFreeQueue<audioFrameShort *> * q);
	HANDLE audioHandle;
	HANDLE fileHandle;
#endif
    void threadedFunction();
    void signal();
    bool isWriting() { return bIsWriting; }
    void close() { bClose = true; stopThread(); signal();  }
private:
    ofMutex conditionMutex;
    Poco::Condition condition;
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
    bool setup(string fname, int w, int h, float fps, int sampleRate=0, int channels=0);
	bool setupCustomOutput(int w, int h, float fps, string outputLocation);
	bool setupCustomOutput(int w, int h, float fps, int sampleRate, int channels, string outputLocation);
	bool runCustomScript(string script);
    void setQuality(ofImageQualityType q);
    void addFrame(const ofPixels &pixels);
    void addAudioSamples(float * samples, int bufferSize, int numChannels);
    void close();

    void setFfmpegLocation(string loc) { ffmpegLocation = loc; }
	void setMovFileExtension(string extension) { movFileExt = extension; }
	void setAudioFileExtension(string extension) { audioFileExt = extension; }
    void setVideoCodec(string codec) { videoCodec = codec; }
    void setAudioCodec(string codec) { audioCodec = codec; }
    void setVideoBitrate(string bitrate) { videoBitrate = bitrate; }
    void setAudioBitrate(string bitrate) { audioBitrate = bitrate; }

    void setPixelFormat( string pixelF){ //rgb24 || gray, default is rgb24
        pixelFormat = pixelF;
    };

    int getVideoQueueSize(){ return frames.size(); }
    int getAudioQueueSize(){ return audioFrames.size(); }
    bool isInitialized(){ return bIsInitialized; }

    string getMoviePath(){ return filePath; }
    int getWidth(){return width;}
    int getHeight(){return height;}

private:
	string filePath;
    string fileName;
	string movFileExt;
	string audioFileExt;
    string videoPipePath, audioPipePath;
    string ffmpegLocation;
    string videoCodec, audioCodec, videoBitrate, audioBitrate, pixelFormat;
    int width, height, sampleRate, audioChannels;
    float frameRate;
    bool bIsInitialized;
    bool bRecordAudio;
    bool bRecordVideo;
    bool bFinishing;
    lockFreeQueue<ofPixels *> frames;
    lockFreeQueue<audioFrameShort *> audioFrames;
    unsigned long long audioSamplesRecorded;
    unsigned long long videoFramesRecorded;
    ofxVideoDataWriterThread videoThread;
    ofxAudioDataWriterThread audioThread;
    execThread ffmpegThread;
	execThread ffmpegVideoThread;
	execThread ffmpegAudioThread;
	bool vThreadRunning, aThreadRunning;
    int videoPipeFd, audioPipeFd;
    int pipeNumber;

    static set<int> openPipes;
    static int requestPipeNumber();
    static void retirePipeNumber(int num);

#ifdef TARGET_WIN32
	HANDLE hVPipe;
	HANDLE hAPipe;
	LPTSTR vPipename;
	LPTSTR aPipename;
#endif
};