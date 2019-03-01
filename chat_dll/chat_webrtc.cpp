#include "rtc_base/win32.h"
#include "chat_webrtc.h"
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include <memory>
#include <utility>
#include <vector>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "media/engine/webrtcvideocapturerfactory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/video_capture/video_capture_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/json.h"
#include "rtc_base/logging.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "api/video/i420_buffer.h"
#include "rtc_base/ssladapter.h"
#include "rtc_base/win32socketinit.h"
#include "rtc_base/win32socketinit.h"
#include "rtc_base/win32socketserver.h"
#include <assert.h>
#include "VideoPlayerGDI.h"


// ChatDlg 消息处理程序
static HWND hwndlocal;
static HWND hwndRemote;
void Start();

VideoPlayerGDI *playerLocal;
VideoPlayerGDI * playerRemote;

class A :public webrtc::CreateSessionDescriptionObserver {
	void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
		RTC_LOG(LERROR) << "OnSuccess";
	}
	void OnFailure(webrtc::RTCError error) override {
		RTC_LOG(LERROR) << ToString(error.type()) << ": " << error.message();
	}
	void OnFailure(const std::string& error) override {
		RTC_LOG(LERROR) << error;
	}
};

CHAT_EXPORT void CHAT_CALL Start(HWND localWnd, HWND remoteWnd) {
	hwndlocal = localWnd;
	hwndRemote = remoteWnd;
	playerLocal = new VideoPlayerGDI(localWnd);
	playerRemote = new VideoPlayerGDI(remoteWnd);
	playerLocal->Init();
	playerRemote->Init();
	Start();
}


class MyObserver;
class VideoRenderer;
class DummySetSessionDescriptionObserver;

SOCKET _socket = INVALID_SOCKET;
HANDLE _thread = NULL;
bool _runFlag = true;
//SOCKET _socket = INVALID_SOCKET;
void SendString(const char* str);
void RecvString(const char* str);
DWORD WINAPI RecvThread(LPVOID param);
void OpenSocket();
void ConnectToPeer();

// Names used for a IceCandidate JSON object.
const char kCandidateSdpMidName[] = "sdpMid";
const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";
const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";
const char kStreamId[] = "stream_id";

rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;


std::unique_ptr<VideoRenderer> local_renderer_;
std::unique_ptr<VideoRenderer> remote_renderer_;
void createMyObserver();
webrtc::CreateSessionDescriptionObserver * GetSessionDescriptionObserver();
webrtc::PeerConnectionObserver* GetPeerConnectionObserver();
static std::unique_ptr<rtc::Thread> g_worker_thread;
static std::unique_ptr<rtc::Thread> g_signaling_thread;

static std::unique_ptr<rtc::Win32SocketServer> w32_ss;

static std::unique_ptr<rtc::Win32Thread> w32_thread;

void Start() {
	rtc::EnsureWinsockInit();
	rtc::InitializeSSL();
	w32_ss.reset( new rtc::Win32SocketServer());
	w32_thread.reset(new rtc::Win32Thread(w32_ss.get()));
	/*rtc::Win32SocketServer w32_ss;
	rtc::Win32Thread w32_thread(&w32_ss);*/
	rtc::ThreadManager::Instance()->SetCurrentThread(w32_thread.get());

	createMyObserver();
	OpenSocket();
	ConnectToPeer();
}



bool InitializePeerConnection();
void DeletePeerConnection();
void AddTracks();
bool CreatePeerConnection(bool dtls);
std::unique_ptr<cricket::VideoCapturer> OpenVideoCaptureDevice();



