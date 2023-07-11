// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GTCharacterMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class GITTEST_API UGTCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	virtual void PerformMovement(float DeltaTime) override;
	virtual void PhysNavWalking(float deltaTime, int32 Iterations) override;
};
