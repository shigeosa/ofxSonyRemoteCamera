/*!
 * This source code is for HDR-AS15, but some apis can not be used.
 * Please check error code and following documents will help you.
 * https://camera.developer.sony.com/pages/documents/view/?id=camera_api 
 */
#include "testApp.h"

static const int MSG_LIST_SIZE(7);
static const unsigned long long WAIT_MSG_INTERVAL(500);
static const unsigned long long WAIT_TIMEOUT(5000);

//--------------------------------------------------------------
void testApp::setup(){

	mMsgList.push_back("Call getAvailableApiList and check your available apis. Available apis depend on your device.");
	mMsgList.push_back("Some apis cannot be used even if apis are listed on available api lists.");

	mPrevTimestamp = 0;

	mRemoteCam.setup();
	ofAddListener(mRemoteCam.imageSizeUpdated, this, &testApp::imageSizeUpdated);
	
	ofxSonyRemoteCamera::SRCError err(ofxSonyRemoteCamera::SRC_OK);
	err = mRemoteCam.startLiveView();
	if (err != ofxSonyRemoteCamera::SRC_OK) mMsgList.push_back(getErrorMsg(err));
	
	// wait untill LiveViewImage packets are received
	bool isSuccessed(true);
	unsigned long long lastMsgTimestamp(0);
	while (!mRemoteCam.isLiveViewFrameNew()) {
		mRemoteCam.update();
		const unsigned long long timestamp(ofGetElapsedTimeMillis());
		if (timestamp - lastMsgTimestamp > WAIT_MSG_INTERVAL) {
			std::cout << "wait untill liveview images are updated" << std::endl;
			lastMsgTimestamp = timestamp;
		} else if (timestamp > WAIT_TIMEOUT) {
			std::cout << "connect server error. pleae check your Wi-Fi connection" << std::endl;
			isSuccessed = false;
			break;
		}
		ofSleepMillis(1);
	}

	if (isSuccessed) {
		err = mRemoteCam.getShootMode(mShootMode);
		if (err != ofxSonyRemoteCamera::SRC_OK) mMsgList.push_back(getErrorMsg(err));
	}
}

//--------------------------------------------------------------
void testApp::exit(){
	mRemoteCam.exit();
}

//--------------------------------------------------------------
void testApp::update(){
	mRemoteCam.update();
	if (mRemoteCam.isLiveViewFrameNew()) {
		int timestamp(0);
		mRemoteCam.getLiveViewImage(mLiveViewImage.getPixels(), timestamp);
		mLiveViewImage.update();

		if (timestamp != mPrevTimestamp) {
			ofSetWindowTitle("Cam FPS: " + ofToString(1000.f/(timestamp-mPrevTimestamp), 1));
			mPrevTimestamp = timestamp;
		}
	}
	while (mMsgList.size() > MSG_LIST_SIZE) {
		mMsgList.pop_front();
	}
}

