// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Object.h"
#include "Networking.h"
#include "NetworkMessage.h"
#include "NetworkManager.generated.h"

/* a simplified version from FNFSMessageHeader, without CRC check */
class FSocketMessageHeader
{
	/* Error checking */
	uint32 Magic = 0;

	/* Payload Size */
	uint32 PayloadSize = 0;

public:
	FSocketMessageHeader(const FSimpleAbstractSocket& InSocket, const TArray<uint8>& Payload)
	{
		PayloadSize = Payload.Num();  // What if PayloadSize is 0
		Magic = InSocket.GetMagic();
	}

	static bool WrapAndSendPayload(const TArray<uint8>& Payload, const FSimpleAbstractSocket& Socket);
	static bool ReceivePayload(FArrayReader& OutPayload, const FSimpleAbstractSocket& Socket);
};

/**
 *
 */
DECLARE_EVENT_OneParam(UNetworkManager, FReceivedEvent, FString)
// TODO: Consider add a pointer to NetworkManager itself
// TODO: Add connected event
UCLASS()
class REALISTICRENDERING_API UNetworkManager : public UObject
// NetworkManager needs to be an UObject, because TimerManager can only support method of an UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 PortNum = 9000;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsConnected = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString ListenIP = "0.0.0.0"; // TODO: this is hard coded right now

	void Start();
	bool SendMessage(const FString& Message);
private:
	bool Connected(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint);
	FSimpleAbstractSocket* ConnectionSocket = NULL; // FSimpleAbstractSocket's receive is hard to use for non-blocking mode
	// FSocket* ConnectionSocket;
	FTcpListener* TcpListener;
	~UNetworkManager();

	bool StartEchoService(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint);
	bool StartMessageService(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint);
};
