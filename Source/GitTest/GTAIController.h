// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GTAIController.generated.h"

/**
 * 
 */
UCLASS()
class GITTEST_API AGTAIController : public AAIController
{
	GENERATED_BODY()

	virtual void Tick(float DeltaSeconds) override;
};