void ConnectToPeer() {


	if (peer_connection_.get()) {
		return;
	}

	if (InitializePeerConnection()) {
		peer_connection_->CreateOffer(GetSessionDescriptionObserver()/*new rtc::RefCountedObject<A>()*/, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
	}
	else {
		RTC_LOG(INFO) << ("Error Failed to initialize PeerConnection");
	}
}


bool InitializePeerConnection() {
	RTC_DCHECK(!peer_connection_factory_);
	RTC_DCHECK(!peer_connection_);
	g_worker_thread.reset(new rtc::Thread());
	g_worker_thread->Start();
	g_signaling_thread.reset(new rtc::Thread());
	g_signaling_thread->Start();
	peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
		//g_worker_thread.get(), g_worker_thread.get(), g_signaling_thread.get(), nullptr,
		nullptr /* network_thread */, nullptr /* worker_thread */,
		nullptr /* signaling_thread */, nullptr /* default_adm */,
		webrtc::CreateBuiltinAudioEncoderFactory(),
		webrtc::CreateBuiltinAudioDecoderFactory(),
		webrtc::CreateBuiltinVideoEncoderFactory(),
		webrtc::CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
		nullptr /* audio_processing */);

	if (!peer_connection_factory_) {
		RTC_LOG(INFO) << ("Error Failed to initialize PeerConnectionFactory");
		DeletePeerConnection();
		return false;
	}

	if (!CreatePeerConnection(/*dtls=*/true)) {
		RTC_LOG(INFO) << ("Error CreatePeerConnection failed");
		DeletePeerConnection();
	}

	AddTracks();

	return peer_connection_ != nullptr;
}

bool CreatePeerConnection(bool dtls) {
	RTC_DCHECK(peer_connection_factory_);
	RTC_DCHECK(!peer_connection_);

	webrtc::PeerConnectionInterface::RTCConfiguration config;
	config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
	config.enable_dtls_srtp = dtls;
	webrtc::PeerConnectionInterface::IceServer server;
	server.uri = "stun:stun.l.google.com:19302";
	config.servers.push_back(server);

	peer_connection_ = peer_connection_factory_->CreatePeerConnection(
		config, nullptr, nullptr, GetPeerConnectionObserver());
	return peer_connection_ != nullptr;
}

void DeletePeerConnection() {
	local_renderer_.reset();
	remote_renderer_.reset();
	peer_connection_ = nullptr;
	peer_connection_factory_ = nullptr;
}



template <typename T>
class AutoLock {
public:
	explicit AutoLock(T* obj) : obj_(obj) { obj_->Lock(); }
	~AutoLock() { obj_->Unlock(); }

protected:
	T* obj_;
};
class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
	VideoRenderer(HWND wnd,
		int width,
		int height,
		webrtc::VideoTrackInterface* track_to_render) : wnd_(wnd), rendered_track_(track_to_render) {
		::InitializeCriticalSection(&buffer_lock_);
		ZeroMemory(&bmi_, sizeof(bmi_));
		bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi_.bmiHeader.biPlanes = 1;
		bmi_.bmiHeader.biBitCount = 32;
		bmi_.bmiHeader.biCompression = BI_RGB;
		bmi_.bmiHeader.biWidth = width;
		bmi_.bmiHeader.biHeight = -height;
		bmi_.bmiHeader.biSizeImage =
			width * height * (bmi_.bmiHeader.biBitCount >> 3);
		rendered_track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
	}
	virtual ~VideoRenderer() {
		rendered_track_->RemoveSink(this);
		::DeleteCriticalSection(&buffer_lock_);
	}

	void Lock() { ::EnterCriticalSection(&buffer_lock_); }

	void Unlock() { ::LeaveCriticalSection(&buffer_lock_); }

	// VideoSinkInterface implementation
	void OnFrame(const webrtc::VideoFrame& video_frame) override {
		{
			AutoLock<VideoRenderer> lock(this);

			rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(
				video_frame.video_frame_buffer()->ToI420());
			if (video_frame.rotation() != webrtc::kVideoRotation_0) {
				buffer = webrtc::I420Buffer::Rotate(*buffer, video_frame.rotation());
			}

			SetSize(buffer->width(), buffer->height());

			RTC_DCHECK(image_.get() != NULL);
			libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(), buffer->DataU(),
				buffer->StrideU(), buffer->DataV(), buffer->StrideV(),
				image_.get(),
				bmi_.bmiHeader.biWidth * bmi_.bmiHeader.biBitCount / 8,
				buffer->width(), buffer->height());
		}
		if (wnd_ == hwndlocal) {
			playerLocal->Show(bmi_, image());
		}
		else {
			playerRemote->Show(bmi_, image());
		}
		//InvalidateRect(wnd_, NULL, TRUE);
	}

	const BITMAPINFO& bmi() const { return bmi_; }
	const uint8_t* image() const { return image_.get(); }

