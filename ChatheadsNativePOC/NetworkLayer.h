#ifndef __NETWORK_LAYER__
#define __NETWORK_LAYER__

#include "RakNetTypes.h"
#include "NetworkMsg.h"

// NetworkLayer is a wrapper over Raknet that handles server creation, client connection and messaging between them.
// Only the message handling (receipt) is threaded (NetworkThread). Sending runs on the app thread.
class NetworkLayer
{
private:
	static DWORD WINAPI NetworkThread(LPVOID lpParam);
	static void AppendPlayerID(RakNet::BitStream& bs, int playerID);
	static void ProcessVideoUpdateMsg(NetworkLayer *pNet, RakNet::Packet *pPacket);

public:
	void Setup(bool bIsServer, const char* connectIPAddress);
	void Shutdown();
	void RegisterCallback(void *pThis, NetworkCallbackFn cb);
	bool SendVideoData(NetMsgVideoUpdate& msg, bool broadcast);
	void WakeNetworkThread() { SetEvent(mhDoWorkEvent); }

	bool InitComplete() const { return mbInitComplete; }
	bool IsServer() const { return mbIsServer; }
	bool IsClientConnectedToServer() const { return mbConnectedToServer;  }
	unsigned int NumClientsConnected() const { return mNumClients; }
	bool IsConnected() const { return (mbIsServer && mNumClients > 0) || (!mbIsServer && mbConnectedToServer); }
	int PlayerID() const { return mPlayerID; }
	const char* GetIPAddress() const;
	void SendInterval(int t) { mSendInterval = t; }
	bool CanSendData() const;
	uint64_t GetBytesSentInLastSecond() const { return mBytesSentInLastSecond; }
	uint64_t GetBytesRcvdInLastSecond() const { return mBytesRcvdInLastSecond; }
	
private:
	static void UpdateBytesSentInLastSecond(NetworkLayer *pNet);
	static void UpdateBytesRcvdInLastSecond(NetworkLayer *pNet);

public:
	static const size_t			cMaxPlayers = 4;
	static const unsigned short cPort = 23000;
private:
	bool						mbIsServer = false;
	bool						mbConnectedToServer = false;
	unsigned int				mNumClients = 0;
	bool						mbStayAlive = false;
	bool						mbInitComplete = false;
	char						mpIPAddress[16];
	int							mPlayerID = -1;
	ULONGLONG					mLastSendTick = 0;
	int							mSendInterval = 10;
	void						*mpThis = nullptr;
	RakNet::RakPeerInterface	*mpPeer = nullptr;
	RakNet::SystemAddress		mServerAddress;
	volatile uint64_t			mBytesSentInLastSecond;
	volatile uint64_t			mBytesRcvdInLastSecond;

	HANDLE						mhNetThread;
	HANDLE						mhDoWorkEvent;
	NetworkCallback				mCallback;
};


#endif // !__NETWORK_LAYER__