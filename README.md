#ofxVideoRecorder
###a fast multi-threaded video recording extension using ffmpeg.
Multi-threaded design allows your app to run at full speed while worker threads pipe video and/or audio data to an instance of ffmpeg. If both audio and video recording is enabled, A/V synchronization is maintained by dynamically adding or skipping frames of video when needed to match the pace of the incoming audio stream; This feature compensates for inconsistant incoming frame-rates (from a camera which adjusts shutter-speed in low light for example) and produces a stable output frame-rate with no synchronization problems.

ofxVideoRecorder relies on [ffmpeg](http://ffmpeg.org) the cross-platform command line program for A/V encoding/decoding. You must have ffmpeg installed in either your system's path directories or in a custom location using the setFfmpegLocation() function (your data folder for example).

##Usage
1. Setup the video recorder. Several setup functions exist from simple to advanced. The setup function will create audio and video pipe files if necessary, launch A/V worker threads, and the main ffmpeg encoding thread.
2. Add frames and/or audio data.
	* Give the recorder ofPixels objects that match the size you set it up at.
	* Currently only supports RGB 8Bits/channel. 
	* Currently only supports ofSoundStream audio streams. Support for recording audio produced by ofSoundPlayer or ofVideoPlayer is not in openFrameworks yet. To record these sounds you will need to use a microphone or audio cable to loop back to the sound card.
3. Close the recorder. This closes the pipes and stops the worker threads, once all input pipes are closed, ffmpeg stops listening for new data and will also return.
4. Goto step 1 to start a new video recording.
	* Should also support multiple output streams using multiple recorder objects. A new pair of pipes will be created for each object's output.