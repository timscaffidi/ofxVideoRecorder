#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
    sampleRate = 48000;
    channels = 1;
    ofSetFrameRate(30);
    
    ofSetLogLevel(OF_LOG_VERBOSE);
    vidGrabber.setDesiredFrameRate(30);
    vidGrabber.initGrabber(640, 360);
//    vidRecorder.setFfmpegLocation(ofFilePath::getAbsolutePath("ffmpeg")); // use this is you have ffmpeg installed in your data folder
    
    fileName = "testMovie";
    fileExt = ".mov";
    
    vidRecorder.setVideoCodec("h264");
    vidRecorder.setVideoBitrate("800k");
    
    soundStream.listDevices();
    soundStream.setup(this, 0, 1, sampleRate, 256, 4);
    
    ofSetWindowShape(vidGrabber.getWidth(), vidGrabber.getHeight()	);
    bRecording = false;
    ofEnableAlphaBlending();
}

void testApp::exit() {
    vidRecorder.close();
}

//--------------------------------------------------------------
void testApp::update(){
    vidGrabber.update();
    if(vidGrabber.isFrameNew() && bRecording){
        vidRecorder.addFrame(vidGrabber.getPixelsRef());
    }
}

//--------------------------------------------------------------
void testApp::draw(){
    ofSetColor(255, 255, 255);
    vidGrabber.draw(0,0);    
    
    stringstream ss;
    ss << "video queue size: " << vidRecorder.getVideoQueueSize() << endl
    << "audio queue size: " << vidRecorder.getAudioQueueSize() << endl
    << "FPS: " << ofGetFrameRate() << endl
    << (bRecording?"pause":"start") << " recording: r" << endl
    << (bRecording?"close current video file: c":"") << endl;
    
    ofSetColor(0,0,0,100);
    ofRect(0, 0, 260, 75);
    ofSetColor(255, 255, 255);
    ofDrawBitmapString(ss.str(),15,15);
    
    if(bRecording){
    ofSetColor(255, 0, 0);
    ofCircle(ofGetWidth() - 20, 20, 5);
    }
}

void testApp::audioIn(float *input, int bufferSize, int nChannels){
    if(bRecording)
        vidRecorder.addAudioSamples(input, bufferSize, nChannels);
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){
    
    if(key=='r'){
        bRecording = !bRecording;
        if(bRecording && !vidRecorder.isInitialized()) {
            vidRecorder.setup(fileName+ofGetTimestampString()+fileExt, vidGrabber.getWidth(), vidGrabber.getHeight(), 30, sampleRate, channels);
//          vidRecorder.setup(fileName+ofGetTimestampString()+fileExt, vidGrabber.getWidth(), vidGrabber.getHeight(), 30); // no audio
//          vidRecorder.setupCustomOutput(vidGrabber.getWidth(), vidGrabber.getHeight(), 15, "-vcodec h264 -sameq -f mpegts udp://localhost:1234"); // for custom ffmpeg output string (streaming, etc)
        }
    }
    if(key=='c'){
        bRecording = false;
        vidRecorder.close();
    }
}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){

}
