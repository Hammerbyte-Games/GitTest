// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GTNewCharacter.generated.h"

UCLASS()
class GITTEST_API AGTNewCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGTNewCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

};
