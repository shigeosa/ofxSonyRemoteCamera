/*!
 * This source code is for HDR-AS15, but some apis can not be used.
 * Please check error code and following documents will help you.
 * https://camera.developer.sony.com/pages/documents/view/?id=camera_api 
 */
#include "testApp.h"

static const int MSG_LIST_SIZE(7);
static const unsigned long long WAIT_TIME_OUT(5000);
static const unsigned long long WAIT_MSG_INTERVAL(500);


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
		} else if (timestamp > WAIT_TIME_OUT) {
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

	mAPIType = TYPE_GET_AVAILABLE_API_LIST;
	mIsRecording = false;
	mIsDebug = true;
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

	if (mIsDebug) {
		drawDebug();
	}
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
		err = mRemoteCam.getShootMode(mShootMode);
		if (err == ofxSonyRemoteCamera::SRC_OK) msg += "Shoot Mode: " + mRemoteCam.getShootModeString(mShootMode);
		break;
	case 'd':
		mIsDebug = !mIsDebug;
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
	case 's':
		err = mRemoteCam.setShootMode(ofxSonyRemoteCamera::SHOOT_MODE_STILL);
		if (err == ofxSonyRemoteCamera::SRC_OK) {
			msg += "Set Shoot Mode: Still";
			mShootMode = ofxSonyRemoteCamera::SHOOT_MODE_STILL;
		}
		break;
	case 'q':
		err = mRemoteCam.actZoom("in", "1shot");
		break;
	case 'w':
		err = mRemoteCam.actZoom("out", "1shot");
		break;
	case ' ':
		toggleRecording(msg);
		break;
	case OF_KEY_UP:
		{
			int type(mAPIType - 1);
			if (type < 0) type = TYPE_NUM - 1;
			mAPIType = static_cast<APIType>(type);
		}
		break;
	case OF_KEY_DOWN:
		mAPIType = static_cast<APIType>((mAPIType+1)%TYPE_NUM);
		break;
	case OF_KEY_RETURN:
		err = executeCameraAPI(mAPIType, msg);
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
ofxSonyRemoteCamera::SRCError testApp::toggleRecording(std::string& msg)
{
	ofxSonyRemoteCamera::SRCError err(ofxSonyRemoteCamera::SRC_OK);
	
	if (!mIsRecording) {
		switch (mShootMode) {
		case ofxSonyRemoteCamera::SHOOT_MODE_MOVIE:
			err = mRemoteCam.startMovieRec();
			mIsRecording = true;
			if (err == ofxSonyRemoteCamera::SRC_OK) {
				msg += "Start Recording";
			}
			break;
		case ofxSonyRemoteCamera::SHOOT_MODE_STILL:
			err = mRemoteCam.actTakePicture();
			if (err == ofxSonyRemoteCamera::SRC_OK) {
				msg += "Take a picture";
			}
			break;
		case ofxSonyRemoteCamera::SHOOT_MODE_INTERVAL_STILL:
			err = mRemoteCam.startIntervalStillRec();
			mIsRecording = true;
			if (err == ofxSonyRemoteCamera::SRC_OK) {
				msg += "Start Recording";
			}
			break;
		}
		if (err == ofxSonyRemoteCamera::SRC_ERROR_ILLEGAL_REQUEST) {
			msg += "Please Check Shoot Mode or Recording Status";
		}
	} else {
		switch (mShootMode) {
		case ofxSonyRemoteCamera::SHOOT_MODE_MOVIE:
			err = mRemoteCam.stopMovieRec();
			mIsRecording = false;
			//if (err == ofxSonyRemoteCamera::SRC_OK) mIsRecording = false;
			break;
		case ofxSonyRemoteCamera::SHOOT_MODE_STILL:
			break;
		case ofxSonyRemoteCamera::SHOOT_MODE_INTERVAL_STILL:
			err = mRemoteCam.stopIntervalStillRec();
			mIsRecording = false;
			//if (err == ofxSonyRemoteCamera::SRC_OK) mIsRecording = false;
			break;
		}
		if (err == ofxSonyRemoteCamera::SRC_OK) {
			msg += "Stop Recording";
		} else if (err == ofxSonyRemoteCamera::SRC_ERROR_ILLEGAL_REQUEST) {
			msg += "Please Check Shoot Mode or Recording Status";
		}
	}
	return err;
}

//--------------------------------------------------------------
std::string testApp::getErrorMsg(ofxSonyRemoteCamera::SRCError err)
{
	if (err != ofxSonyRemoteCamera::SRC_OK) {
		return "Err code: " + ofToString(static_cast<int>(err)) + ", " + mRemoteCam.getErrorString(err);
	}
	return "";
}

//--------------------------------------------------------------
std::string testApp::getAPITypeStr(APIType type)
{
	switch (type) {
	case TYPE_GET_AVAILABLE_API_LIST:
		return "GET_AVAILABLE_API_LIST";
	case TYPE_GET_METHOD_TYPES:
		return "GET_METHOD_TYPES";
	case TYPE_GET_APPLICATION_INFO:
		return "GET_APPLICATION_INFO";
	case TYPE_GET_EVENT:
		return "GET_EVENT";
	case TYPE_GET_VERSIONS:
		return "GET_VERSIONS";
	case TYPE_GET_SUPPORTED_SHOOT_MODE:
		return "GET_SUPPORTED_SHOOT_MODE";
	case TYPE_GET_AVAILABLE_SHOOT_MODE:
		return "GET_AVAILABLE_SHOOT_MODE";
	case TYPE_GET_SUPPORTED_POST_VIEW_IMAGE_SIZE:
		return "GET_SUPPORTED_POST_VIEW_IMAGE_SIZE";
	case TYPE_GET_AVAILABLE_POST_VIEW_IMAGE_SIZE:
		return "GET_AVAILABLE_POST_VIEW_IMAGE_SIZE";
	case TYPE_GET_SUPPORTED_SELF_TIMER:
		return "GET_SUPPORTED_SELF_TIMER";
	case TYPE_GET_AVAILABLE_SELF_TIMER:
		return "GET_AVAILABLE_SELF_TIMER";
#if 0
	case TYPE_GET_SUPPORTED_MOVIE_QUALITY:
		return "GET_SUPPORTED_MOVIE_QUALITY";
	case TYPE_GET_AVAILABLE_MOVIE_QUALITY:
		return "GET_AVAILABLE_MOVIE_QUALITY";
	case TYPE_GET_SUPPORTED_STEADY_MODE:
		return "GET_SUPPORTED_STEADY_MODE";
	case TYPE_GET_AVAILABLE_STEADY_MODE:
		return "GET_AVAILABLE_STEADY_MODE";
	case TYPE_GET_SUPPORTED_VIEW_ANGLE:
		return "GET_SUPPORTED_VIEW_ANGLE";
	case TYPE_GET_AVAILABLE_VIEW_ANGLE:
		return "GET_AVAILABLE_VIEW_ANGLE";
#endif
	}
	return "";
}

//--------------------------------------------------------------
void testApp::drawDebug()
{
	const int width(ofGetWidth());
	const int height(ofGetHeight());
	ofPushStyle();
	{
		ofEnableAlphaBlending();	
		ofSetColor(0,0,0,150);
		ofRect(0,height-70,width,70);
		ofSetColor(255,255,0);
		// draw status
		ofDrawBitmapString(
			"Timestamp: " + ofToString(mPrevTimestamp/1000.f, 2) + " [sec.], " "ShootMode: " + mRemoteCam.getShootModeString(mShootMode)
			, 0, height - 55);
		// draw key commands
		ofSetColor(255);
		ofDrawBitmapString(
			"ESC: exit" ", 1: startLiveView" ", 2: stopLiveView, 3: getShootMode , \n"
			"d: show/hide debug info, f: toggleFullScreen" ", i: setShootMode IntervalStill" ", m: setShootMode Movie" ", s: setShootMode Still\n"
			"space: toggle recording, up/down: select, q: zoom in, w: zoom out \n"
			, 0, height - 40);
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

		drawAPIList(width-300,120);
	}
	ofPopStyle();
}

//--------------------------------------------------------------
void testApp::drawAPIList(int x, int y)
{
	ofPushStyle();
	{
		for (int i(0); i<TYPE_NUM; ++i) {
			const APIType type(static_cast<APIType>(i));
			if (type == mAPIType) {
				ofSetColor(255,255,0);
			} else {
				ofSetColor(200,200,200);
			}
			ofDrawBitmapString(getAPITypeStr(type), x, y+10*i);
		}
	}
	ofPopStyle();
}

//--------------------------------------------------------------
ofxSonyRemoteCamera::SRCError testApp::executeCameraAPI(APIType type, std::string& msg)
{
	ofxSonyRemoteCamera::SRCError err(ofxSonyRemoteCamera::SRC_OK);

	switch (type) {
	case TYPE_GET_AVAILABLE_API_LIST:
		err = mRemoteCam.getAvailableApiList(msg);
		break;
	case TYPE_GET_METHOD_TYPES:
		err = mRemoteCam.getMethodTypes(msg);
		break;
	case TYPE_GET_APPLICATION_INFO:
		err = mRemoteCam.getApplicationInfo(msg);
		break;
	case TYPE_GET_EVENT:
		err = mRemoteCam.getEvent(msg, true);		
		break;
	case TYPE_GET_VERSIONS:
		err = mRemoteCam.getVersions(msg);
		break;
	case TYPE_GET_SUPPORTED_SHOOT_MODE:
		err = mRemoteCam.getSupportedShootMode(msg);
		break;
	case TYPE_GET_AVAILABLE_SHOOT_MODE:
		err = mRemoteCam.getAvailableShootMode(msg);
		break;
	case TYPE_GET_SUPPORTED_POST_VIEW_IMAGE_SIZE:
		err = mRemoteCam.getSupportedPostViewImageSize(msg);
		break;
	case TYPE_GET_AVAILABLE_POST_VIEW_IMAGE_SIZE:
		err = mRemoteCam.getAvailablePostViewImageSize(msg);
		break;
	case TYPE_GET_SUPPORTED_SELF_TIMER:
		err = mRemoteCam.getSupportedSelfTimer(msg);
		break;
	case TYPE_GET_AVAILABLE_SELF_TIMER:
		err = mRemoteCam.getAvailableSelfTimer(msg);
		break;
#if 0
	case TYPE_GET_SUPPORTED_MOVIE_QUALITY:
		err = mRemoteCam.getSupportedMovieQuality(msg);
		break;
	case TYPE_GET_AVAILABLE_MOVIE_QUALITY:
		err = mRemoteCam.getAvailableMovieQuality(msg);
		break;
	case TYPE_GET_SUPPORTED_STEADY_MODE:
		err = mRemoteCam.getSupportedSteadyMode(msg);
		break;
	case TYPE_GET_AVAILABLE_STEADY_MODE:
		err = mRemoteCam.getAvailableSteadyMode(msg);
		break;
	case TYPE_GET_SUPPORTED_VIEW_ANGLE:
		err = mRemoteCam.getSupportedViewAngle(msg);
		break;
	case TYPE_GET_AVAILABLE_VIEW_ANGLE:
		err = mRemoteCam.getAvailableViewAngle(msg);
		break;
#endif
	}
	std::cout << msg << std::endl;
	return err;
}
