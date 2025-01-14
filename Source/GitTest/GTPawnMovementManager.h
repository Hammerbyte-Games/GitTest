// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GTPawnMovementManager.generated.h"

class UGTCharacterMovementComponent;
UCLASS()
class GITTEST_API AGTPawnMovementManager : public AActor
{
	GENERATED_BODY()
	
public:

	UPROPERTY(BlueprintReadWrite)
	TArray<UGTCharacterMovementComponent*> MovementComponents;
	
	AGTPawnMovementManager();
	virtual void Tick(float DeltaTime) override;
protected:
	virtual void BeginPlay() override;
	

};
