#pragma once

#include "ofMain.h"
#include "ofxSonyRemoteCamera.h"

class testApp : public ofBaseApp{
public:
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
	std::string getErrorMsg(ofxSonyRemoteCamera::SRCError err);

	ofxSonyRemoteCamera mRemoteCam;
	ofxSonyRemoteCamera::ShootMode mShootMode;
	ofImage mLiveViewImage;
	std::list<std::string> mMsgList;
	int mPrevTimestamp;

	
};
