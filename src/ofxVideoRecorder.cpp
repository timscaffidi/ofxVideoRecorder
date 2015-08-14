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

int setNonBlocking(int fd)
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
    startThread(true);
}

void execThread::threadedFunction(){
    if(isThreadRunning()){
        system(execCommand.c_str());
    }
}

//===============================
ofxVideoDataWriterThread::ofxVideoDataWriterThread(){};
void ofxVideoDataWriterThread::setup(string filePath, lockFreeQueue<ofPixels *> * q){
    this->filePath = filePath;
    fd = -1;
    queue = q;
    bIsWriting = false;
    bClose = false;
    bNotifyError = false;
    startThread(true);
}

void ofxVideoDataWriterThread::threadedFunction(){
    if(fd == -1){
        fd = ::open(filePath.c_str(), O_WRONLY);
    }

    while(isThreadRunning())
    {
        ofPixels * frame = NULL;
        if(queue->Consume(frame) && frame){
            bIsWriting = true;
            int b_offset = 0;
            int b_remaining = frame->getWidth()*frame->getHeight()*frame->getBytesPerPixel();
            
            while(b_remaining > 0 && isThreadRunning())
            {
                errno = 0;
                
                int b_written = ::write(fd, ((char *)frame->getPixels())+b_offset, b_remaining);
                
                if(b_written > 0){
                    b_remaining -= b_written;
                    b_offset += b_written;
                    if (b_remaining != 0) {
                        ofLogWarning("ofxVideoDataWriterThread") << ofGetTimestampString("%H:%M:%S:%i") << " - b_remaining is not 0 -> " << b_written << " - " << b_remaining << " - " << b_offset << ".";
                        //break;
                    }
                }
                else if (b_written < 0) {
                    ofLogError("ofxVideoDataWriterThread") << ofGetTimestampString("%H:%M:%S:%i") << " - write to PIPE failed with error -> " << errno << " - " << strerror(errno) << ".";
                    bNotifyError = true;
                    break;
                }
                else {
                    if(bClose){
                        ofLogVerbose("ofxVideoDataWriterThread") << ofGetTimestampString("%H:%M:%S:%i") << " - Nothing was written and bClose is TRUE.";
                        break; // quit writing so we can close the file
                    }
                    ofLogWarning("ofxVideoDataWriterThread") << ofGetTimestampString("%H:%M:%S:%i") << " - Nothing was written. Is this normal?";
                }
                
                if (!isThreadRunning()) {
                    ofLogWarning("ofxVideoDataWriterThread") << ofGetTimestampString("%H:%M:%S:%i") << " - The thread is not running anymore let's get out of here!";
                }
            }
            bIsWriting = false;
            frame->clear();
            delete frame;
        }
        else{
            condition.wait(conditionMutex);
        }
    }

    ::close(fd);
}

void ofxVideoDataWriterThread::signal(){
    condition.signal();
}

void ofxVideoDataWriterThread::setPipeNonBlocking(){
    setNonBlocking(fd);
}

//===============================
ofxAudioDataWriterThread::ofxAudioDataWriterThread(){};
void ofxAudioDataWriterThread::setup(string filePath, lockFreeQueue<audioFrameShort *> *q){
    this->filePath = filePath;
    fd = -1;
    queue = q;
    bIsWriting = false;
    startThread(true);
}

void ofxAudioDataWriterThread::threadedFunction(){
    if(fd == -1){
        fd = ::open(filePath.c_str(), O_WRONLY);
    }

    while(isThreadRunning())
    {
        audioFrameShort * frame = NULL;
        if(queue->Consume(frame) && frame){
            bIsWriting = true;
            int b_offset = 0;
            int b_remaining = frame->size*sizeof(short);
            while(b_remaining > 0){
                int b_written = ::write(fd, ((char *)frame->data)+b_offset, b_remaining);
                if(b_written > 0){
                    b_remaining -= b_written;
                    b_offset += b_written;
                }
                else {
                    if(bClose){
                        break; // quit writing so we can close the file
                    }
                }
            }
            bIsWriting = false;
            delete [] frame->data;
            delete frame;
        }
        else{
            condition.wait(conditionMutex);
        }
    }

    ::close(fd);
}
void ofxAudioDataWriterThread::signal(){
    condition.signal();
}

void ofxAudioDataWriterThread::setPipeNonBlocking(){
    setNonBlocking(fd);
}

