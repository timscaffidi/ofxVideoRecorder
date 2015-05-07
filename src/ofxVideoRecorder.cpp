//
//  ofxVideoRecorder.cpp
//  ofxVideoRecorderExample
//
//  Created by Timothy Scaffidi on 9/23/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "ofxVideoRecorder.h"
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
#include <unistd.h>
#endif
#ifdef TARGET_WIN32
#include <io.h>
#endif
#include <fcntl.h>

int setNonblocking(int fd)
{
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
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
#endif
	return 0;
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
	if (isThreadRunning()){
		system(execCommand.c_str());
	}
}

//===============================
ofxVideoDataWriterThread::ofxVideoDataWriterThread(){};
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
void ofxVideoDataWriterThread::setup(string filePath, lockFreeQueue<ofPixels *> * q){
	this->filePath = filePath;
	fd = -1;
	queue = q;
	bIsWriting = false;
	bClose = false;
	startThread(true);
}
#endif
#ifdef TARGET_WIN32
void ofxVideoDataWriterThread::setup(HANDLE videoHandle_, lockFreeQueue<ofPixels *> * q){
	queue = q;
	bIsWriting = false;
	bClose = false;
	videoHandle = videoHandle_;
	startThread(true);
}
#endif
void ofxVideoDataWriterThread::threadedFunction(){
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
	if (fd == -1){
		fd = ::open(filePath.c_str(), O_WRONLY);
	}
#endif
	//maybe create file here? these threads act as the client and the main thread as the server?
	while (isThreadRunning())
	{
		ofPixels * frame = NULL;
		if (queue->Consume(frame) && frame){
			bIsWriting = true;
			int b_offset = 0;
			int b_remaining = frame->getWidth()*frame->getHeight()*frame->getBytesPerPixel();
			while (b_remaining > 0)
			{
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
				int b_written = ::write(fd, ((char *)frame->getPixels()) + b_offset, b_remaining);
#endif
#ifdef TARGET_WIN32
				DWORD b_written;
				if (!WriteFile(videoHandle, ((char *)frame->getPixels()) + b_offset, b_remaining, &b_written, 0)) {
					ofLogNotice("Video Thread") << "WriteFile to pipe failed. GLE " << GetLastError();
				}
#endif
				if (b_written > 0){
					b_remaining -= b_written;
					b_offset += b_written;
				}
				else {
					if (bClose){
						break; // quit writing so we can close the file
					}
				}
			}
			bIsWriting = false;
			frame->clear();
			delete frame;
		}
		else{
			condition.wait(conditionMutex, 1000);
			int test = 0;
		}
	}

#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
	::close(fd); 
#endif
#ifdef TARGET_WIN32
	FlushFileBuffers(videoHandle);
	DisconnectNamedPipe(videoHandle);
	CloseHandle(videoHandle);
#endif
}

void ofxVideoDataWriterThread::signal(){
	condition.signal();
}

//===============================
ofxAudioDataWriterThread::ofxAudioDataWriterThread(){};
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
void ofxAudioDataWriterThread::setup(string filePath, lockFreeQueue<audioFrameShort *> *q){
	this->filePath = filePath;
	fd = -1;
	queue = q;
	bIsWriting = false;
	startThread(true);
}
#endif
#ifdef TARGET_WIN32
void ofxAudioDataWriterThread::setup(HANDLE audioHandle_, lockFreeQueue<audioFrameShort *> *q){
	queue = q;
	bIsWriting = false;
	audioHandle = audioHandle_;
	startThread(true);
}
#endif
void ofxAudioDataWriterThread::threadedFunction(){
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )

	if (fd == -1){
		//write only, fd is the handle what is the windows eqivalent
		fd = ::open(filePath.c_str(), O_WRONLY);
	}