protected:
	void SetSize(int width, int height) {
		AutoLock<VideoRenderer> lock(this);

		if (width == bmi_.bmiHeader.biWidth && height == bmi_.bmiHeader.biHeight) {
			return;
		}

		bmi_.bmiHeader.biWidth = width;
		bmi_.bmiHeader.biHeight = -height;
		bmi_.bmiHeader.biSizeImage =
			width * height * (bmi_.bmiHeader.biBitCount >> 3);
		image_.reset(new uint8_t[bmi_.bmiHeader.biSizeImage]);
	}

	enum {
		SET_SIZE,
		RENDER_FRAME,
	};

	HWND wnd_;
	BITMAPINFO bmi_;
	std::unique_ptr<uint8_t[]> image_;
	CRITICAL_SECTION buffer_lock_;
	rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
};

void AddTracks() {
	if (!peer_connection_->GetSenders().empty()) {
		return;  // Already added tracks.
	}

	rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
		peer_connection_factory_->CreateAudioTrack(
			kAudioLabel, peer_connection_factory_->CreateAudioSource(
				cricket::AudioOptions())));
	auto result_or_error = peer_connection_->AddTrack(audio_track, { kStreamId });
	if (!result_or_error.ok()) {
		RTC_LOG(LS_ERROR) << "Failed to add audio track to PeerConnection: "
			<< result_or_error.error().message();
	}

	std::unique_ptr<cricket::VideoCapturer> video_device =
		OpenVideoCaptureDevice();
	if (video_device) {
		rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_(
			peer_connection_factory_->CreateVideoTrack(
				kVideoLabel, peer_connection_factory_->CreateVideoSource(
					std::move(video_device), nullptr)));
		local_renderer_.reset(new VideoRenderer(hwndlocal, 1, 1, video_track_));

		result_or_error = peer_connection_->AddTrack(video_track_, { kStreamId });
		if (!result_or_error.ok()) {
			RTC_LOG(LS_ERROR) << "Failed to add video track to PeerConnection: "
				<< result_or_error.error().message();
		}
	}
	else {
		RTC_LOG(LS_ERROR) << "OpenVideoCaptureDevice failed";
	}
}


std::unique_ptr<cricket::VideoCapturer> OpenVideoCaptureDevice() {
	std::vector<std::string> device_names;
	{
		std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
			webrtc::VideoCaptureFactory::CreateDeviceInfo());
		if (!info) {
			return nullptr;
		}
		int num_devices = info->NumberOfDevices();
		for (int i = 0; i < num_devices; ++i) {
			const uint32_t kSize = 256;
			char name[kSize] = { 0 };
			char id[kSize] = { 0 };
			if (info->GetDeviceName(i, name, kSize, id, kSize) != -1) {
				device_names.push_back(name);
			}
		}
	}

	cricket::WebRtcVideoDeviceCapturerFactory factory;
	std::unique_ptr<cricket::VideoCapturer> capturer;
	for (const auto& name : device_names) {
		capturer = factory.Create(cricket::Device(name, 0));
		if (capturer) {
			break;
		}
	}
	return capturer;
}


class DummySetSessionDescriptionObserver
	: public webrtc::SetSessionDescriptionObserver {
public:
	static DummySetSessionDescriptionObserver* Create() {
		return new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
	}
	virtual void OnSuccess() { RTC_LOG(INFO) << __FUNCTION__; }
	virtual void OnFailure(webrtc::RTCError error) {
		RTC_LOG(INFO) << __FUNCTION__ << " " << ToString(error.type()) << ": "
			<< error.message();
	}
};

