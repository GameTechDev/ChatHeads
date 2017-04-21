/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __NETWORK_MSG_H__
#define __NETWORK_MSG_H__

#include "MessageIdentifiers.h" // ID_USER_PACKET_ENUM
#include <winnt.h> // LONGLONG	

enum NetworkMsg
{
	ID_GAME_MESSAGE_CLIENTID = ID_USER_PACKET_ENUM + 1, // server to client
	ID_GAME_MESSAGE_VIDEO_UPDATE // server to clients, client to server
};

// Definition for network messages passed between server and clients in Chat Heads
typedef void(*NetworkCallbackFn)(NetworkMsg eMsg, void *pThis, void *pMsg);


struct NetworkCallback
{
	void				*pThis;
	NetworkCallbackFn	cbFn;
};


struct NetMsgVideoUpdate
{
	struct vuheader
	{
		int				playerId;
		int				width;
		int				height;
		LONGLONG		timestamp;
		LONGLONG		duration;
	};

	vuheader		header;
	unsigned int	sizeBytes;
	byte			*pEncodedData;	
};

#endif