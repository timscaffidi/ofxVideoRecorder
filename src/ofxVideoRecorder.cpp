//
//  ofxVideoRecorder.cpp
//  ofxVideoRecorderExample
//
//  Created by Timothy Scaffidi on 9/23/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "ofxVideoRecorder.h"
#include <sys/types.h>
#include <sys/stat.h>


int setNonblocking(int fd)
{
    int flags;
    
    /* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
    /* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    /* Otherwise, use the old way of doing it */
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}

//===============================
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

//===============================
ofxVideoDataWriterThread::ofxVideoDataWriterThread(){};
void ofxVideoDataWriterThread::setup(int file_desc, lockFreeQueue<ofPixels *> * q){
    fd = file_desc;
    queue = q;
    bIsWriting = false;
    startThread(true, false);
}

void ofxVideoDataWriterThread::threadedFunction(){
    while(isThreadRunning())
    {
        ofPixels * frame = NULL;
        if(queue->Consume(frame) && frame){
            bIsWriting = true;
            int b_offset = 0;
            int b_remaining = frame->getWidth()*frame->getHeight()*frame->getBytesPerPixel();
            int b_written = ::write(fd, ((char *)frame->getPixels())+b_offset, b_remaining);
            bIsWriting = false;
            frame->clear();
            delete frame;
        }
        else{
            condition.wait(conditionMutex);
        }
    }
}

void ofxVideoDataWriterThread::signal(){
    condition.signal();
}

//===============================
ofxAudioDataWriterThread::ofxAudioDataWriterThread(){};
void ofxAudioDataWriterThread::setup(int file_desc, lockFreeQueue<audioFrameShort *> *q){
    fd = file_desc;
    queue = q;
    bIsWriting = false;
    startThread(true, false);
}

void ofxAudioDataWriterThread::threadedFunction(){
    while(isThreadRunning())
    {
        audioFrameShort * frame = NULL;
        if(queue->Consume(frame) && frame){
            bIsWriting = true;
            
            int b_offset = 0;
            int b_remaining = frame->size*sizeof(short);
            int b_written = ::write(fd, ((char *)frame->data)+b_offset, b_remaining); 
            bIsWriting = false;
            delete [] frame->data;
            delete frame;
        }
        else{
            condition.wait(conditionMutex);
        }
    }
}
void ofxAudioDataWriterThread::signal(){
    condition.signal();
}

//===============================
int ofxVideoRecorder::pipeNumber = 0;

ofxVideoRecorder::ofxVideoRecorder()
{
    bIsInitialized = false;
    ffmpegLocation = "ffmpeg";
    videoCodec = "mpeg4";
    audioCodec = "aac";
    videoBitrate = "2000k";
    audioBitrate = "128k";
}

bool ofxVideoRecorder::setup(string fname, int w, int h, float fps, int sampleRate, int channels)
{
    if(bIsInitialized)
    {
        close();
    }
    
    fileName = fname;
    string absFilePath = ofFilePath::getAbsolutePath(fileName);
    
    moviePath = ofFilePath::getAbsolutePath(fileName);
    
    stringstream outputSettings;
    outputSettings
    << " -vcodec " << videoCodec
    << " -b " << videoBitrate
    << " -acodec " << audioCodec
    << " -ab " << audioBitrate
    << " " << absFilePath;
    
    return setupCustomOutput(w, h, fps, sampleRate, channels, outputSettings.str());
}

bool ofxVideoRecorder::setupCustomOutput(int w, int h, float fps, string outputString){
    return setupCustomOutput(w, h, fps, 0, 0, outputString);
}

bool ofxVideoRecorder::setupCustomOutput(int w, int h, float fps, int sampleRate, int channels, string outputString)
{
    if(bIsInitialized)
    {
        close();
    }
    
    bRecordAudio = (sampleRate > 0 && channels > 0);
    bRecordVideo = (w > 0 && h > 0 && fps > 0);
    bFinishing = false;
    
    videoFramesRecorded = 0;
    audioSamplesRecorded = 0;
    
    if(!bRecordVideo && !bRecordAudio) {
        ofLogWarning() << "ofxVideoRecorder::setupCustomOutput(): invalid parameters, could not setup video or audio stream.\n"
        << "video: " << w << "x" << h << "@" << fps << "fps\n"
        << "audio: " << "channels: " << channels << " @ " << sampleRate << "Hz\n";
        return false;
    }
    videoPipePath = "";
    audioPipePath = "";
    if(bRecordVideo) {
        width = w;
        height = h;
        frameRate = fps;
        
        // recording video, create a FIFO pipe
        videoPipePath = ofFilePath::getAbsolutePath("ofxvrpipe" + ofToString(pipeNumber));
        if(!ofFile::doesFileExist(videoPipePath)){
            mkfifo(videoPipePath.c_str(),0666);
//            string cmd = "bash --login -c 'mkfifo " + videoPipePath + "'";
//            system(cmd.c_str());
            // TODO: add windows compatable pipe creation (does ffmpeg work with windows pipes?)
        }
    }
    
    if(bRecordAudio) {
        this->sampleRate = sampleRate;
        audioChannels = channels;
        
        // recording video, create a FIFO pipe
        audioPipePath = ofFilePath::getAbsolutePath("ofxarpipe" + ofToString(pipeNumber));
        if(!ofFile::doesFileExist(audioPipePath)){
            mkfifo(audioPipePath.c_str(),0666);
//            string cmd = "bash --login -c 'mkfifo " + audioPipePath + "'";
//            system(cmd.c_str());
            
            // TODO: add windows compatable pipe creation (does ffmpeg work with windows pipes?)
        }
    }
    pipeNumber++; // increase pipe number. if there are multiple video reocrdings they must have separate pipe files
    
    stringstream cmd;
    // basic ffmpeg invocation, -y option overwrites output file
    cmd << "bash --login -c '" << ffmpegLocation << " -y";
    if(bRecordAudio){
        cmd << " -acodec pcm_s16le -f s16le -ar " << sampleRate << " -ac " << audioChannels << " -i " << audioPipePath;
    }
    else { // no audio stream
        cmd << " -an";
    }
    if(bRecordVideo){ // video input options and file
        cmd << " -r "<< fps << " -s " << w << "x" << h << " -f rawvideo -pix_fmt rgb24 -i " << videoPipePath << " -r " << fps;
    }
    else { // no video stream
        cmd << " -vn";
    }
    cmd << " "+ outputString +"'";
    
    ffmpegThread.setup(cmd.str()); // start ffmpeg thread, will wait for input pipes to be opened
    
    if(bRecordAudio){
        audioPipeFd = ::open(audioPipePath.c_str(), O_WRONLY);
        audioThread.setup(audioPipeFd, &audioFrames);
    }
    if(bRecordVideo){
        videoPipeFd = ::open(videoPipePath.c_str(), O_WRONLY);
        videoThread.setup(videoPipeFd, &frames);
    }
    bIsInitialized = true;
    
    return bIsInitialized;
}

