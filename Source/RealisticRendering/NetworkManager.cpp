// Fill out your copyright notice in the Description page of Project Settings.

#include "RealisticRendering.h"
#include "NetworkManager.h"
#include "Networking.h"
#include <string>



bool FSocketMessageHeader::WrapAndSendPayload(const TArray<uint8>& Payload, const FSimpleAbstractSocket& Socket)
{
	FSocketMessageHeader Header(Socket, Payload);

	FBufferArchive Ar;
	Ar << Header.Magic;
	Ar << Header.PayloadSize;
	Ar.Append(Payload);

	if (!Socket.Send(Ar.GetData(), Ar.Num()))

	{
		UE_LOG(LogTemp, Error, TEXT("Unable to send."));
		return false;
	}
	return true;
}


bool FSocketMessageHeader::ReceivePayload(FArrayReader& OutPayload, const FSimpleAbstractSocket& Socket)
{
	TArray<uint8> HeaderBytes;
	int32 Size = sizeof(FSocketMessageHeader);
	HeaderBytes.AddZeroed(Size);
	
	if (!Socket.Receive(HeaderBytes.GetData(), Size)) 
		// This is not compatible with a non-blocking socket
		// in non-blocking mode, recv will return -1 if no data available.
	{
		// false here can mean -1 or 0.
		UE_LOG(LogTemp, Error, TEXT("Unable to read header, Socket disconnected."));
		return false;
	}

	FMemoryReader Reader(HeaderBytes);
	uint32 Magic;
	Reader << Magic;

	if (Magic != Socket.GetMagic())
	{
		UE_LOG(LogTemp, Error, TEXT("Bad network header magic"));
		return false;
	}

	uint32 PayloadSize;
	Reader << PayloadSize; // Accept zero size payload
	if (!PayloadSize)
	{
		UE_LOG(LogTemp, Error, TEXT("Empty payload"));
		return false;
	}

	int32 PayloadOffset = OutPayload.AddUninitialized(PayloadSize);
	OutPayload.Seek(PayloadOffset);
	if (!Socket.Receive(OutPayload.GetData() + PayloadOffset, PayloadSize))
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to read full payload"));
		return false;
	}

	// Skip CRC checking in FNFSMessageHeader
	return true;
}


// TODO: Wrap these two functions into a class
FString StringFromBinaryArray(const TArray<uint8>& BinaryArray)
{
	std::string cstr(reinterpret_cast<const char*>(BinaryArray.GetData()), BinaryArray.Num());
	return FString(cstr.c_str());
}

void BinaryArrayFromString(const FString& Message, TArray<uint8>& OutBinaryArray)
{
	const TArray<TCHAR>& CharArray = Message.GetCharArray();
	OutBinaryArray.Append(CharArray); // TODO: Need to convert TCHAR to uint8. Fix this later
}

/* Provide a dummy echo service to echo received data back for development purpose */
bool UNetworkManager::StartEchoService(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint)
{
	if (!this->ConnectionSocket) // Only maintain one active connection, So just reuse the TCPListener thread.
	{
		UE_LOG(LogTemp, Warning, TEXT("New client connected from %s"), *ClientEndpoint.ToString());
		// ClientSocket->SetNonBlocking(false); // When this in blocking state, I can not use this socket to send message back
		ConnectionSocket = new FSimpleAbstractSocket_FSocket(ClientSocket);
		
		// Listening data here or start a new thread for data?
		// Reuse the TCP Listener thread for getting data, only support one connection
		uint32 BufferSize = 1024;
		int32 Read = 0;
		TArray<uint8> ReceivedData;
		ReceivedData.SetNumZeroed(BufferSize);
		while (1)
		{ 
			// Easier to use raw FSocket here, need to detect remote socket disconnection
			bool RecvStatus = ClientSocket->Recv(ReceivedData.GetData(), ReceivedData.Num(), Read);
			
			// if (!RecvStatus) // The connection is broken 
			if (Read == 0) // RecvStatus == true if Read >= 0, this is used to determine client disconnection
				// -1 means no data, 0 means disconnected
			{ 
				ConnectionSocket = NULL; // Use this to determine whether client is connected
				return false;
			}
			int32 Sent;
			ClientSocket->Send(ReceivedData.GetData(), Read, Sent); // Echo the message back
			check(Read == Sent);
		}
		return true;
	}
	return false;
}


bool UNetworkManager::StartMessageService(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint)
{
	if (!this->ConnectionSocket)
	{
		UE_LOG(LogTemp, Warning, TEXT("New client connected from %s"), *ClientEndpoint.ToString());
		ClientSocket->SetNonBlocking(false); // When this in blocking state, I can not use this socket to send message back
		// ConnectionSocket = ClientSocket;

		while (1)
		{
			FArrayReader ArrayReader;
			if (!FSocketMessageHeader::ReceivePayload(ArrayReader, *ConnectionSocket)) 
				// Wait forever until got a message, or return false when error happened
			{
				this->ConnectionSocket = NULL;
				break; // Remote socket disconnected
			}

			FString Message = StringFromBinaryArray(ArrayReader);
			UE_LOG(LogTemp, Warning, TEXT("Receive message %s"), *Message);
		}
	}
	return false; // Already have a connection
}

bool UNetworkManager::Connected(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint)
{
	bool ServiceStatus = false;
	// ServiceStatus = StartEchoService(ClientSocket, ClientEndpoint);
	ServiceStatus = StartMessageService(ClientSocket, ClientEndpoint);
	return ServiceStatus;
	// This is a blocking service, if need to support multiple connections, consider start a new thread here.
}

void UNetworkManager::Start()
{
	FIPv4Address IPAddress = FIPv4Address(0, 0, 0, 0);
	int32 PortNum = 9000; // Make this configuable
	FIPv4Endpoint Endpoint(IPAddress, PortNum);

	TcpListener = new FTcpListener(Endpoint); // This will be released after start
	// In FSocket, when a FSocket is set as reusable, it means SO_REUSEADDR, not SO_REUSEPORT.  see SocketsBSD.cpp
	TcpListener->OnConnectionAccepted().BindUObject(this, &UNetworkManager::Connected);
	if (TcpListener->Init())
	{
		UE_LOG(LogTemp, Warning, TEXT("Start listening on %d"), PortNum);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Can not start listening on port %d"), PortNum);
	}


	// Start a timer to check data regularly
}

bool UNetworkManager::SendMessage(const FString& Message)
{
	// TCHAR* SerializedChar = Message.GetCharArray().GetData();
	if (ConnectionSocket)
	{
		TArray<uint8> Payload;
		BinaryArrayFromString(Message, Payload);

		FSocketMessageHeader::WrapAndSendPayload(Payload, *ConnectionSocket);
		return true;
	}
	return false;
}

UNetworkManager::~UNetworkManager()
{
	delete TcpListener;
}