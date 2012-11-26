//
//  ofxVideoRecorder.cpp
//  ofxVideoRecorderExample
//
//  Created by Timothy Scaffidi on 9/23/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "ofxVideoRecorder.h"
#include <unistd.h>
#include <fcntl.h>

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

    if(encodeOnTheFly){
        string pipeFile = ofFilePath::getAbsolutePath("ofxvrpipe" + ofToString(pipeNumber));
        if(!ofFile::doesFileExist(pipeFile)){
            string cmd = "bash --login -c 'mkfifo " + pipeFile + "'";
            system(cmd.c_str());
            // TODO: add windows compatable pipe creation (does ffmpeg work with windows pipes?)
        }

        string absFilePath = ofFilePath::getAbsolutePath(fileName);
        string cmd = "bash --login -c 'ffmpeg -y -r "+ofToString(fps)+" -s "+ofToString(w)+"x"+ofToString(h)+" -f rawvideo -pix_fmt rgb24 -i "+ pipeFile +" -r "+ofToString(fps)+" -vcodec mpeg4 -sameq "+ absFilePath +"'";
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

bool ofxVideoRecorder::setupCustomOutput(int w, int h, int fps, string outputString)
{
    if(bIsInitialized)
    {
        close();
    }
    
    width = w;
    height = h;
    frameRate = fps;
    encodeOnTheFly = true;
    
    string pipeFile = ofFilePath::getAbsolutePath("ofxvrpipe" + ofToString(pipeNumber));
    if(!ofFile::doesFileExist(pipeFile)){
        string cmd = "bash --login -c 'mkfifo " + pipeFile + "'";
        system(cmd.c_str());
        // TODO: add windows compatable pipe creation (does ffmpeg work with windows pipes?)
    }
    pipeNumber++;
    
    string absFilePath = ofFilePath::getAbsolutePath(fileName);
    string cmd = "bash --login -c 'ffmpeg -y -r "+ofToString(fps)+" -s "+ofToString(w)+"x"+ofToString(h)+" -f rawvideo -pix_fmt rgb24 -i "+ pipeFile +" -r "+ofToString(fps) + " "+ outputString +"'";
    ffmpegThread.setup(cmd);
    
    fp = ::open(pipeFile.c_str(), O_WRONLY);
    bIsInitialized = true;
    
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
            if(encodeOnTheFly){
                //int br = ::write(fp, data.getBinaryBuffer(), data.size());
                int br = ::write(fp, (char *)frame->getPixels(), frame->getWidth()*frame->getHeight()*frame->getBytesPerPixel());
            }
            else{
                ofBuffer data;
                ofSaveImage(*frame,data, OF_IMAGE_FORMAT_BMP, jpeg_quality);
                videoFile.writeFromBuffer(data);
                data.clear();
            }
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