void ofxVideoRecorder::addFrame(const ofPixels &pixels)
{
    if(bIsInitialized && bRecordVideo)
    {
        int framesToAdd = 1; //default add one frame per request
        if(bRecordAudio && !bFinishing){
            //if also recording audio, check the overall recorded time for audio and video to make sure audio is not going out of sync
            //this also handles incoming dynamic framerate while maintaining desired outgoing framerate
            double videoRecordedTime = videoFramesRecorded / frameRate;
            double audioRecordedTime = audioSamplesRecorded  / (double)sampleRate;
            double avDelta = audioRecordedTime - videoRecordedTime;
            
            if(avDelta > 1.0/frameRate) {
                //more than one video frame's worth of audio data is waiting, we need to send extra video frames.
                int numFramesCopied = 0;
                while(avDelta > 1.0/frameRate) {
                    framesToAdd++;
                    avDelta -= 1.0/frameRate;
                }
                ofLogVerbose() << "ofxVideoRecorder: avDelta = " << avDelta << ". Not enough video frames for desired frame rate, copied this frame " << framesToAdd << " times.\n";
            }
            else if(avDelta < -1.0/frameRate){
                //more than one video frame is waiting, skip this frame
                framesToAdd = 0;
                ofLogVerbose() << "ofxVideoRecorder: avDelta = " << avDelta << ". Too many video frames, skipping.\n";
            }
        }
        for(int i=0;i<framesToAdd;i++){
            //add desired number of frames
            frames.Produce(new ofPixels(pixels));
            videoFramesRecorded++;
        }
        
        videoThread.signal();
    }
}

void ofxVideoRecorder::addAudioSamples(float *samples, int bufferSize, int numChannels){
    if(bIsInitialized && bRecordAudio){
        int size = bufferSize*numChannels;
        audioFrameShort * shortSamples = new audioFrameShort;
        shortSamples->data = new short[size];
        shortSamples->size = size;
        
        for(int i=0; i < size; i++){
            shortSamples->data[i] = (short)(samples[i] * 32767.0f);
        }
        audioFrames.Produce(shortSamples);
        audioThread.signal();
        audioSamplesRecorded += size;
    }
}

void ofxVideoRecorder::close()
{
    //set pipes to non_blocking so we dont get stuck at the final writes
    bFinishing = true; // override the multi-frame adding feature in this state
    while(frames.size() > 0 || audioFrames.size() > 0 ) {
        videoThread.signal();
        audioThread.signal();
        //ofSleepMillis(5);
        if(frames.size() <=1){
            setNonblocking(audioPipeFd);
            setNonblocking(videoPipeFd);
        }
        if (audioFrames.size() < sampleRate/frameRate) {
            setNonblocking(audioPipeFd);
            setNonblocking(videoPipeFd);
        }
                
        if ((frames.size() > 0 || videoThread.isWriting()) && audioFrames.size() == 0) {
            // video frame in queue, or video thread stuck in a write. try to add enough audio data for ffmpeg to consume it
            ofLogVerbose() <<"ofxVideoRecorder::close(): stuck writing video, adding blank audio samples" <<endl;
            int numSamples = audioChannels*sampleRate/frameRate;
            float * samples = new float[numSamples];
            memset(samples, 0, sizeof(float)*numSamples);
            addAudioSamples(samples, numSamples, audioChannels);
            delete [] samples;
        }
        else if(frames.size() == 0 && (audioFrames.size() > 0 || audioThread.isWriting())) {
            // audio samples in queue, or audioThread stuck in a write. add blank video frame
            ofLogVerbose() <<"ofxVideoRecorder::close(): stuck writing audio, adding blank video frames" <<endl;
            ofPixels pixels;
            pixels.allocate(width,height,3);
            addFrame(pixels);
        }
    }
    
    
    videoThread.stopThread();
    videoThread.signal();
    audioThread.stopThread();
    audioThread.signal();
    bIsInitialized = false;
    
    if (bRecordVideo) {
        ::close(videoPipeFd);
    }
    if (bRecordAudio) {
        ::close(audioPipeFd);
    }
    ffmpegThread.waitForThread();
    
}