//===============================
ofxVideoRecorder::ofxVideoRecorder()
{
    bIsInitialized = false;
    ffmpegLocation = "ffmpeg";
    videoCodec = "mpeg4";
    audioCodec = "pcm_s16le";
    videoBitrate = "2000k";
    audioBitrate = "128k";
    pixelFormat = "rgb24";
}

bool ofxVideoRecorder::setup(string fname, int w, int h, float fps, int sampleRate, int channels, bool sysClockSync, bool silent)
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

    return setupCustomOutput(w, h, fps, sampleRate, channels, outputSettings.str(), sysClockSync, silent);
}

bool ofxVideoRecorder::setupCustomOutput(int w, int h, float fps, string outputString, bool sysClockSync, bool silent){
    return setupCustomOutput(w, h, fps, 0, 0, outputString, sysClockSync, silent);
}

bool ofxVideoRecorder::setupCustomOutput(int w, int h, float fps, int sampleRate, int channels, string outputString, bool sysClockSync, bool silent)
{
    if(bIsInitialized)
    {
        close();
    }

    bIsSilent = silent;
    bSysClockSync = sysClockSync;

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
    pipeNumber = requestPipeNumber();
    if(bRecordVideo) {
        width = w;
        height = h;
        frameRate = fps;

        // recording video, create a FIFO pipe
        videoPipePath = ofFilePath::getAbsolutePath("ofxvrpipe" + ofToString(pipeNumber));
        if(!ofFile::doesFileExist(videoPipePath)){
            string cmd = "bash --login -c 'mkfifo " + videoPipePath + "'";
            system(cmd.c_str());
            // TODO: add windows compatable pipe creation (does ffmpeg work with windows pipes?)
        }
    }

    if(bRecordAudio) {
        this->sampleRate = sampleRate;
        audioChannels = channels;

        // recording video, create a FIFO pipe
        audioPipePath = ofFilePath::getAbsolutePath("ofxarpipe" + ofToString(pipeNumber));
        if(!ofFile::doesFileExist(audioPipePath)){
            string cmd = "bash --login -c 'mkfifo " + audioPipePath + "'";
            system(cmd.c_str());

            // TODO: add windows compatable pipe creation (does ffmpeg work with windows pipes?)
        }
    }

    stringstream cmd;
    // basic ffmpeg invocation, -y option overwrites output file
    cmd << "bash --login -c '" << ffmpegLocation << (bIsSilent?" -loglevel quiet ":" ") << "-y";
    if(bRecordAudio){
        cmd << " -acodec pcm_s16le -f s16le -ar " << sampleRate << " -ac " << audioChannels << " -i " << audioPipePath;
    }
    else { // no audio stream
        cmd << " -an";
    }
    if(bRecordVideo){ // video input options and file
        cmd << " -r "<< fps << " -s " << w << "x" << h << " -f rawvideo -pix_fmt " << pixelFormat <<" -i " << videoPipePath << " -r " << fps;
    }
    else { // no video stream
        cmd << " -vn";
    }
    cmd << " "+ outputString +"' &";

    //cerr << cmd.str();

    ffmpegThread.setup(cmd.str()); // start ffmpeg thread, will wait for input pipes to be opened

    if(bRecordAudio){
        audioThread.setup(audioPipePath, &audioFrames);
    }
    if(bRecordVideo){
        videoThread.setup(videoPipePath, &frames);
    }

    bIsInitialized = true;
    bIsRecording = false;
    bIsPaused = false;

    startTime = 0;
    recordingDuration = 0;
    totalRecordingDuration = 0;

    return bIsInitialized;
}