#endif
	while (isThreadRunning())
	{
		audioFrameShort * frame = NULL;
		if (queue->Consume(frame) && frame){
			bIsWriting = true;
			int b_offset = 0;
			int b_remaining = frame->size*sizeof(short);
			while (b_remaining > 0){
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
				int b_written = ::write(fd, ((char *)frame->data) + b_offset, b_remaining);
#endif
#ifdef TARGET_WIN32
				DWORD b_written;
				if (!WriteFile(audioHandle, ((char *)frame->data) + b_offset, b_remaining, &b_written, 0)){
					ofLogNotice("Audio Thread") << "WriteFile to pipe failed. GLE " << GetLastError();
				}
#endif
				if (b_written > 0){
					b_remaining -= b_written;
					b_offset += b_written;
				}
				else {
					if (bClose){
						break; // quit writing so we can close the file
					}
				}
			}
			bIsWriting = false;
			delete[] frame->data;
			delete frame;
		}
		else{
			condition.wait(conditionMutex, 1000);
		}
	}

#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
	::close(fd);
#endif
#ifdef TARGET_WIN32
	FlushFileBuffers(audioHandle);
	DisconnectNamedPipe(audioHandle);
	CloseHandle(audioHandle);
#endif
}
void ofxAudioDataWriterThread::signal(){
	condition.signal();
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
	movFileExt = ".mp4";
	audioFileExt = ".m4a";
	aThreadRunning = false;
	vThreadRunning = false;
}

bool ofxVideoRecorder::setup(string fname, int w, int h, float fps, int sampleRate, int channels)
{
	if (bIsInitialized)
	{
		close();
	}

	fileName = fname;
	filePath = ofFilePath::getAbsolutePath(fileName);
	ofStringReplace(filePath, " ", "\\ ");

	return setupCustomOutput(w, h, fps, sampleRate, channels, filePath);
}

bool ofxVideoRecorder::setupCustomOutput(int w, int h, float fps, string outputLocation){
	return setupCustomOutput(w, h, fps, 0, 0, outputLocation);
}

