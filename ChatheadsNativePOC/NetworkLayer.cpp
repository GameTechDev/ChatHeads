/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#include "NetworkLayer.h"
#include "RakPeerInterface.h"
#include "BitStream.h"
#include "RakNetStatistics.h"

#include "VTuneScopedTask.h"
#include "SetThreadName.h"
#include "CPUTOSServices.h" // CPUT logging

using namespace CPUTFileSystem;
extern CPUTLog Log;
extern __itt_domain* g_pDomain;

void NetworkLayer::Setup(	bool bIsServer, const char* connectIPAddress)
{
	VTUNE_TASK(g_pDomain, "Network Setup");

	if (bIsServer)
		mPlayerID = 0;	// server id is 0, client receives its ID from server

	mbIsServer = bIsServer;
	strncpy(mpIPAddress, connectIPAddress, sizeof(mpIPAddress)/sizeof(char));
	mLastSendTick = GetTickCount64();
	mpPeer = RakNet::RakPeerInterface::GetInstance();

	// Start server/connect to server
	if (mbIsServer)
	{
		Log.Log(LOG_INFO, "Starting the server.\n");

		RakNet::SocketDescriptor sd(cPort, 0);
		mpPeer->Startup(cMaxPlayers, &sd, 1);
		mbIsServer = true;
		// We need to let the server accept incoming connections from the clients
		mpPeer->SetMaximumIncomingConnections(cMaxPlayers);
	}
	else
	{
		Log.Log(LOG_INFO, "Starting the client.\n");

		RakNet::SocketDescriptor sd;
		mpPeer->Startup(1, &sd, 1);
		mbIsServer = false;
		mpPeer->Connect(mpIPAddress, //host
			cPort, // port
			0, // password
			0, // password length
			0, // public key
			0, // conn socket index
			6, // connection attempt count
			1000, // time between attempts in millisec
			1000 * 120); // time out in millisec;
	}

	mbInitComplete = true;

	// Create event, set bool and then create the network thread
	mbStayAlive = true;
	mhDoWorkEvent = CreateEvent(NULL, true, false, L"NetworkDoWork");
	DWORD ThreadID = 0;
	mhNetThread = CreateThread(NULL, 0, NetworkThread, (void*)this, 0, &ThreadID);
	SetThreadName(ThreadID, "NetworkThread");	
}


void NetworkLayer::Shutdown()
{
	mbStayAlive = false;

	if (!mbInitComplete)
		return;

	WakeNetworkThread(); // if the thread is waiting on the event, setting the bool won't help. this will make the thread get the event and then stop.
	WaitForSingleObject(mhNetThread, INFINITE);
	CloseHandle(mhNetThread);
	CloseHandle(mhDoWorkEvent);

	mbConnectedToServer = false;
	if (mpPeer)
	{
		mpPeer->Shutdown(5 /*ms : wait for this much time to send/receive disconnect notification*/);
		delete mpPeer;
	}

	Log.Log(LOG_INFO, "Network thread has been shutdown");
}


void NetworkLayer::RegisterCallback(void *pThis, NetworkCallbackFn cb)
{
	mCallback.pThis = pThis;
	mCallback.cbFn = cb;
}


bool NetworkLayer::CanSendData() const
{
	ULONGLONG curClockTick = GetTickCount64();
	if (curClockTick - mLastSendTick < mSendInterval)
		return false;

	return true;
}


const char* NetworkLayer::GetIPAddress() const
{
	return mpPeer->GetSystemAddressFromGuid(mpPeer->GetMyGUID()).ToString(false /* don't need port info */);
}


// Send encoded video data (happens on the app thread)
bool NetworkLayer::SendVideoData(NetMsgVideoUpdate& msg, bool broadcast)
{
	VTUNE_TASK(g_pDomain, "SendVideoData");

	if (!mbInitComplete)
		return false;
	if (mbIsServer && mNumClients == 0 || (!mbIsServer && !mbConnectedToServer))
		return false;

	bool bSentData = false;

	RakNet::BitStream bsOut;

	bsOut.Write((RakNet::MessageID)ID_GAME_MESSAGE_VIDEO_UPDATE);	
	bsOut.Write(reinterpret_cast<char*> (&msg.header), sizeof(msg.header));
	bsOut.Write(reinterpret_cast<char*> (msg.pEncodedData), msg.sizeBytes);

	NetMsgVideoUpdate::vuheader& header = msg.header;
	//Log.Log(LOG_INFO, "SEND: Message %d width %d height timestamp %lld, duration %lld, size %lu \n", header.width, header.height, header.timestamp, header.duration, msg.sizeBytes);

	uint32_t retVal;
	if (broadcast)
		retVal = mpPeer->Send(&bsOut, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0, RakNet::UNASSIGNED_RAKNET_GUID, true);
	else
		retVal = mpPeer->Send(&bsOut, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0, mServerAddress, false);

	mLastSendTick = GetTickCount64();

	bSentData = (retVal != 0);	


	return bSentData;
}