//--------------------------------------------------------------
void testApp::draw(){
	
	const int width(ofGetWidth());
	const int height(ofGetHeight());
	
	ofSetColor(255,255,255);
	mLiveViewImage.draw(0,0,width, width * mLiveViewImage.getHeight() / mLiveViewImage.getWidth());

	ofEnableAlphaBlending();
	
	ofSetColor(0,0,0,150);
	ofRect(0,height-100,width,100);
	ofSetColor(255,255,0);
	// draw status
	ofDrawBitmapString(
		"Timestamp: " + ofToString(mPrevTimestamp/1000.f, 2) + " [sec.], " "ShootMode: " + mRemoteCam.getShootModeString(mShootMode)
		, 0, height - 85);
	// draw key commands
	ofSetColor(255);
	ofDrawBitmapString(
		"ESC: exit" ", 1: startLiveView" ", 2: stopLiveView" ", 3: getAvailableApiList" ", 4: getMethodTypes" ", 5: getSupportedShootMode\n"
		"6: getAvailableShootmode" ", 7: getShootMode , 8: getAvailableSteadyMode, 9: getApplicationInfo, 0: \n"
		"e: getEvent" ", f: toggleFullScreen" ", i: setShootMode IntervalStill" ", m: setShootMode Movie" ", n: setShootMode Still(HDR-AS15 not supported)\n"
		"r: startMovieRec/startIntervalStillRec" ", s: stopMovieRec/stopIntervalStillRec\n"
		"v: getVersions"
		, 0, height - 70);
	// draw msg
	ofSetColor(0,0,0,150);
	ofRect(0,0,width,100);
	ofSetColor(255);
	std::string msg("");
	for (std::list<std::string>::const_iterator it=mMsgList.begin(); it!=mMsgList.end(); ++it) {
		msg += (*it) + "\n";
	}
	ofDrawBitmapString(msg, 0, 10);

	ofDisableAlphaBlending();
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){

	ofxSonyRemoteCamera::SRCError err(ofxSonyRemoteCamera::SRC_OK);
	std::string msg("");

	switch(key) {
	case '1':
		err = mRemoteCam.startLiveView();
		if (err == ofxSonyRemoteCamera::SRC_OK) msg += "Start Live View";
		break;
	case '2':
		err =mRemoteCam.stopLiveView();
		if (err == ofxSonyRemoteCamera::SRC_OK) msg += "Stop Live View";
		break;
	case '3':
		err = mRemoteCam.getAvailableApiList(msg);
		std::cout << msg << std::endl;
		break;
	case '4':
		err = mRemoteCam.getMethodTypes(msg);
		std::cout << msg << std::endl;
		break;
	case '5':
		err = mRemoteCam.getSupportedShootMode(msg);
		break;
	case '6':
		err = mRemoteCam.getAvailableShootMode(msg);
		break;
	case '7':
		err = mRemoteCam.getShootMode(mShootMode);
		if (err == ofxSonyRemoteCamera::SRC_OK) msg += "Shoot Mode: " + mRemoteCam.getShootModeString(mShootMode);
		break;
	case '8':
		err = mRemoteCam.getAvailableSteadyMode(msg);
		break;
	case '9':
		err = mRemoteCam.getApplicationInfo(msg);
		break;
	case '0':
		break;
	case 'e':
		err = mRemoteCam.getEvent(msg, false);
		std::cout << msg << std::endl;
		break;
	case 'f':
		ofToggleFullscreen();
		break;
	case 'i':
		err = mRemoteCam.setShootMode(ofxSonyRemoteCamera::SHOOT_MODE_INTERVAL_STILL);
		if (err == ofxSonyRemoteCamera::SRC_OK) {
			msg += "Set Shoot Mode: Interval Still";
			mShootMode = ofxSonyRemoteCamera::SHOOT_MODE_INTERVAL_STILL;
		}
		break;
	case 'm':
		err = mRemoteCam.setShootMode(ofxSonyRemoteCamera::SHOOT_MODE_MOVIE);
		if (err == ofxSonyRemoteCamera::SRC_OK) {
			msg += "Set Shoot Mode: Movie";
			mShootMode = ofxSonyRemoteCamera::SHOOT_MODE_MOVIE;
		}
		break;
	case 'n':
		err = mRemoteCam.setShootMode(ofxSonyRemoteCamera::SHOOT_MODE_STILL);
		if (err == ofxSonyRemoteCamera::SRC_OK) {
			msg += "Set Shoot Mode: Still";
			mShootMode = ofxSonyRemoteCamera::SHOOT_MODE_STILL;
		}
		break;
	case 'r':
		switch (mShootMode) {
		case ofxSonyRemoteCamera::SHOOT_MODE_MOVIE:
			err = mRemoteCam.startMovieRec();
			break;
		case ofxSonyRemoteCamera::SHOOT_MODE_STILL:
			err = mRemoteCam.actTakePicture();
			break;
		case ofxSonyRemoteCamera::SHOOT_MODE_INTERVAL_STILL:
			err = mRemoteCam.startIntervalStillRec();
			break;
		}
		if (err == ofxSonyRemoteCamera::SRC_OK) {
			msg += "Start Recording";
		} else if (err == ofxSonyRemoteCamera::SRC_ERROR_ILLEGAL_REQUEST) {
			msg += "Please Check Shoot Mode or Recording Status";
		}
		break;
	case 's':
		switch (mShootMode) {
		case ofxSonyRemoteCamera::SHOOT_MODE_MOVIE:
			err = mRemoteCam.stopMovieRec();
			break;
		case ofxSonyRemoteCamera::SHOOT_MODE_STILL:
			// TODO
			break;
		case ofxSonyRemoteCamera::SHOOT_MODE_INTERVAL_STILL:
			err = mRemoteCam.stopIntervalStillRec();
			break;
		}
		if (err == ofxSonyRemoteCamera::SRC_OK) {
			msg += "Stop Recording";
		} else if (err == ofxSonyRemoteCamera::SRC_ERROR_ILLEGAL_REQUEST) {
			msg += "Please Check Shoot Mode or Recording Status";
		}
		break;
	case 'v':
		err = mRemoteCam.getVersions(msg);
		break;
	}

	std::string outputStr("");
	if (err != ofxSonyRemoteCamera::SRC_OK) {
		outputStr += getErrorMsg(err);
	}
	if (msg.size()) {
		if (outputStr.size() ) outputStr += ", ";
		outputStr += "msg: " + msg;
	}
	if (outputStr.size()) {
		mMsgList.push_back(outputStr);
	}
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y){

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

//--------------------------------------------------------------
void testApp::imageSizeUpdated(ofxSonyRemoteCamera::ImageSize& size) {
	std::string msg("ImageSizeUpdated: " + ofToString(size.width) + ", " + ofToString(size.height));
	mMsgList.push_back(msg);
	mLiveViewImage.allocate(size.width, size.height, OF_IMAGE_COLOR);
}

//--------------------------------------------------------------
std::string testApp::getErrorMsg(ofxSonyRemoteCamera::SRCError err)
{
	if (err != ofxSonyRemoteCamera::SRC_OK) {
		return "Err code: " + ofToString(static_cast<int>(err)) + ", " + mRemoteCam.getErrorString(err);
	}
	return "";
}
