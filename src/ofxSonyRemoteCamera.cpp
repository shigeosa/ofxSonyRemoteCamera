//
//  ofxSonyRemoteCamera.cpp
//  Created by Osamu Shigeta on 9/12/2013.
//
#include "ofxSonyRemoteCamera.h"

static const std::string VERSION("1.0");
static const std::string ACTION_LIST_URL("sony");
static const std::string SERVICE_TYPE_CAMERA("camera");
static const std::string SERVICE_TYPE_GUIDE("guide");
static const  std::string SERVICE_TYPE_ACCESS_CONTROL("accessControl");
static const int DEFAULT_ID(1);
static const int COMMON_HEADER_SIZE(1+1+2+4);
static const int PAYLOAD_HEADER_SIZE(4+3+1+4+1+115);
static const BYTE COMMON_HEADER_START_BYTE(0xff);
static const BYTE PAYLOAD_HEADER_START_BYTES[] = {0x24, 0x35, 0x68, 0x79};
//static const unsigned long long SESSION_TIMEOUT(5000*1000);	//!< ms

ofxSonyRemoteCamera::ofxSonyRemoteCamera()	
{
}

ofxSonyRemoteCamera::~ofxSonyRemoteCamera()
{
	exit();
}

bool ofxSonyRemoteCamera::setup( const std::string& host/*="10.0.0.1"*/, int port/*=10000*/ )
{
	mHost = host;
	mPort = port;
	mId = DEFAULT_ID;
	mIsLiveViewStreaming = false;
	mIsVerbose = true;
	mLiveViewTimestamp = 0;
	mLastLiveViewTimestamp = 0;
	mpLiveViewStream = 0;
	mIsImageSizeUpdated = false;

	mSession.reset();
	mSession.setHost(mHost);
	mSession.setPort(port);
	mSession.setKeepAlive(true);

	mSessionCameraPath = "/" + ACTION_LIST_URL + "/" + SERVICE_TYPE_CAMERA;
	mSessionGuidePath =  "/" + ACTION_LIST_URL + "/" + SERVICE_TYPE_GUIDE;
	mSessionAccessControlPath =  "/" + ACTION_LIST_URL + "/" + SERVICE_TYPE_ACCESS_CONTROL;
	return true;
}

void ofxSonyRemoteCamera::exit()
{
	stopLiveView();
	mSession.reset();
}