bool ofxVideoRecorder::setupCustomOutput(int w, int h, float fps, int sampleRate, int channels, string outputLocation)
{
	if (bIsInitialized)
	{
		close();
	}

	bRecordAudio = (sampleRate > 0 && channels > 0);
	bRecordVideo = (w > 0 && h > 0 && fps > 0);
	bFinishing = false;

	videoFramesRecorded = 0;
	audioSamplesRecorded = 0;

	if (!bRecordVideo && !bRecordAudio) {
		ofLogWarning() << "ofxVideoRecorder::setupCustomOutput(): invalid parameters, could not setup video or audio stream.\n"
			<< "video: " << w << "x" << h << "@" << fps << "fps\n"
			<< "audio: " << "channels: " << channels << " @ " << sampleRate << "Hz\n";
		return false;
	}
	videoPipePath = "";
	audioPipePath = "";
	pipeNumber = requestPipeNumber();
	if (bRecordVideo) {
		width = w;
		height = h;
		frameRate = fps;

#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
		// recording video, create a FIFO pipe
		videoPipePath = ofFilePath::getAbsolutePath("ofxvrpipe" + ofToString(pipeNumber));

		ofStringReplace(videoPipePath, " ", "\\ ");

		if (!ofFile::doesFileExist(videoPipePath)){
			string cmd = "bash --login -c 'mkfifo " + videoPipePath + "'";
			system(cmd.c_str());
		}
#endif
#ifdef TARGET_WIN32
		vPipename = TEXT("\\\\.\\pipe\\videoPipe");

		hVPipe = CreateNamedPipe(
			vPipename, // name of the pipe
			PIPE_ACCESS_OUTBOUND, // 1-way pipe -- send only
			PIPE_TYPE_BYTE, // send data as a byte stream
			1, // only allow 1 instance of this pipe
			0, // outbound buffer defaults to system default
			0, // no inbound buffer
			0, // use default wait time
			NULL // use default security attributes
			);

		if (!(hVPipe != INVALID_HANDLE_VALUE)){
			if (GetLastError() != ERROR_PIPE_BUSY)
			{
				ofLogError("Video Pipe") << "Could not open video pipe.";
			}
			// All pipe instances are busy, so wait for 5 seconds. 
			if (!WaitNamedPipe(vPipename, 5000))
			{
				ofLogError("Video Pipe") << "Could not open video pipe: 20 second wait timed out.";
			}
		}

#endif
	}

	if (bRecordAudio) {
		this->sampleRate = sampleRate;
		audioChannels = channels;

#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
		// recording video, create a FIFO pipe
		audioPipePath = ofFilePath::getAbsolutePath("ofxarpipe" + ofToString(pipeNumber));

		ofStringReplace(audioPipePath, " ", "\\ ");

		if (!ofFile::doesFileExist(audioPipePath)){

			string cmd = "bash --login -c 'mkfifo " + audioPipePath + "'";
			system(cmd.c_str());
		}
#endif
#ifdef TARGET_WIN32
		aPipename = TEXT("\\\\.\\pipe\\audioPipe");

		hAPipe = CreateNamedPipe(
			aPipename,
			PIPE_ACCESS_OUTBOUND, // 1-way pipe -- send only
			PIPE_TYPE_BYTE, // send data as a byte stream
			1, // only allow 1 instance of this pipe
			0, // outbound buffer defaults to system default
			0, // no inbound buffer
			0, // use default wait time
			NULL // use default security attributes
			);

		if (!(hAPipe != INVALID_HANDLE_VALUE)){
			if (GetLastError() != ERROR_PIPE_BUSY)
			{
				ofLogError("Audio Pipe") << "Could not open audio pipe.";
			}
			// All pipe instances are busy, so wait for 5 seconds. 
			if (!WaitNamedPipe(aPipename, 5000))
			{
				ofLogError("Audio Pipe") << "Could not open pipe: 20 second wait timed out.";
			}
		}

#endif
	}

	stringstream cmd;
	// basic ffmpeg invocation, -y option overwrites output file
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
	cmd << "bash --login -c '" << ffmpegLocation << " -y";

	if (bRecordAudio){
		cmd << " -acodec " << audioCodec << " -f s16le -ar " << sampleRate << " -ac " << audioChannels << " -i " << audioPipePath;
	}
	else { // no audio stream
		cmd << " -an";
	}
	if (bRecordVideo){ // video input options and file
		cmd << " -r " << fps << " -s " << w << "x" << h << " -f rawvideo -pix_fmt " << pixelFormat << " -i " << videoPipePath;
	}
	else { // no video stream
		cmd << " -vn";
	}

	if(bRecordAudio)
		cmd << " -b:a " << audioBitrate;
	if (bRecordVideo)
		cmd << " -vcodec " << videoCodec << " -b:v " << videoBitrate;

	cmd << " " + outputString + "' &";

	ofLogNotice("FFMpeg Command") << cmd.str() << endl;

	ffmpegThread.setup(cmd.str()); // start ffmpeg thread, will wait for input pipes to be opened

	if (bRecordAudio){
		//        audioPipeFd = ::open(audioPipePath.c_str(), O_WRONLY);
		audioThread.setup(audioPipePath, &audioFrames);
	}
	if (bRecordVideo){
		//        videoPipeFd = ::open(videoPipePath.c_str(), O_WRONLY);

		videoThread.setup(videoPipePath, &frames);
	}

#endif
#ifdef TARGET_WIN32
	//evidently there are issues with multiple named pipes http://trac.ffmpeg.org/ticket/1663

	if (bRecordAudio && bRecordVideo){
		bool fSuccess;

		// Audio Thread
		
		stringstream aCmd;
		aCmd << ffmpegLocation << " -y " << " -f s16le -acodec " << audioCodec << " -ar " << sampleRate << " -ac " << audioChannels << " -i " << "\\\\.\\pipe\\audioPipe" << " -b:a " << audioBitrate << " " << outputLocation << "_atemp" << audioFileExt;
		ffmpegAudioThread.setup(aCmd.str());
		ofLogNotice("FFMpeg Command") << aCmd.str() << endl;

		fSuccess = ConnectNamedPipe(hAPipe, NULL);
		if (!fSuccess)
		{
			ofLogError("Audio Pipe") << "SetNamedPipeHandleState failed. GLE " << GetLastError();
		}
		else {
			ofLogNotice("Audio Pipe") << "\n==========================\nAudio Pipe Connected Successfully\n==========================\n" << endl;
			audioThread.setup(hAPipe, &audioFrames);
		}

		// Video Thread

		stringstream vCmd;
		vCmd << ffmpegLocation << " -y " << " -r " << fps << " -s " << w << "x" << h << " -f rawvideo -pix_fmt " << pixelFormat << " -i " << "\\\\.\\pipe\\videoPipe" << " -vcodec " << videoCodec << " -b:v " << videoBitrate << " " << outputLocation << "_vtemp" << movFileExt;
		ffmpegVideoThread.setup(vCmd.str());
		ofLogNotice("FFMpeg Command") << vCmd.str() << endl;

		fSuccess = ConnectNamedPipe(hVPipe, NULL);
		if (!fSuccess)
		{
			ofLogError("Video Pipe") << "SetNamedPipeHandleState failed. GLE " << GetLastError();
		}
		else {
			ofLogNotice("Video Pipe") << "\n==========================\nVideo Pipe Connected Successfully\n==========================\n" << endl;
			videoThread.setup(hVPipe, &frames);
		}
	}
	else {
		cmd << ffmpegLocation << " -y ";
		if (bRecordAudio){
			cmd << " -f s16le -acodec " << audioCodec << " -ar " << sampleRate << " -ac " << audioChannels << " -i " << "\\\\.\\pipe\\audioPipe";
		}
		else { // no audio stream
			cmd << " -an";
		}
		if (bRecordVideo){ // video input options and file
			cmd << " -r " << fps << " -s " << w << "x" << h << " -f rawvideo -pix_fmt " << pixelFormat << " -i " << "\\\\.\\pipe\\videoPipe";
		}
		else { // no video stream
			cmd << " -vn";
		}
		if (bRecordAudio)
			cmd << " -b:a " << audioBitrate;
		if (bRecordVideo)
			cmd << " -vcodec " << videoCodec << " -b:v " << videoBitrate;
		cmd << " " << outputLocation << movFileExt;

		ofLogNotice("FFMpeg Command") << cmd.str() << endl;

		ffmpegThread.setup(cmd.str()); // start ffmpeg thread, will wait for input pipes to be opened

		if (bRecordAudio){
			//this blocks, so we have to call it after ffmpeg is listening for a pipe
			bool fSuccess = ConnectNamedPipe(hAPipe, NULL);
			if (!fSuccess)
			{
				ofLogError("Audio Pipe") << "SetNamedPipeHandleState failed. GLE " << GetLastError();
			}
			else {
				ofLogNotice("Audio Pipe") << "\n==========================\nAudio Pipe Connected Successfully\n==========================\n" << endl;
				audioThread.setup(hAPipe, &audioFrames);
			}
		}
		if (bRecordVideo){
			//this blocks, so we have to call it after ffmpeg is listening for a pipe
			bool fSuccess = ConnectNamedPipe(hVPipe, NULL);
			if (!fSuccess)
			{
				ofLogError("Video Pipe") << "SetNamedPipeHandleState failed. GLE " << GetLastError();
			}
			else {
				ofLogNotice("Video Pipe") << "\n==========================\nVideo Pipe Connected Successfully\n==========================\n" << endl;
				videoThread.setup(hVPipe, &frames);
			}
		}

	}
#endif

	bIsInitialized = true;

	return bIsInitialized;
}

