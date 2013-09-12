//
//  ofxSonyRemoteCamera.h
//  Created by Osamu Shigeta on 9/12/2013.
//
#pragma once

#include "ofMain.h"
#include "picojson.h"

#include "Poco/URI.h" 
#include "Poco/File.h"
#include "Poco/StreamCopier.h" 
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"

class ofxSonyRemoteCamera : public ofThread
{
public:
	/*!
	 *	Error codes. Please refer following documents.
	 *	https://camera.developer.sony.com/pages/documents/view/?id=camera_api
	*/
	enum SRCError
	{
		SRC_OK							= 0,
		SRC_ERROR_ANY					= 1,
		SRC_ERROR_TIMEOUT				= 2,
		SRC_ERROR_ILLEGAL_ARGUMENT		= 3,
		SRC_ERROR_ILLEGAL_DATA_FORMAT	= 4,
		SRC_ERROR_ILLEGAL_REQUEST		= 5,
		SRC_ERROR_ILLEGAL_RESPONSE		= 6,
		SRC_ERROR_ILLEGAL_STATE			= 7,
		SRC_ERROR_ILLEGAL_TYPE			= 8,
		SRC_ERROR_INDEX_OUT_OF_BOUNDS	= 9,
		SRC_ERROR_NO_SUCH_ELEMENT		= 10,
		SRC_ERROR_NO_SUCH_FIELD			= 11,
		SRC_ERROR_NO_SUCH_METHOD		= 12,
		SRC_ERROR_NULL_POINTER			= 13,
		SRC_ERROR_UNSUPPORTED_VERSION	= 14,
		SRC_ERROR_UNSUPPORTED_OPERATION	= 15,

		SRC_ERROR_UNKNOWN				= 16,	//!< my error code

		SRC_ERROR_SHOOTING_FAIL					= 40400,
		SRC_ERROR_CAMERA_NOT_READY				= 40401,
		SRC_ERROR_ALREADY_RUNNING_POLLING_API	= 40402,
		SRC_ERROR_STILL_CAPTURING_NOT_FINISHED	= 40403,
	};
	enum ShootMode
	{
		SHOOT_MODE_STILL,
		SHOOT_MODE_INTERVAL_STILL,
		SHOOT_MODE_MOVIE,
	};

	enum MovieQuality
	{
		MOVIE_QUALITY_PS,
		MOVIE_QUALITY_HQ,
		MOVIE_QUALITY_STD,
		MOVIE_QUALITY_SLOW,
		MOVIE_QUALITY_SSLOW,
		MOVIE_QUALITY_VGA
	};
	struct CommonHeader
	{
		CommonHeader(): payLoadType(0), frameId(0), timestamp(0) {}
		int payLoadType;
		int frameId;
		int timestamp;
	};
	struct PayloadHeader
	{
		PayloadHeader(): jpegSize(0), paddingSize(0) {}
		int jpegSize;
		int paddingSize;
	};
	struct ImageSize
	{
		ImageSize(): width(0), height(0) {} 
		int width;
		int height;
	};
public:
	ofxSonyRemoteCamera();
	~ofxSonyRemoteCamera();
	bool setup(const std::string& host="10.0.0.1", int port=10000);
	void exit();
	void update();

	ofEvent<ImageSize> imageSizeUpdated;

	//-----------------------------------------------------------------
	// Liveview (Available)
	//-----------------------------------------------------------------
	SRCError startLiveView();
	SRCError stopLiveView();
	bool isLiveViewFrameNew();
	bool isLiveViewSessionConnected();
	/*!
		memorySize = getLiveViewImageWidth() * getLiveViewImageHeight() * 3
	*/
	void getLiveViewImage(unsigned char* pImg, int& timestamp);
	void getLiveViewImage(ofPixels& pixels, int& timestamp);
	/*!
		this api cannot be used untill live view is updated. 
	*/
	int getLiveViewImageWidth() const;
	/*!
		this api cannot be used untill live view is updated.
	*/
	int getLiveViewImageHeight() const;

	void getCommonHeader(CommonHeader& header);
	void getPayloadHeader(PayloadHeader& header);

	//-----------------------------------------------------------------
	// Still capture
	//-----------------------------------------------------------------
	SRCError actTakePicture();
	SRCError awaitTakePicture();

