// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GTCharacterMovementComponent.h"
#include "GTPawnMovementComponent.h"
#include "GameFramework/Character.h"
#include "GTCharacterUnit.generated.h"


UCLASS()
class GITTEST_API AGTCharacterUnit : public APawn
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	UCapsuleComponent* CapsuleComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	USkeletalMeshComponent* SkeletalMeshComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	UGTPawnMovementComponent* PawnMovementComponent;
	
	AGTCharacterUnit();
protected:
	virtual void BeginPlay() override;
};
