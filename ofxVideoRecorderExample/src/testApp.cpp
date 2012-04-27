#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
    ofSetLogLevel(OF_LOG_NOTICE);
     vidGrabber.setDesiredFrameRate(60);
    vidGrabber.initGrabber(640, 480);
    vidRecorder.setup("testMovie.mov", vidGrabber.width, vidGrabber.height, 60, JPEG_QUALITYSUPERB);
}

void testApp::exit() {
    vidRecorder.encodeVideo();
}

//--------------------------------------------------------------
void testApp::update(){
    vidGrabber.update();
    if(vidGrabber.isFrameNew()) {
        vidRecorder.addFrame(vidGrabber.getPixelsRef());
    }
    char title[256];
    sprintf(title,"%d/%d=%.2f, %.2f", vidRecorder.getNumFramesInQueue(), ofGetElapsedTimeMillis()/1000, vidRecorder.getNumFramesInQueue()/ofGetElapsedTimef(), ofGetFrameRate());
    ofSetWindowTitle(ofToString(title));
}

//--------------------------------------------------------------
void testApp::draw(){
    vidGrabber.draw(0,0);
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
    if(key=='c')
        vidRecorder.close();
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

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