bool ofxVideoRecorder::runCustomScript(string script)
{
	stringstream cmd;
	cmd << ffmpegLocation << " -y ";


	ofLogNotice("FFMpeg Command") << script << endl;
	ffmpegThread.setup(script);

	bIsInitialized = true;

	return bIsInitialized;
}

void ofxVideoRecorder::addFrame(const ofPixels &pixels)
{
	if (bIsInitialized && bRecordVideo)
	{
		int framesToAdd = 1; //default add one frame per request
		if (bRecordAudio && !bFinishing){
			//if also recording audio, check the overall recorded time for audio and video to make sure audio is not going out of sync
			//this also handles incoming dynamic framerate while maintaining desired outgoing framerate
			double videoRecordedTime = videoFramesRecorded / frameRate;
			double audioRecordedTime = (audioSamplesRecorded / audioChannels) / (double)sampleRate;
			double avDelta = audioRecordedTime - videoRecordedTime;

			if (avDelta > 1.0 / frameRate) {
				//more than one video frame's worth of audio data is waiting, we need to send extra video frames.
				int numFramesCopied = 0;
				while (avDelta > 1.0 / frameRate) {
					framesToAdd++;
					avDelta -= 1.0 / frameRate;
				}
				ofLogVerbose() << "ofxVideoRecorder: avDelta = " << avDelta << ". Not enough video frames for desired frame rate, copied this frame " << framesToAdd << " times.\n";
			}
			else if (avDelta < -1.0 / frameRate){
				//more than one video frame is waiting, skip this frame
				framesToAdd = 0;
				ofLogVerbose() << "ofxVideoRecorder: avDelta = " << avDelta << ". Too many video frames, skipping.\n";
			}
		}
		for (int i = 0; i < framesToAdd; i++){
			//add desired number of frames
			frames.Produce(new ofPixels(pixels));
			videoFramesRecorded++;
		}

		videoThread.signal();
	}
}