void ofxSonyRemoteCamera::update()
{
	if (mIsImageSizeUpdated) {
		if (lock()) {
			ofNotifyEvent(imageSizeUpdated, mImageSize);
			mIsImageSizeUpdated = false;
			unlock();
		}
	}
	/*
	if (lock()) {
		if (mIsImageSizeUpdated) {
			ofNotifyEvent(imageSizeUpdated, mImageSize);
			mIsImageSizeUpdated = false;
		}
		unlock();
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
// LiveViewAPIs
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::startLiveView()
{
	waitForThread();
	mIsLiveViewStreaming = false;
	closeLiveViewSession();

	const std::string json(httpPost(createJson("startLiveview"), mSessionCameraPath));
	SRCError err(checkError(json));
	if (err != SRC_OK) return err;

	Poco::URI uri;
	picojson::array resultArray;
	if (getJsonResultArray(resultArray, json)) {
		uri = resultArray[0].get<std::string>();
	}
	mLiveViewPath = uri.getPathAndQuery();
	if (!openLiveViewSession(uri.getHost(), uri.getPort())) {
		return SRC_ERROR_UNKNOWN;;
	}
	mIsLiveViewStreaming = true;
	startThread();
	return SRC_OK;
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::stopLiveView()
{
	waitForThread();
	mIsLiveViewStreaming = false;
	closeLiveViewSession();

	const std::string json(httpPost(createJson("stopLiveview"), mSessionCameraPath));
	return checkError(json);
}

bool ofxSonyRemoteCamera::isLiveViewFrameNew()
{
	if (lock()) {
		bool isNew(false);
		if (mLiveViewTimestamp != mLastLiveViewTimestamp) {
			mLastLiveViewTimestamp = mLiveViewTimestamp;
			isNew = true;
		}
		unlock();
		return isNew;
	}
	return false;
}

bool ofxSonyRemoteCamera::isLiveViewSessionConnected()
{
	return mLiveViewSession.connected();
}

void ofxSonyRemoteCamera::getLiveViewImage( unsigned char* pImg, int& timestamp )
{
	// memcpy_s
	if (lock()) {
		timestamp = mCommonHeader.timestamp;
		memcpy(pImg, mLiveViewPixels.getPixels(), mLiveViewPixels.getWidth()*mLiveViewPixels.getHeight()*mLiveViewPixels.getBytesPerPixel());
		unlock();
	} else if (mIsVerbose) {
		std::cout << "cannot lock" << std::endl;
	}
}

void ofxSonyRemoteCamera::getLiveViewImage( ofPixels& pixels, int& timestamp )
{
	if (lock()) {
		timestamp = mCommonHeader.timestamp;
		pixels = mLiveViewPixels;
		unlock();
	} else if (mIsVerbose) {
		std::cout << "cannot lock" << std::endl;
	}
}

int ofxSonyRemoteCamera::getLiveViewImageWidth() const
{
	return mLiveViewPixels.getWidth();
}

int ofxSonyRemoteCamera::getLiveViewImageHeight() const
{
	return mLiveViewPixels.getHeight();
}

void ofxSonyRemoteCamera::getCommonHeader(CommonHeader& header)
{
	if (lock()) {
		header = mCommonHeader;
		unlock();
	}
}

void ofxSonyRemoteCamera::getPayloadHeader(PayloadHeader& header)
{
	if (lock()) {
		header = mPayloadHeader;
		unlock();
	}
}
//////////////////////////////////////////////////////////////////////////
// Still Capture
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::actTakePicture()
{
	const std::string json(httpPost(createJson("actTakePicture"), mSessionCameraPath));
	picojson::array resultArray;
	if (getJsonResultArray(resultArray, json)) {
		mPostViewPath = resultArray[0].get<std::string>();
	}
	return checkError(json);
	/*
	httpPostAsync(createJson("actTakePicture"), mSessionCameraPath);
	return SRC_OK;
	*/
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::awaitTakePicture()
{
	const std::string json(httpPost(createJson("awaitTakePicture"), mSessionCameraPath));
	return checkError(json);	
}
//////////////////////////////////////////////////////////////////////////
// Movie recording
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::startMovieRec()
{
	const std::string json(httpPost(createJson("startMovieRec"), mSessionCameraPath));
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::stopMovieRec()
{
	const std::string json(httpPost(createJson("stopMovieRec"), mSessionCameraPath));
	return checkError(json);	
}
//////////////////////////////////////////////////////////////////////////
// Zoom
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::actZoom(const std::string& direction, const std::string& movement)
{
	std::vector<picojson::value> params(2);
	params[0] = static_cast<picojson::value>(static_cast<std::string>(direction));
	params[1] = static_cast<picojson::value>(static_cast<std::string>(movement));
	
	const std::string json(httpPost(createJson("actZoom", params), mSessionCameraPath));
	return checkError(json);
	/*
	httpPostAsync(createJson("actZoom", params), mSessionCameraPath);
	return SRC_OK;
	*/
}
//////////////////////////////////////////////////////////////////////////
// Self-timer
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getSupportedSelfTimer( std::string& json )
{
	json = httpPost(createJson("getSupportedSelfTimer"), mSessionCameraPath);
	return checkError(json);
}
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailableSelfTimer( std::string& json )
{
	json = httpPost(createJson("getAvailableSelfTimer"), mSessionCameraPath);
	return checkError(json);
}
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getSelfTimer(int& second)
{
	const std::string json(httpPost(createJson("getSelfTimer"), mSessionCameraPath));
	SRCError err(checkError(json));
	if (err != SRC_OK) return err;

	picojson::array resultArray;
	if (getJsonResultArray(resultArray, json)) {
		second = resultArray[0].get<double>();
		return SRC_OK;
	}
	return SRC_ERROR_UNKNOWN;	
}
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::setSelfTimer(int second)
{
	std::vector<picojson::value> params(1);
	params[0] = static_cast<picojson::value>(static_cast<double>(second));
	const std::string json(httpPost(createJson("setSelfTimer", params), mSessionCameraPath));
	return checkError(json);	
}
//////////////////////////////////////////////////////////////////////////
// Postview image size
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getSupportedPostViewImageSize( std::string& json )
{
	json = httpPost(createJson("getSupportedPostviewImageSize"), mSessionCameraPath);
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailablePostViewImageSize( std::string& json )
{
	json = httpPost(createJson("getAvailablePostviewImageSize"), mSessionCameraPath);
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getPostViewImageSize(PostViewImageSize& size)
{
	const std::string json(httpPost(createJson("getPostviewImageSize"), mSessionCameraPath));
	SRCError err(checkError(json));
	if (err != SRC_OK) return err;

	picojson::array resultArray;
	if (getJsonResultArray(resultArray, json)) {
		if (resultArray[0].get<std::string>().compare("Original") == 0) {
			size = POST_VIEW_IMG_SIZE_ORIGINAL;
			return SRC_OK;
		} else if (resultArray[0].get<std::string>().compare("2M") == 0) {
			size = POST_VIEW_IMG_SIZE_2M;
			return SRC_OK;
		} else {
			ofLogError("not implemented yet.");
		}
	}
	return SRC_ERROR_UNKNOWN;	
}
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::setPostViewImageSize(PostViewImageSize size)
{

	std::vector<picojson::value> params(1);
	switch (size) {
		case POST_VIEW_IMG_SIZE_ORIGINAL:
			params[0] = static_cast<picojson::value>(static_cast<std::string>("Original"));
			break;
		case POST_VIEW_IMG_SIZE_2M:
			params[0] = static_cast<picojson::value>(static_cast<std::string>("2M"));
			break;
		default:
			ofLogError("not implemented yet.");
			break;
		}

	const std::string json(httpPost(createJson("setShootMode", params), mSessionCameraPath));
	return checkError(json);
}

//////////////////////////////////////////////////////////////////////////
// Shoot mode
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getSupportedShootMode( std::string& json )
{
	json = httpPost(createJson("getSupportedShootMode"), mSessionCameraPath);
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailableShootMode( std::string& json )
{
	json = httpPost(createJson("getAvailableShootMode"), mSessionCameraPath);
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getShootMode(ShootMode& mode)
{
	const std::string json(httpPost(createJson("getShootMode"), mSessionCameraPath));
	SRCError err(checkError(json));
	if (err != SRC_OK) return err;

	picojson::array resultArray;
	if (getJsonResultArray(resultArray, json)) {
		if (resultArray[0].get<std::string>().compare("movie") == 0) {
			mode = SHOOT_MODE_MOVIE;
			return SRC_OK;
		} else if (resultArray[0].get<std::string>().compare("still") == 0) {
			mode = SHOOT_MODE_STILL;
			return SRC_OK;
		} else if (resultArray[0].get<std::string>().compare("intervalstill") == 0) {
			mode = SHOOT_MODE_INTERVAL_STILL;
			return SRC_OK;
		} else {
			ofLogError("not implemented yet.");
		}
	}
	return SRC_ERROR_UNKNOWN;	
}
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::setShootMode(ShootMode mode)
{

	std::vector<picojson::value> params(1);
	switch (mode) {
		case SHOOT_MODE_MOVIE:
			params[0] = static_cast<picojson::value>(static_cast<std::string>("movie"));
			break;
		case SHOOT_MODE_STILL:
			params[0] = static_cast<picojson::value>(static_cast<std::string>("still"));
			break;
		case SHOOT_MODE_INTERVAL_STILL:
			params[0] = static_cast<picojson::value>(static_cast<std::string>("intervalstill"));
			break;
		default:
			ofLogError("not implemented yet.");
			break;
		}

	const std::string json(httpPost(createJson("setShootMode", params), mSessionCameraPath));
	return checkError(json);
}
//////////////////////////////////////////////////////////////////////////
// Event notification
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getEvent( std::string& json, bool pollingFlag )
{
	std::vector<picojson::value> params(1);
	params[0] = static_cast<picojson::value>(static_cast<bool>(pollingFlag));
	json = httpPost(createJson("getEvent", params), mSessionCameraPath);
	return checkError(json);
}
//////////////////////////////////////////////////////////////////////////
// Camera setup
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::startRecMode()
{
	const std::string json(httpPost(createJson("startRecMode"), mSessionCameraPath));
	return checkError(json);
}
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::stopRecMode()
{
	const std::string json(httpPost(createJson("stopRecMode"), mSessionCameraPath));
	return checkError(json);
}
//////////////////////////////////////////////////////////////////////////
// Server information
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailableApiList( std::string& json )
{
	json = httpPost(createJson("getAvailableApiList"), mSessionCameraPath);
	return checkError(json);
}
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getMethodTypes( std::string& json )
{
	std::vector<picojson::value> params(1);
	params[0] = static_cast<picojson::value>(static_cast<std::string>(VERSION));
	json = httpPost(createJson("getMethodTypes", params), mSessionCameraPath);
	return checkError(json);
}
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getVersions( std::string& json )
{
	json = httpPost(createJson("getVersions"), mSessionCameraPath);
	return checkError(json);
}
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getApplicationInfo( std::string& json )
{
	json = httpPost(createJson("getApplicationInfo"), mSessionCameraPath);
	return checkError(json);
}
//////////////////////////////////////////////////////////////////////////
// othrers
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::startIntervalStillRec()
{
	const std::string json(httpPost(createJson("startIntervalStillRec"), mSessionCameraPath));
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::stopIntervalStillRec()
{
	const std::string json(httpPost(createJson("stopIntervalStillRec"), mSessionCameraPath));
	return checkError(json);
}


ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getSupportedViewAngle( std::string& json )
{
	json = httpPost(createJson("getSupportedViewAngle"), mSessionCameraPath);
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailableViewAngle( std::string& json )
{
	json = httpPost(createJson("getAvailableViewAngle"), mSessionCameraPath);
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getViewAngle( int& angle )
{
	const std::string json(httpPost(createJson("getViewAngle"), mSessionCameraPath));
	SRCError err(checkError(json));
	if (err != SRC_OK) return err;

	picojson::array resultArray;
	if (getJsonResultArray(resultArray, json)) {
		angle = resultArray[0].get<double>();
		return SRC_OK;
	}
	return SRC_ERROR_UNKNOWN;	
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::setViewAngle(int angle)
{
	std::vector<picojson::value> params(1);
	params[0] = static_cast<picojson::value>(static_cast<double>(angle));
	const std::string json(httpPost(createJson("setViewAngle", params), mSessionCameraPath));
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getSupportedMovieQuality( std::string& json )
{
	json = httpPost(createJson("getSupportedMovieQuality"), mSessionCameraPath);
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailableMovieQuality( std::string& json )
{
	json = httpPost(createJson("getAvailableMovieQuality"), mSessionCameraPath);
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getMovieQuality( std::string& json )
{
	json = httpPost(createJson("getMovieQuality"), mSessionCameraPath);
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::setMovieQuality( const std::string& quality )
{
	std::vector<picojson::value> params(1);
	params[0] = static_cast<picojson::value>(static_cast<std::string>(quality));
	const std::string json(httpPost(createJson("setMovieQuality", params), mSessionCameraPath));
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getSupportedSteadyMode( std::string& json )
{
	json = httpPost(createJson("getSupportedSteadyMode"), mSessionCameraPath);
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailableSteadyMode( std::string& json )
{
	json = httpPost(createJson("getAvailableSteadyMode"), mSessionCameraPath);
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getStorageInformation( std::string& json )
{
	json = httpPost(createJson("getStorageInformation"), mSessionCameraPath);
	return checkError(json);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailableCameraFunction( std::string& json )
{
	json = httpPost(createJson("getAvailableCameraFunction"), mSessionCameraPath);
	return checkError(json);
}

//////////////////////////////////////////////////////////////////////////
// Helper Functions
//////////////////////////////////////////////////////////////////////////
std::string ofxSonyRemoteCamera::getErrorString(SRCError err) const
{
	switch (err) {
	case SRC_OK:
		return "OK";
	case SRC_ERROR_ANY:
		return "ERROR_ANY";
	case SRC_ERROR_TIMEOUT:
		return "ERROR_TIMEOUT";
	case SRC_ERROR_ILLEGAL_ARGUMENT:
		return "ERROR_ILLEGAL_ARGUMENT";
	case SRC_ERROR_ILLEGAL_DATA_FORMAT:
		return "ERROR_ILLEGAL_DATA_FORMAT";
	case SRC_ERROR_ILLEGAL_REQUEST:
		return "ERROR_ILLEGAL_REQUEST";
	case SRC_ERROR_ILLEGAL_RESPONSE:
		return "ERROR_ILLEGAL_RESPONSE";
	case SRC_ERROR_ILLEGAL_STATE:
		return "ERROR_ILLEGAL_STATE";
	case SRC_ERROR_ILLEGAL_TYPE:
		return "ERROR_ILLEGAL_TYPE";
	case SRC_ERROR_INDEX_OUT_OF_BOUNDS:
		return "ERROR_INDEX_OUT_OF_BOUNDS";
	case SRC_ERROR_NO_SUCH_ELEMENT:
		return "ERROR_NO_SUCH_ELEMENT";
	case SRC_ERROR_NO_SUCH_FIELD:
		return "ERROR_NO_SUCH_FIELD";
	case SRC_ERROR_NO_SUCH_METHOD:
		return "ERROR_NO_SUCH_METHOD";
	case SRC_ERROR_NULL_POINTER:
		return "ERROR_NULL_POINTER";
	case SRC_ERROR_UNSUPPORTED_VERSION:
		return "ERROR_UNSUPPORTED_VERSION";
	case SRC_ERROR_UNSUPPORTED_OPERATION:
		return "ERROR_UNSUPPORTED_OPERATION";
	case SRC_ERROR_SHOOTING_FAIL:
		return "ERROR_SHOOTING_FAIL";
	case SRC_ERROR_CAMERA_NOT_READY:
		return "ERROR_CAMERA_NOT_READY";
	case SRC_ERROR_ALREADY_RUNNING_POLLING_API:
		return "ERROR_ALREADY_RUNNING_POLLING_API";
	case SRC_ERROR_STILL_CAPTURING_NOT_FINISHED:
		return "ERROR_STILL_CAPTURING_NOT_FINISHED";
	}
	return "ERROR_UNKNOWN";
}

std::string ofxSonyRemoteCamera::getShootModeString(ShootMode mode) const
{
	switch (mode) {
	case SHOOT_MODE_MOVIE:
		return "MOVIE";
	case SHOOT_MODE_STILL:
		return "STILL";
	case SHOOT_MODE_INTERVAL_STILL:
		return "INTERVAL STILL";
	}
	return "UNKNOWN";
}

//////////////////////////////////////////////////////////////////////////
// private functions
//////////////////////////////////////////////////////////////////////////
void ofxSonyRemoteCamera::threadedFunction()
{
	while (isThreadRunning()) {
		if (!updateLiveView()) {
			this->sleep(1);
		}
		//updateRequest();
	}
}

bool ofxSonyRemoteCamera::openLiveViewSession( const std::string& host, int port )
{
	mLiveViewSession.reset();
	mLiveViewSession.setHost(host);
	mLiveViewSession.setPort(port);
	mLiveViewSession.setKeepAlive(true);

	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, mLiveViewPath, Poco::Net::HTTPMessage::HTTP_1_1);
	Poco::Net::HTTPResponse response;
	mLiveViewSession.sendRequest(request);
	istream& res = mLiveViewSession.receiveResponse(response);
	if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK)
	{
		mpLiveViewStream = &res;
		return true;
	}
	return false;;
}

void ofxSonyRemoteCamera::closeLiveViewSession()
{
	mpLiveViewStream = 0;
	mLiveViewSession.reset();
}

bool ofxSonyRemoteCamera::updateLiveView()
{
	if (!updateCommonHeader()) return false;
	if (!updatePayloadHeader()) return false;
	if (!updatePayloadData()) return false;
	if (lock()) {
		mLiveViewTimestamp = mCommonHeader.timestamp;
		unlock();
	}
	//++mLiveViewFrameId;
	return true;
}

bool ofxSonyRemoteCamera::updateCommonHeader()
{
	if (mpLiveViewStream == 0) return false;

	BYTE commonHeaderBytes[COMMON_HEADER_SIZE];
	mpLiveViewStream->read((char*)commonHeaderBytes, COMMON_HEADER_SIZE);
	// check header
	if ((mpLiveViewStream->gcount() != COMMON_HEADER_SIZE) ||
		(commonHeaderBytes[0] != COMMON_HEADER_START_BYTE)
		) {
			return false;	
	}
	if (lock()) {
		mCommonHeader.payLoadType = bytesToInt(commonHeaderBytes, 1, 1);
		mCommonHeader.frameId = bytesToInt(commonHeaderBytes, 2, 2);
		mCommonHeader.timestamp = bytesToInt(commonHeaderBytes, 4, 4);
		unlock();
	}
	/*
	std::cout << "startByte: " << (int)commonHeaderBytes[0] << std::endl;
	std::cout << "payLoadType: " << mCommonHeader.payLoadType << std::endl;
	std::cout << "sequenceNumber: " << mCommonHeader.frameId << std::endl;
	std::cout << "timestamp: " << mCommonHeader.timestamp << std::endl;
	*/
	return true;
}

bool ofxSonyRemoteCamera::updatePayloadHeader()
{
	if (mpLiveViewStream == 0) return false;

	BYTE payloadHeaderBytes[PAYLOAD_HEADER_SIZE];
	mpLiveViewStream->read((char*)payloadHeaderBytes, PAYLOAD_HEADER_SIZE);
	// check header
	if ((mpLiveViewStream->gcount() != PAYLOAD_HEADER_SIZE) ||
		(payloadHeaderBytes[0] != PAYLOAD_HEADER_START_BYTES[0]) ||
		(payloadHeaderBytes[1] != PAYLOAD_HEADER_START_BYTES[1]) ||
		(payloadHeaderBytes[2] != PAYLOAD_HEADER_START_BYTES[2]) ||
		(payloadHeaderBytes[3] != PAYLOAD_HEADER_START_BYTES[3])
		) {
			return false;	
	}
	if (lock()) {
		mPayloadHeader.jpegSize = bytesToInt(payloadHeaderBytes,4,3);
		mPayloadHeader.paddingSize = bytesToInt(payloadHeaderBytes,7,1);
		unlock();
	}
	return true;
}

bool ofxSonyRemoteCamera::updatePayloadData()
{
	if (mpLiveViewStream == 0) return false;
	
	int jpegSize(0);
	if (lock()) {
		jpegSize = mPayloadHeader.jpegSize;
		unlock();
	}
	BYTE* apJpegBytes = new BYTE[jpegSize];
	mpLiveViewStream->read((char*)apJpegBytes, jpegSize);
	if (mpLiveViewStream->gcount() != jpegSize) {
		delete [] apJpegBytes;
		return false;
	}
	ofBuffer buf((char*)apJpegBytes, jpegSize);
	// cvt jpeg to bitmap
	if (lock()) {
		ofLoadImage(mLiveViewPixels, buf);
		if ( (mLiveViewPixels.getWidth() != mImageSize.width) || (mLiveViewPixels.getHeight() != mImageSize.height)) {
			mImageSize.width = mLiveViewPixels.getWidth();
			mImageSize.height = mLiveViewPixels.getHeight();
			mIsImageSizeUpdated = true;
		}
		unlock();
	} else if (mIsVerbose) {
		std::cout << "cannot lock" << std::endl;
	}
	delete [] apJpegBytes;
	return true;
}

void ofxSonyRemoteCamera::updateRequest()
{
	lock();
	mHttpPostList = mHttpPostListEntry;
	mHttpPostListEntry.clear();
	unlock();
	while (mHttpPostList.size()) {
		const std::string json(httpPost(mHttpPostList.front().json, mHttpPostList.front().path));
		std::cout << json << std::endl;
		mHttpPostList.pop_front();
	}
	
}

std::string ofxSonyRemoteCamera::httpPost( const std::string& json, const std::string& path )
{
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, path, Poco::Net::HTTPMessage::HTTP_1_1);
	request.setContentLength(json.length());
	request.setContentType("application/json");
	mSession.sendRequest(request) << json;

	Poco::Net::HTTPResponse response;
	std::istream& rs = mSession.receiveResponse(response);
	if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED)
	{
		std::string responseStr;
		Poco::StreamCopier::copyToString(rs, responseStr);
		return responseStr;
		// TODO
	}
	return "";
}

std::string ofxSonyRemoteCamera::httpPostAsync( const std::string& json, const std::string& path )
{
	MyHttpPostRequest r(json, path);
	lock();
	mHttpPostListEntry.push_back(r);
	unlock();
	return "";
}

picojson::value ofxSonyRemoteCamera::parse(const std::string& json) const
{
	picojson::value v;
	const char* m(json.c_str());
	std::string err;
	picojson::parse(v, m, m+strlen(m), &err);
	if (!err.empty()) {
		ofLogError("JSON parse error: " + err);
	}
	return v;
}

std::string ofxSonyRemoteCamera::createJson(const std::string& method, const std::vector<picojson::value>& params ) const
{
	picojson::object obj;
	obj["method"] = (picojson::value)(std::string)(method);
	obj["id"] = (picojson::value)(double)(mId);
	obj["version"] = (picojson::value)(std::string)(VERSION);
	picojson::array paramArray;
	for (std::vector<picojson::value>::const_iterator it=params.begin(); it!=params.end(); ++it) {
		paramArray.push_back(*it);
	}
	obj.insert(make_pair("params", paramArray));
	return (static_cast<picojson::value>(obj)).serialize();
}

bool ofxSonyRemoteCamera::getJsonResultArray(picojson::array& outArray, const std::string& json) const
{
	const picojson::value v = parse(json);
	const picojson::value::object& obj(v.get<picojson::object>());
	for (picojson::value::object::const_iterator it=obj.begin(); it!=obj.end(); ++it) {
		if ( (it->first).compare("result") == 0) {
			outArray = it->second.get<picojson::array>();
			return true;
		}
	}
	return false;
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::checkError( const std::string& json ) const
{
	const picojson::value v = parse(json);
	const picojson::value::object& obj(v.get<picojson::object>());
	
	int errcode(0);
	for (picojson::value::object::const_iterator it=obj.begin(); it!=obj.end(); ++it) {
		//std::cout << it->first << ": " <<  it->second.to_str() << std::endl;
		if ( (it->first).compare("error") == 0) {
		picojson::array a(it->second.get<picojson::array>());
		errcode = a[0].get<double>();		
		}
	}
	if (errcode == 0) return SRC_OK;
	if (mIsVerbose) {
		//ofLogError(json);
		std::cout << json << std::endl;
	}
	return cvtError(errcode);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::cvtError(int errorcode) const {

	if (SRC_OK <= errorcode && errorcode <= SRC_ERROR_UNSUPPORTED_OPERATION) {
		return static_cast<SRCError>(errorcode);
	}
	if (SRC_ERROR_SHOOTING_FAIL <= errorcode && errorcode <= SRC_ERROR_STILL_CAPTURING_NOT_FINISHED) {
		return static_cast<SRCError>(errorcode);
	}
	if (mIsVerbose) {
		ofLogError("Error code: " + ofToString(errorcode));
		ofLogError("Error code is HTTP status code or Unknown.");

	}
	return SRC_ERROR_UNKNOWN;
}

int ofxSonyRemoteCamera::bytesToInt(BYTE byteData[], int startIndex, int count) const
{
	int ret(0);
	for (int i(startIndex); i<startIndex+count; ++i) {
		ret = (ret << 8) | (byteData[i] & 0xff);
	}
	return ret;
}