// Note: This should be called from the n/w thread ONLY.
void NetworkLayer::UpdateBytesSentInLastSecond(NetworkLayer *pNet)
{
	uint64_t numBytes = 0;

	if (pNet->mbIsServer)
	{
		using namespace RakNet;

		DataStructures::List<SystemAddress> addresses;
		DataStructures::List<RakNetGUID> guids;
		DataStructures::List<RakNetStatistics> statistics;

		pNet->mpPeer->GetStatisticsList(addresses, guids, statistics);

		unsigned int numSystems = statistics.Size();

		for (unsigned int ii = 0; ii < numSystems; ii++)
			numBytes += statistics[ii].valueOverLastSecond[RakNet::USER_MESSAGE_BYTES_SENT];
	}
	else
	{	
		RakNet::RakNetStatistics *pStats = pNet->mpPeer->GetStatistics(pNet->mServerAddress);
		numBytes = pStats->valueOverLastSecond[RakNet::USER_MESSAGE_BYTES_SENT];
	}

	pNet->mBytesSentInLastSecond = numBytes;
}


// Note: This should be called from the n/w thread ONLY.
void NetworkLayer::UpdateBytesRcvdInLastSecond(NetworkLayer *pNet)
{
	uint64_t numBytes = 0;

	if (pNet->mbIsServer)
	{
		using namespace RakNet;

		DataStructures::List<SystemAddress> addresses;
		DataStructures::List<RakNetGUID> guids;
		DataStructures::List<RakNetStatistics> statistics;

		pNet->mpPeer->GetStatisticsList(addresses, guids, statistics);

		unsigned int numSystems = statistics.Size();

		for (unsigned int ii = 0; ii < numSystems; ii++)
			numBytes += statistics[ii].valueOverLastSecond[RakNet::USER_MESSAGE_BYTES_RECEIVED_PROCESSED];
	}
	else
	{
		RakNet::RakNetStatistics *pStats = pNet->mpPeer->GetStatistics(pNet->mServerAddress);
		numBytes = pStats->valueOverLastSecond[RakNet::USER_MESSAGE_BYTES_RECEIVED_PROCESSED];
	}
	
	pNet->mBytesRcvdInLastSecond = numBytes;
}


