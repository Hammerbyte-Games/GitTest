// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GTPawnMovementManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GTCharacterMovementComponent.generated.h"

UCLASS()
class GITTEST_API UGTCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UGTCharacterMovementComponent();
	virtual void PerformMovement(float DeltaTime) override;
	virtual void PhysNavWalking(float deltaTime, int32 Iterations) override;
	virtual void ControlledCharacterMove(const FVector& InputVector, float DeltaSeconds) override;
	virtual void ApplyRootMotionToVelocity(float deltaTime) override;
	virtual void CallMovementUpdateDelegate(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void UpdateMovement(float DeltaTime);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	AGTPawnMovementManager* PawnMovementManager;

};
