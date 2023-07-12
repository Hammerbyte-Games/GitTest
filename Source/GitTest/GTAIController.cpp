// Fill out your copyright notice in the Description page of Project Settings.


#include "GTAIController.h"

AGTAIController::AGTAIController()
{
	GetRootComponent()->bUseAttachParentBound = true;
	GetTransformComponent()->bUseAttachParentBound = true;
}

void AGTAIController::Tick(float DeltaSeconds)
{
	//Super::Tick(DeltaSeconds);

	UpdateControlRotation(DeltaSeconds, false);
	
}

void AGTAIController::BeginPlay()
{
	Super::BeginPlay();
	GetTransformComponent()->bComputeBoundsOnceForGame = true;
	GetTransformComponent()->bComputeFastLocalBounds = true;
}
