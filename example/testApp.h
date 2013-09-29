#pragma once

#include "ofMain.h"
#include "ofxSonyRemoteCamera.h"

class testApp : public ofBaseApp{
public:
	enum APIType
	{
		TYPE_GET_AVAILABLE_API_LIST,
		TYPE_GET_METHOD_TYPES,
		TYPE_GET_APPLICATION_INFO,
		TYPE_GET_EVENT,
		TYPE_GET_VERSIONS,
		TYPE_GET_SUPPORTED_SHOOT_MODE,
		TYPE_GET_AVAILABLE_SHOOT_MODE,
		TYPE_GET_SUPPORTED_POST_VIEW_IMAGE_SIZE,
		TYPE_GET_AVAILABLE_POST_VIEW_IMAGE_SIZE,
		TYPE_GET_SUPPORTED_SELF_TIMER,
		TYPE_GET_AVAILABLE_SELF_TIMER,
#if 0
		TYPE_GET_SUPPORTED_MOVIE_QUALITY,
		TYPE_GET_AVAILABLE_MOVIE_QUALITY,
		TYPE_GET_SUPPORTED_STEADY_MODE,
		TYPE_GET_AVAILABLE_STEADY_MODE,
		TYPE_GET_SUPPORTED_VIEW_ANGLE,
		TYPE_GET_AVAILABLE_VIEW_ANGLE,
#endif
		TYPE_NUM
	};
	
	void setup();
	void exit();
	void update();
	void draw();
		
	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);

	// my callback func.
	void imageSizeUpdated(ofxSonyRemoteCamera::ImageSize& size);

	// 
	ofxSonyRemoteCamera::SRCError toggleRecording(std::string& msg);
	std::string getErrorMsg(ofxSonyRemoteCamera::SRCError err);

	void drawDebug();
	void drawAPIList(int x, int y);
	std::string getAPITypeStr(APIType type);
	ofxSonyRemoteCamera::SRCError executeCameraAPI(APIType type, std::string& msg);
	
private:
	ofxSonyRemoteCamera mRemoteCam;
	ofxSonyRemoteCamera::ShootMode mShootMode;
	ofImage mLiveViewImage;

	std::list<std::string> mMsgList;
	int mPrevTimestamp;
	APIType mAPIType;
	bool mIsRecording;
	bool mIsDebug;
};
