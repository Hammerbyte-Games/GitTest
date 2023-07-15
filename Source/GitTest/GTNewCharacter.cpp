// Fill out your copyright notice in the Description page of Project Settings.


#include "GTNewCharacter.h"
#include "GTCharacterMovementComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"

AGTNewCharacter::AGTNewCharacter(const FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer.SetDefaultSubobjectClass<UGTCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
 	PrimaryActorTick.bCanEverTick = false;
	
	GetMesh()->PrimaryComponentTick.bCanEverTick = false;
	GetMesh()->bUseAttachParentBound = true;
	
	GetCharacterMovement()->PrimaryComponentTick.bCanEverTick = false;
}

void AGTNewCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	GetMesh()->bComputeBoundsOnceForGame = true;
	GetMesh()->bComputeFastLocalBounds = true;
	GetCapsuleComponent()->bComputeFastLocalBounds = true;
	//GetArrowComponent()->bComputeBoundsOnceForGame = true;
	//GetArrowComponent()->bComputeFastLocalBounds = true;
	
	
	
	GetRootComponent()->SetShouldUpdatePhysicsVolume(false);
}