class MyObserver :
	public webrtc::PeerConnectionObserver,
	public webrtc::CreateSessionDescriptionObserver {

public:
	MyObserver(int i){}

	//PeerConnectionObserver
	void OnSignalingChange(
		webrtc::PeerConnectionInterface::SignalingState new_state) override {
	};
	void OnAddTrack(
		rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
		const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
		streams) override {
		auto* track = receiver->track().release();
		if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
			auto* video_track = static_cast<webrtc::VideoTrackInterface*>(track);
			remote_renderer_.reset(new VideoRenderer(hwndRemote, 1, 1, video_track));
		}
		track->Release();
	}
	void OnRemoveTrack(
		rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override {

	}
	void OnDataChannel(
		rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {
	}
	void OnRenegotiationNeeded() override {}
	void OnIceConnectionChange(
		webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
	};
	void OnIceGatheringChange(
		webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
	};
	void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
		RTC_LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();

		Json::StyledWriter writer;
		Json::Value jcandidate;
		Json::Value jmessage;

		jcandidate[kCandidateSdpMidName] = candidate->sdp_mid();
		jcandidate[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
		std::string sdp;
		if (!candidate->ToString(&sdp)) {
			RTC_LOG(LS_ERROR) << "Failed to serialize candidate";
			return;
		}
		jcandidate[kCandidateSdpName] = sdp;
		jmessage["candidate"] = jcandidate;

		std::string message = writer.write(jmessage);
		SendString(message.c_str());
	}
	void OnIceConnectionReceivingChange(bool receiving) override {}

	//CreateSessionDescriptionObserver

	void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
		peer_connection_->SetLocalDescription(
			DummySetSessionDescriptionObserver::Create(), desc);

		std::string sdp;
		desc->ToString(&sdp);


		Json::StyledWriter writer;
		Json::Value jsdp;
		Json::Value jmessage;
		jsdp[kSessionDescriptionTypeName] =
			webrtc::SdpTypeToString(desc->GetType());
		jsdp[kSessionDescriptionSdpName] = sdp;
		jmessage["desc"] = jsdp;
		std::string str = writer.write(jmessage);
		SendString(str.c_str());
	}
	void OnFailure(webrtc::RTCError error) override {
		RTC_LOG(LERROR) << ToString(error.type()) << ": " << error.message();
	}
	 void OnFailure(const std::string& error) override {
		 RTC_LOG(LERROR) << error;
	}

	 void AddRef() const{
		ref_count_.IncRef();
	}
	 rtc::RefCountReleaseStatus Release()const {
		const auto status = ref_count_.DecRef();
		if (status == rtc::RefCountReleaseStatus::kDroppedLastRef) {
			delete this;
		}
		return status;
	}
private:
	mutable webrtc::webrtc_impl::RefCounter ref_count_{ 0 };

};
rtc::scoped_refptr<MyObserver> myObserver;

void createMyObserver() {
	myObserver= new MyObserver(1);
}
webrtc::CreateSessionDescriptionObserver * GetSessionDescriptionObserver() {
	return myObserver;
 }

webrtc::PeerConnectionObserver* GetPeerConnectionObserver() {
	return myObserver;
}


void OpenSocket() {

	int ret = 0;

	_socket = socket(AF_INET, SOCK_STREAM, 0);
	assert(_socket != INVALID_SOCKET);


	int flag1 = 1;
	setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&flag1, sizeof(flag1));
	flag1 = 1024 * 100;
	setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (const char*)&flag1, sizeof(flag1));
	setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (const char*)&flag1, sizeof(flag1));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET; //地址家族
	addr.sin_port = htons(8777); //注意转化为网络字节序
	addr.sin_addr.S_un.S_addr = inet_addr("172.20.2.252"); //使用INADDR_ANY 指示任意地址

	ret = connect(_socket, (struct sockaddr *)&addr, sizeof(addr));
	if (ret == SOCKET_ERROR) {
		closesocket(_socket); //关闭套接字
		_socket = INVALID_SOCKET;
		RTC_LOG(INFO) << "打开发送socket失败";
		return;
	}

	_runFlag = true;
	_thread = CreateThread(NULL, 0, RecvThread, 0, 0, NULL);
}

void CloseSocket() {
	_runFlag = false;
	if (_socket != INVALID_SOCKET) {
		closesocket(_socket);
		_socket = INVALID_SOCKET;
	}
	WaitForSingleObject(_thread, INFINITE);
	CloseHandle(_thread);
	_thread = NULL;
}

