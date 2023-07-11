// Fill out your copyright notice in the Description page of Project Settings.


#include "GTAIController.h"

void AGTAIController::Tick(float DeltaSeconds)
{
	//Super::Tick(DeltaSeconds);

	UpdateControlRotation(DeltaSeconds, false);
}
