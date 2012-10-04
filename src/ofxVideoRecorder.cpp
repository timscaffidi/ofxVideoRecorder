//
//  ofxVideoRecorder.cpp
//  ofxVideoRecorderExample
//
//  Created by Timothy Scaffidi on 9/23/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "ofxVideoRecorder.h"
#include <unistd.h>

execThread::execThread(){
    execCommand = "";
}

void execThread::setup(string command){
    execCommand = command;
    startThread(true, false);
}

void execThread::threadedFunction(){
    if(isThreadRunning()){
        system(execCommand.c_str());
    }
}
int ofxVideoRecorder::pipeNumber = 0;

ofxVideoRecorder::ofxVideoRecorder()
{
    bIsInitialized = false;
}

bool ofxVideoRecorder::setup(string fname, int w, int h, int fps, ofImageQualityType quality, bool onTheFly)
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
    encodeOnTheFly = onTheFly;
    
    if(onTheFly){        
        string pipeFile = ofFilePath::getAbsolutePath("ofxvrpipe" + ofToString(pipeNumber));
        if(!ofFile::doesFileExist(pipeFile)){
            string cmd = "bash --login -c 'mkfifo " + pipeFile + "'";
            system(cmd.c_str());
        }
        
        string absFilePath = ofFilePath::getAbsolutePath(fileName);
        string cmd = "bash --login -c 'ffmpeg -y -r 15 -f mjpeg -vcodec mjpeg -i "+ pipeFile +" -r 15 -vcodec mpeg4 -sameq "+ absFilePath +"'";
        ffmpegThread.setup(cmd);
        
        fp = ::open(pipeFile.c_str(), O_WRONLY);
        bIsInitialized = true;
    }
    else{
        bIsInitialized = videoFile.open(fileName+".mjpg",ofFile::WriteOnly,true);
    }
    bufferPath = videoFile.getAbsolutePath();
    moviePath = ofFilePath::getAbsolutePath(fileName);
    
    if(!isThreadRunning())
        startThread(true, false);
    return bIsInitialized;
}

void ofxVideoRecorder::setQuality(ofImageQualityType q){
    jpeg_quality = q;
}

void ofxVideoRecorder::addFrame(const ofPixels &pixels)
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

void ofxVideoRecorder::close()
{
    condition.signal();
    while(frames.size() > 0) ofSleepMillis(100);
    
    bIsInitialized = false;
    condition.signal();
    
    stopThread();
    ofSleepMillis(100);
    
    if(videoFile.is_open())
    {
        videoFile.close();
    }
    if(encodeOnTheFly){
//        ::close(videoPipeFd[1]);
        //        ::close(videoPipeFd[0]);
        ::close(fp);
        ffmpegThread.waitForThread();
    }
}

int ofxVideoRecorder::encodeVideo()
{
    if(bIsInitialized) close();
    if(encodeOnTheFly) return 0;
    string fr = ofToString(frameRate);
    string cmd = "bash --login -c 'ffmpeg -y -r " + fr + " -i " + bufferPath + " -r " + fr + " -vcodec copy " + moviePath + "; rm " + bufferPath + "'";
    
    return system(cmd.c_str());
}

void ofxVideoRecorder::threadedFunction()
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
            if(encodeOnTheFly){
                int br = ::write(fp, data.getBinaryBuffer(), data.size());
            }
            else{
                videoFile.writeFromBuffer(data);
            }
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