bool ofxVideoRecorder::addFrame(const ofPixels &pixels)
{
    if (!bIsRecording || bIsPaused) return false;

    if(bIsInitialized && bRecordVideo)
    {
        int framesToAdd = 1; //default add one frame per request

        if((bRecordAudio || bSysClockSync) && !bFinishing){

            double syncDelta;
            double videoRecordedTime = videoFramesRecorded / frameRate;

            if (bRecordAudio) {
                //if also recording audio, check the overall recorded time for audio and video to make sure audio is not going out of sync
                //this also handles incoming dynamic framerate while maintaining desired outgoing framerate
                double audioRecordedTime = (audioSamplesRecorded/audioChannels)  / (double)sampleRate;
                syncDelta = audioRecordedTime - videoRecordedTime;
            }
            else {
                //if just recording video, synchronize the video against the system clock
                //this also handles incoming dynamic framerate while maintaining desired outgoing framerate
                syncDelta = systemClock() - videoRecordedTime;
            }

            if(syncDelta > 1.0/frameRate) {
                //no enought video frames, we need to send extra video frames.
                int numFramesCopied = 0;
                while(syncDelta > 1.0/frameRate) {
                    framesToAdd++;
                    syncDelta -= 1.0/frameRate;
                }
                ofLogVerbose() << "ofxVideoRecorder: recDelta = " << syncDelta << ". Not enough video frames for desired frame rate, copied this frame " << framesToAdd << " times.\n";
            }
            else if(syncDelta < -1.0/frameRate){
                //more than one video frame is waiting, skip this frame
                framesToAdd = 0;
                ofLogVerbose() << "ofxVideoRecorder: recDelta = " << syncDelta << ". Too many video frames, skipping.\n";
            }
        }

        for(int i=0;i<framesToAdd;i++){
            //add desired number of frames
            frames.Produce(new ofPixels(pixels));
            videoFramesRecorded++;
        }

        videoThread.signal();
        
        if (videoThread.bNotifyError) {
            ofLogError("ofxVideoRecorder::addFrame()") << ofGetTimestampString("%H:%M:%S:%i") << " - Notify video error!.";
            return false;
        }
        else {
            return true;
        }
    }
}

void ofxVideoRecorder::addAudioSamples(float *samples, int bufferSize, int numChannels){
    if (!bIsRecording || bIsPaused) return;

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

void ofxVideoRecorder::start()
{
    if(!bIsInitialized) return;

    if (bIsRecording) {
        //  We are already recording. No need to go further.
       return;
    }

    // Start a recording.
    bIsRecording = true;
    bIsPaused = false;
    startTime = ofGetElapsedTimef();

    ofLogVerbose() << "Recording." << endl;
}

void ofxVideoRecorder::setPaused(bool bPause)
{
    if(!bIsInitialized) return;

    if (!bIsRecording || bIsPaused == bPause) {
        //  We are not recording or we are already paused. No need to go further.
        return;
    }

    // Pause the recording
    bIsPaused = bPause;

    if (bIsPaused) {
        totalRecordingDuration += recordingDuration;

        // Log
        ofLogVerbose() << "Paused." << endl;
    } else {
        startTime = ofGetElapsedTimef();

        // Log
        ofLogVerbose() << "Recording." << endl;
    }
}

void ofxVideoRecorder::close()
{
    if(!bIsInitialized) return;

    bIsRecording = false;

    if(bRecordVideo && bRecordAudio) {
        //set pipes to non_blocking so we dont get stuck at the final writes
        audioThread.setPipeNonBlocking();
        videoThread.setPipeNonBlocking();

        while(frames.size() > 0 && audioFrames.size() > 0) {
            // if there are frames in the queue or the thread is writing, signal them until the work is done.
            videoThread.signal();
            audioThread.signal();
        }
    }
    else if(bRecordVideo) {
        //set pipes to non_blocking so we dont get stuck at the final writes
        videoThread.setPipeNonBlocking();

        while(frames.size() > 0) {
            // if there are frames in the queue or the thread is writing, signal them until the work is done.
            videoThread.signal();
        }
    }
    else if(bRecordAudio) {
        //set pipes to non_blocking so we dont get stuck at the final writes
        audioThread.setPipeNonBlocking();

        while(audioFrames.size() > 0) {
            // if there are frames in the queue or the thread is writing, signal them until the work is done.
            audioThread.signal();
        }
    }

    //at this point all data that ffmpeg wants should have been consumed
    // one of the threads may still be trying to write a frame,
    // but once close() gets called they will exit the non_blocking write loop
    // and hopefully close successfully

    bIsInitialized = false;

    if (bRecordVideo) {
        videoThread.close();
    }
    if (bRecordAudio) {
        audioThread.close();
    }

    retirePipeNumber(pipeNumber);

    ffmpegThread.waitForThread();
    // TODO: kill ffmpeg process if its taking too long to close for whatever reason.

}

float ofxVideoRecorder::systemClock()
{
    recordingDuration = ofGetElapsedTimef() - startTime;
    return totalRecordingDuration + recordingDuration;
}

set<int> ofxVideoRecorder::openPipes;

int ofxVideoRecorder::requestPipeNumber(){
    int n = 0;
    while (openPipes.find(n) != openPipes.end()) {
        n++;
    }
    openPipes.insert(n);
    return n;
}

void ofxVideoRecorder::retirePipeNumber(int num){
    if(!openPipes.erase(num)){
        ofLogNotice() << "ofxVideoRecorder::retirePipeNumber(): trying to retire a pipe number that is not being tracked: " << num << endl;
    }
}