void ofxVideoRecorder::addAudioSamples(float *samples, int bufferSize, int numChannels){
	if (bIsInitialized && bRecordAudio){
		int size = bufferSize*numChannels;
		audioFrameShort * shortSamples = new audioFrameShort;
		shortSamples->data = new short[size];
		shortSamples->size = size;

		for (int i = 0; i < size; i++){
			shortSamples->data[i] = (short)(samples[i] * 32767.0f);
		}
		audioFrames.Produce(shortSamples);
		audioThread.signal();
		audioSamplesRecorded += size;
	}
}

void ofxVideoRecorder::close()
{
	if (!bIsInitialized) return;
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )

	//set pipes to non_blocking so we dont get stuck at the final writes
	setNonblocking(audioPipeFd);
	setNonblocking(videoPipeFd);

	//gettting stuck here... is it not clearing the buffer?
	while (frames.size() > 0 && audioFrames.size() > 0) {
		// if there are frames in the queue or the thread is writing, signal them until the work is done.
		videoThread.signal();
		audioThread.signal();
	}

	retirePipeNumber(pipeNumber);

	ffmpegThread.waitForThread();
#endif
#ifdef TARGET_WIN32 
	if (bRecordVideo) {
		videoThread.close();
	}
	if (bRecordAudio) {
		audioThread.close();
	}

	//at this point all data that ffmpeg wants should have been consumed
	// one of the threads may still be trying to write a frame,
	// but once close() gets called they will exit the non_blocking write loop
	// and hopefully close successfully

	if (bRecordAudio && bRecordVideo){
		ffmpegAudioThread.waitForThread(); 
		ffmpegVideoThread.waitForThread();

		//need to do one last script here to join the audio and video recordings

		stringstream finalCmd;

		/*finalCmd << ffmpegLocation << " -y " << " -i " << filePath << "_vtemp" << movFileExt << " -i " << filePath << "_atemp" << movFileExt << " \\ ";
		finalCmd << "-filter_complex \"[0:0] [1:0] concat=n=2:v=1:a=1 [v] [a]\" \\";
		finalCmd << "-map \"[v]\" -map \"[a]\" ";
		finalCmd << " -vcodec " << videoCodec << " -b:v " << videoBitrate << " -b:a " << audioBitrate << " ";
		finalCmd << filePath << movFileExt;*/

		finalCmd << ffmpegLocation << " -y " << " -i " << filePath << "_vtemp" << movFileExt << " -i " << filePath << "_atemp" << audioFileExt << " ";
		finalCmd << "-c:v copy -c:a copy -strict experimental ";
		finalCmd << filePath << movFileExt;

		ofLogNotice("FFMpeg Merge") << "\n==============================================\n Final Command \n==============================================\n";
		ofLogNotice("FFMpeg Merge") << finalCmd.str();
		ffmpegThread.setup(finalCmd.str());

	}
	else
		ffmpegThread.waitForThread();

	/*stringstream removeCmd;
	removeCmd << "DEL " << filePath << "_vtemp" << movFileExt << " " << filePath << "_atemp" << audioFileExt;
	system(removeCmd.str().c_str());*/
#endif
	// TODO: kill ffmpeg process if its taking too long to close for whatever reason.
	ofLogNotice("ofxVideoRecorder") << "\n==============================================\n Closed ffmpeg \n==============================================\n";
	bIsInitialized = false;

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
	if (!openPipes.erase(num)){
		ofLogNotice() << "ofxVideoRecorder::retirePipeNumber(): trying to retire a pipe number that is not being tracked: " << num << endl;
	}
}
