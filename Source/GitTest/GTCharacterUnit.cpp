// Fill out your copyright notice in the Description page of Project Settings.


#include "GTCharacterUnit.h"

#include "GTCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Net/UnrealNetwork.h"


AGTCharacterUnit::AGTCharacterUnit()
{
	PrimaryActorTick.bCanEverTick = false;

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("Capsule Component");
	SetRootComponent(CapsuleComponent);
	
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("Skeletal Mesh");
	SkeletalMeshComponent->SetupAttachment(CapsuleComponent);
	SkeletalMeshComponent->PrimaryComponentTick.bCanEverTick = false;
	SkeletalMeshComponent->bUseAttachParentBound = true;
	
	PawnMovementComponent = CreateDefaultSubobject<UGTPawnMovementComponent>("Pawn");
}

void AGTCharacterUnit::BeginPlay()
{
	Super::BeginPlay();
	SkeletalMeshComponent->bComputeBoundsOnceForGame = true;
	SkeletalMeshComponent->bComputeFastLocalBounds = true;
}