/******************************************************** static functions **************************************************************/
// This runs in its own thread. 
// Takes care of server/client creation/connection and processing messages received
DWORD WINAPI NetworkLayer::NetworkThread(LPVOID lpParam)
{
	NetworkLayer *pNet = (NetworkLayer*)lpParam;

	// Message handling loop
	while (pNet->mbStayAlive)
	{
		// The app wakes up the network thread during Update()
		WaitForSingleObject(pNet->mhDoWorkEvent, INFINITE);
		ResetEvent(pNet->mhDoWorkEvent);

		// Update stats
		UpdateBytesRcvdInLastSecond(pNet);
		UpdateBytesSentInLastSecond(pNet);


		RakNet::Packet *packet;

		for (packet = pNet->mpPeer->Receive(); packet; pNet->mpPeer->DeallocatePacket(packet), packet = pNet->mpPeer->Receive())
		{
			switch (packet->data[0])
			{
			/*********************************************** Raknet msgs *********************************************************************/
			case ID_REMOTE_DISCONNECTION_NOTIFICATION:
				Log.Log(LOG_INFO, "ID_REMOTE_DISCONNECTION_NOTIFICATION: Another client has disconnected.\n");
				//CPUTOSServices::OpenMessageBox("", "Another client has disconnected. You stay strong..");
				break;

			case ID_REMOTE_CONNECTION_LOST:
				Log.Log(LOG_INFO, "ID_REMOTE_CONNECTION_LOST: Another client has lost the connection.\n");
				//CPUTOSServices::OpenMessageBox(":-(", "Another client has lost its connection. You stay strong..");

				// TODO change numclinets?
				break;

			case ID_REMOTE_NEW_INCOMING_CONNECTION:
				Log.Log(LOG_INFO, "ID_REMOTE_NEW_INCOMING_CONNECTION: Another client has connected.\n");
				//CPUTOSServices::OpenMessageBox(":-)", "Another client has connected");
				break;

			case ID_NEW_INCOMING_CONNECTION:
			{
				Log.Log(LOG_INFO, "ID_NEW_INCOMING_CONNECTION: A connection is incoming.\n");
				pNet->mNumClients++;

				// Send the client a message with its client id
				RakNet::BitStream bsOut;
				bsOut.Write((RakNet::MessageID)ID_GAME_MESSAGE_CLIENTID);
				int playerID = pNet->mNumClients;
				AppendPlayerID(bsOut, playerID);

				pNet->mpPeer->Send(&bsOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

				//CPUTOSServices::OpenMessageBox("", "You have company! A client has connected..");

			}
			break;

			case ID_CONNECTION_REQUEST_ACCEPTED:
			{
				Log.Log(LOG_INFO, "ID_CONNECTION_REQUEST_ACCEPTED: Our connection request has been accepted.\n");
				pNet->mServerAddress = packet->systemAddress;
			}
			break;

			case ID_CONNECTION_ATTEMPT_FAILED:
			{
				Log.Log(LOG_INFO, "ID_CONNECTION_ATTEMPT_FAILED: Our connection request failed\n");
				//CPUTOSServices::OpenMessageBox("Error", "Could not connect to server. The show must still go on..");
				// pNet->mbConnectedToServer will be false here.
			}
			break;

			case ID_NO_FREE_INCOMING_CONNECTIONS:
				Log.Log(LOG_INFO, "ID_NO_FREE_INCOMING_CONNECTIONS: The server is full.\n");
				break;

			case ID_DISCONNECTION_NOTIFICATION:
				if (pNet->mbIsServer){
					Log.Log(LOG_INFO, "ID_DISCONNECTION_NOTIFICATION: A client has disconnected.\n");
					pNet->mNumClients--;

					//CPUTOSServices::OpenMessageBox(":-(", "A client has disconnected.");
				}
				else {
					//CPUTOSServices::OpenMessageBox(":-(", "The server doesn't want to serve us anymore.");
					Log.Log(LOG_INFO, "ID_DISCONNECTION_NOTIFICATION: We have been disconnected.\n");
				}
				break;

			case ID_CONNECTION_LOST:
				if (pNet->mbIsServer){
					Log.Log(LOG_INFO, "ID_CONNECTION_LOST: A client lost the connection.\n");
					pNet->mNumClients--;
					//CPUTOSServices::OpenMessageBox("", "A client has lost its connection.");
				}
				else {
					Log.Log(LOG_INFO, "ID_CONNECTION_LOST: Connection lost.\n");
				}
				break;

				
			/*********************************************** Chat Heads specific msgs**********************************************************/
			case ID_GAME_MESSAGE_VIDEO_UPDATE:
			{
				pNet->ProcessVideoUpdateMsg(pNet, packet);
			}
			break;

			case ID_GAME_MESSAGE_CLIENTID:
			{
				RakNet::BitStream bsIn(packet->data, packet->length, false);

				bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
				bsIn.Read(reinterpret_cast<char*>(&pNet->mPlayerID), sizeof(pNet->mPlayerID));
				Log.Log(LOG_INFO, "My player id is %d", pNet->mPlayerID);
				
				pNet->mbConnectedToServer = true;
			}
			break;

			default:
				Log.Log(LOG_INFO, "Message with identifier %i has arrived.\n", packet->data[0]);
				break;
			}
		}
	}

	return 0;
}


// Note: Incoming messages are called by NetworkThread, and hence execute on the n/w thread (and not the app thread)
//		 So, they're all declared as static fns.
void NetworkLayer::ProcessVideoUpdateMsg(NetworkLayer *pNet, RakNet::Packet *pPacket)
{
	VTUNE_TASK(g_pDomain, "ProcessVideoUpdateMsg");

	RakNet::BitStream bsIn(pPacket->data, pPacket->length, false);

	NetMsgVideoUpdate msg;

	// Fill struct with stream data
	bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
	bsIn.Read(reinterpret_cast<char*> (&msg.header), sizeof(msg.header));	

	size_t headerSize = sizeof(RakNet::MessageID) + sizeof(msg.header);
	size_t payloadSizeBytes = pPacket->length - headerSize;

	msg.pEncodedData = pPacket->data + headerSize; // point to the right data in the bitstream
	msg.sizeBytes = pPacket->length - (unsigned int) headerSize; // how big is the data?

	// Call the registered callback
	NetworkCallbackFn cbFn = pNet->mCallback.cbFn;
	void *pCbThis = pNet->mCallback.pThis;
	cbFn(ID_GAME_MESSAGE_VIDEO_UPDATE, pCbThis, static_cast<void*>(&msg));

	if (pNet->mbIsServer)
		uint32_t retVal = pNet->mpPeer->Send(&bsIn, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0, pPacket->systemAddress, true);
}


void NetworkLayer::AppendPlayerID(RakNet::BitStream& bs, int playerID)
{
	bs.Write(reinterpret_cast<char*>(&playerID), sizeof(playerID));
}