DWORD WINAPI RecvThread(LPVOID param) {
	const int buffSize = 1024 * 1024;
	uint8_t *buff = new uint8_t[buffSize];
	int readble = 0;
	int recvSize = 0;

	while (_runFlag) {
		recvSize = recv(_socket, (char*)buff + readble, buffSize - readble, 0);
		if (recvSize <= 0) {
			RTC_LOG(INFO) << ("recvfrom: ")<< WSAGetLastError();
			readble = 0;
			Sleep(1);
			continue;
		}
		else {
			readble += recvSize;
		}
		if (readble < 4)
			continue;

		int len = 0;
		len = ((buff[0] << 24) & 0xff000000) |
			((buff[1] << 16) & 0xff0000) |
			((buff[2] << 8) & 0xff00) |
			((buff[3]) & 0xff);
		if (len <= 0 || len > 1024 * 1024) {
			readble = 0;
			continue;
		}

		if (readble - 4 < len)
			continue;

		RTC_LOG(INFO) << "string len:"<< len;

		char * str = new char[len + 1];
		str[len] = 0;
		memcpy(str, buff + 4, len);
		readble = readble - 4 - len;
		if (readble > 0) {
			memmove(buff, buff + 4 + len, readble);
		}

		readble = 0;

		RecvString(str);
		delete[]str;
	}
	delete[] buff;
	return 0;
}

void RecvString(const char* str) {
	if (!peer_connection_.get()) {

		if (!InitializePeerConnection()) {
			RTC_LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
			return;
		}
	}

	std::string message = str;
	Json::Reader reader;
	Json::Value jmessage;
	if (!reader.parse(message, jmessage)) {
		RTC_LOG(WARNING) << "Received unknown message. " << message;
		return;
	}
	if (jmessage.isMember("desc")) {
		Json::Value jdesc;
		rtc::GetValueFromJsonObject(jmessage,"desc", &jdesc);


		std::string type_str;

		rtc::GetStringFromJsonObject(jdesc, kSessionDescriptionTypeName,
			&type_str);
		if (!type_str.empty()) {

			absl::optional<webrtc::SdpType> type_maybe =
				webrtc::SdpTypeFromString(type_str);
			if (!type_maybe) {
				RTC_LOG(LS_ERROR) << "Unknown SDP type: " << type_str;
				return;
			}
			webrtc::SdpType type = *type_maybe;
			std::string sdp;
			if (!rtc::GetStringFromJsonObject(jdesc, kSessionDescriptionSdpName,
				&sdp)) {
				RTC_LOG(WARNING) << "Can't parse received session description message.";
				return;
			}
			webrtc::SdpParseError error;
			std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
				webrtc::CreateSessionDescription(type, sdp, &error);
			if (!session_description) {
				RTC_LOG(WARNING) << "Can't parse received session description message. "
					<< "SdpParseError was: " << error.description;
				return;
			}
			RTC_LOG(INFO) << " Received session description :" << message;
			peer_connection_->SetRemoteDescription(
				DummySetSessionDescriptionObserver::Create(),
				session_description.release());
			if (type == webrtc::SdpType::kOffer) {
				peer_connection_->CreateAnswer(
					myObserver, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
			}
		}
	}
	else if (jmessage.isMember("candidate")) {
		Json::Value jcandidate;
		rtc::GetValueFromJsonObject(jmessage, "candidate", &jcandidate);

		std::string sdp_mid;
		int sdp_mlineindex = 0;
		std::string sdp;
		if (!rtc::GetStringFromJsonObject(jcandidate, kCandidateSdpMidName,
			&sdp_mid) ||
			!rtc::GetIntFromJsonObject(jcandidate, kCandidateSdpMlineIndexName,
				&sdp_mlineindex) ||
			!rtc::GetStringFromJsonObject(jcandidate, kCandidateSdpName, &sdp)) {
			RTC_LOG(WARNING) << "Can't parse received message.";
			return;
		}
		webrtc::SdpParseError error;
		std::unique_ptr<webrtc::IceCandidateInterface> candidate(
			webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, &error));
		if (!candidate.get()) {
			RTC_LOG(WARNING) << "Can't parse received candidate message. "
				<< "SdpParseError was: " << error.description;
			return;
		}
		if (!peer_connection_->AddIceCandidate(candidate.get())) {
			RTC_LOG(WARNING) << "Failed to apply the received candidate";
			return;
		}
		RTC_LOG(INFO) << " Received candidate :" << message;
	}

}

void SendString(const char* str) {

	BYTE head[4] = { 0 };
	int length = strlen(str);
	head[0] = length >> 24 & 0xff;
	head[1] = length >> 16 & 0xff;
	head[2] = length >> 8 & 0xff;
	head[3] = length & 0xff;
	send(_socket, (const char*)head, 4, 0);
	send(_socket, (const char*)str, length, 0);
}