	//-----------------------------------------------------------------
	// Movie recording (Available)
	//-----------------------------------------------------------------
	SRCError startMovieRec();
	SRCError stopMovieRec();

	//-----------------------------------------------------------------
	// Zoom
	//-----------------------------------------------------------------
	/*!
		@params direction "in" or "out"
		@params movement "start", "stop" or "1shot"
		@return SRCError SRC_OK is ok, others are error
	*/
	SRCError actZoom(const std::string& direction, const std::string& movement);

	//-----------------------------------------------------------------
	// Self-timer
	//-----------------------------------------------------------------
	SRCError getSupportedSelfTimer(std::string& json);
	SRCError getAvailableSelfTimer(std::string& json);
	SRCError getSelfTimer(int& second);
	SRCError setSelfTimer(int second);

	//-----------------------------------------------------------------
	// Shoot mode (Available)
	//-----------------------------------------------------------------
	SRCError getSupportedShootMode(std::string& json);
	SRCError getAvailableShootMode(std::string& json);
	SRCError getShootMode(ShootMode& mode);
	SRCError setShootMode(ShootMode mode);

	//-----------------------------------------------------------------
	// Event notification (Available)
	//-----------------------------------------------------------------
	/*!
		@params pollingFlag true: Callback when timeout or change point detection, false: Callback immediately
	*/
	SRCError getEvent(std::string& json, bool pollingFlag);

	//-----------------------------------------------------------------
	// Camera setup
	//-----------------------------------------------------------------
	SRCError startRecMode();
	SRCError stopRecMode();

	//-----------------------------------------------------------------
	// Server information
	//-----------------------------------------------------------------
	SRCError getAvailableApiList(std::string& json);
	SRCError getMethodTypes(std::string& json);
	SRCError getVersions(std::string& json);
	SRCError getApplicationInfo(std::string& json);

	//-----------------------------------------------------------------
	// othrer
	//-----------------------------------------------------------------
	SRCError startIntervalStillRec();
	SRCError stopIntervalStillRec();

	SRCError getSupportedViewAngle(std::string& json);
	SRCError getAvailableViewAngle(std::string& json);
	SRCError getViewAngle(int& angle);
	SRCError setViewAngle(int angle);

	SRCError getSupportedMovieQuality(std::string& json);
	SRCError getAvailableMovieQuality(std::string& json);
	SRCError getMovieQuality(std::string& json);
	SRCError setMovieQuality(const std::string& quality); // TODO create arg
	
	SRCError getSupportedSteadyMode(std::string& json);
	SRCError getAvailableSteadyMode(std::string& json);
	
	SRCError getAvailableCameraFunction(std::string& json);
	SRCError getStorageInformation(std::string& json);

	//-----------------------------------------------------------------
	// My Helper Functions
	//-----------------------------------------------------------------
	std::string getErrorString(SRCError err) const;
	std::string getShootModeString(ShootMode mode) const;

private:
	virtual void threadedFunction();
	bool updateLiveView();
	bool updateCommonHeader();
	bool updatePayloadHeader();
	bool updatePayloadData();
	bool openLiveViewSession(const std::string& host, int port);
	void closeLiveViewSession();

	std::string httpPost(const std::string& json, const std::string& path);
	//json	
	std::string createJson(const std::string& method, const std::vector<picojson::value>& params=std::vector<picojson::value>()) const;
	picojson::value parse(const std::string& json)  const;
	bool getJsonResultArray(picojson::array&  outArray, const std::string& json) const;
	SRCError checkError(const std::string& json) const;
	SRCError cvtError(int errorcode) const;
	//
	int bytesToInt(BYTE byteData[], int startIndex, int count) const;
	
private:

	std::string mHost;
	int mPort;
	int mId;

	bool mIsVerbose;

	int mLiveViewTimestamp;
	int mLastLiveViewTimestamp;
	bool mIsLiveViewStreaming;
	ofPixels mLiveViewPixels;

	bool mIsImageSizeUpdated;
	ImageSize mImageSize;

	Poco::Net::HTTPClientSession mLiveViewSession;
	std::string mLiveViewPath;
	Poco::Net::HTTPClientSession mSession;
	std::string mSessionCameraPath;
	std::string mSessionGuidePath;
	std::string mSessionAccessControlPath;

	std::istream* mpLiveViewStream;

	CommonHeader mCommonHeader;
	PayloadHeader mPayloadHeader;
};