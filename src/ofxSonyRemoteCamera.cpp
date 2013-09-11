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
static const unsigned long long SESSION_TIMEOUT(5000*1000);	//!< ms

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

	mSession.setHost(mHost);
	mSession.setPort(port);
	mSession.reset();
	mSession.setTimeout(SESSION_TIMEOUT);
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
	if (lock()) {
		if (mIsImageSizeUpdated) {
			ofNotifyEvent(imageSizeUpdated, mImageSize);
			mIsImageSizeUpdated = false;
		}
		unlock();
	}
}

//////////////////////////////////////////////////////////////////////////
// LiveViewAPIs
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::startLiveView()
{
	waitForThread();
	mIsLiveViewStreaming = false;
	closeLiveViewSession();

	picojson::object obj(createJsonObj("startLiveview"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string json( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(json, mSessionCameraPath));

	SRCError err(checkError(ret));
	if (err != SRC_OK) return err;

	Poco::URI uri;
	picojson::array resultArray;
	if (getJsonResultArray(resultArray, ret)) {
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

	picojson::object obj(createJsonObj("stopLiveview"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string json( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(json, mSessionCameraPath));
	return checkError(ret);
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
// CameraAPIs
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::startMovieRec()
{
	picojson::object obj(createJsonObj("startMovieRec"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::stopMovieRec()
{
	picojson::object obj(createJsonObj("stopMovieRec"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getSupportedShootMode( std::string& json )
{
	picojson::object obj(createJsonObj("getSupportedShootMode"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailableShootMode( std::string& json )
{
	picojson::object obj(createJsonObj("getAvailableShootMode"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getShootMode(ShootMode& mode)
{
	picojson::object obj(createJsonObj("getShootMode"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	SRCError err(checkError(ret));
	if (err != SRC_OK) return err;

	picojson::array resultArray;
	if (getJsonResultArray(resultArray, ret)) {
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
	picojson::object obj(createJsonObj("setShootMode"));
	{
		picojson::array params;
		switch (mode) {
		case SHOOT_MODE_MOVIE:
			params.push_back(static_cast<picojson::value>(static_cast<std::string>("movie")));
			break;
		case SHOOT_MODE_STILL:
			params.push_back(static_cast<picojson::value>(static_cast<std::string>("still")));
			break;
		case SHOOT_MODE_INTERVAL_STILL:
			params.push_back(static_cast<picojson::value>(static_cast<std::string>("intervalstill")));
			break;
		default:
			ofLogError("not implemented yet.");
			break;
		}
		obj.insert(make_pair("params", params));
	}
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailableApiList( std::string& json )
{
	picojson::object obj(createJsonObj("getAvailableApiList"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getMethodTypes( std::string& json )
{
	picojson::object obj(createJsonObj("getMethodTypes"));
	picojson::array params;
	params.push_back( (picojson::value)(std::string)("1.0"));
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getVersions( std::string& json )
{
	picojson::object obj(createJsonObj("getVersions"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getEvent( std::string& json, bool pollingFlag )
{
	picojson::object obj(createJsonObj("getEvent"));
	picojson::array params;
	params.push_back(static_cast<picojson::value>(bool(pollingFlag)));
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
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
// CameraAPIs
//////////////////////////////////////////////////////////////////////////
ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::startRecMode()
{
	picojson::object obj(createJsonObj("startRecMode"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::stopRecMode()
{
	picojson::object obj(createJsonObj("stopRecMode"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::startIntervalStillRec()
{
	picojson::object obj(createJsonObj("startIntervalStillRec"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::stopIntervalStillRec()
{
	picojson::object obj(createJsonObj("stopIntervalStillRec"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	return checkError(ret);
}


ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getSupportedViewAngle( std::string& json )
{
	picojson::object obj(createJsonObj("getSupportedViewAngle"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailableViewAngle( std::string& json )
{
	picojson::object obj(createJsonObj("getAvailableViewAngle"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getViewAngle( int& angle )
{
	picojson::object obj(createJsonObj("getViewAngle"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	SRCError err(checkError(ret));
	if (err != SRC_OK) return err;

	picojson::array resultArray;
	if (getJsonResultArray(resultArray, ret)) {
		angle = resultArray[0].get<double>();
		return SRC_OK;
	}
	return SRC_ERROR_UNKNOWN;	
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::setViewAngle(int angle)
{
	picojson::object obj(createJsonObj("setViewAngle"));
	picojson::array params;
	params.push_back( (picojson::value)(double)(angle));
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getSupportedMovieQuality( std::string& json )
{
	picojson::object obj(createJsonObj("getSupportedMovieQuality"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailableMovieQuality( std::string& json )
{
	picojson::object obj(createJsonObj("getAvailableMovieQuality"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getMovieQuality( std::string& json )
{
	picojson::object obj(createJsonObj("getMovieQuality"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::setMovieQuality( const std::string& quality )
{
	picojson::object obj(createJsonObj("setMovieQuality"));
	picojson::array params;
	params.push_back( (picojson::value)(std::string)(quality));
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getSupportedSteadyMode( std::string& json )
{
	picojson::object obj(createJsonObj("getSupportedSteadyMode"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailableSteadyMode( std::string& json )
{
	picojson::object obj(createJsonObj("getAvailableSteadyMode"));
	picojson::array params;
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getStorageInformation( std::string& json )
{
	picojson::object obj(createJsonObj("getStorageInformation"));
	picojson::array params;
	params.push_back( (picojson::value)(std::string)("1.0"));
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getAvailableCameraFunction( std::string& json )
{
	picojson::object obj(createJsonObj("getAvailableCameraFunction"));
	picojson::array params;
	params.push_back( (picojson::value)(std::string)("1.0"));
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
}

ofxSonyRemoteCamera::SRCError ofxSonyRemoteCamera::getApplicationInfo( std::string& json )
{
	picojson::object obj(createJsonObj("getApplicationInfo"));
	picojson::array params;
	params.push_back( (picojson::value)(std::string)("1.0"));
	obj.insert(make_pair("params", params));
	const std::string j( (static_cast<picojson::value>(obj)).serialize() );
	const std::string ret(httpPost(j, mSessionCameraPath));
	json = ret;
	return checkError(ret);
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
	}
}



bool ofxSonyRemoteCamera::openLiveViewSession( const std::string& host, int port )
{
	mLiveViewSession.setHost(host);
	mLiveViewSession.setPort(port);
	mLiveViewSession.reset();
	mLiveViewSession.setTimeout(SESSION_TIMEOUT);

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

picojson::object ofxSonyRemoteCamera::createJsonObj( const std::string& method ) const
{
	picojson::object obj;
	obj["method"] = (picojson::value)(std::string)(method);
	obj["id"] = (picojson::value)(double)(mId);
	obj["version"] = (picojson::value)(std::string)(VERSION);
	return obj;
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







