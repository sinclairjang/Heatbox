// Fill out your copyright notice in the Description page of Project Settings.


#include "FireBlob.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

// Sets default values
AFireBlob::AFireBlob()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	//Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root Component"));
	//RootComponent = Root;

	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>("Fire VFX Component");
	RootComponent = NiagaraComponent;
	//NiagaraComponent->SetupAttachment(Root);

	IsPending = 0;
	MarkForDestroy = 0;
}

// Called when the game starts or when spawned
void AFireBlob::BeginPlay()
{
	Super::BeginPlay();
	
	BurnOutDelay = FMath::RandRange(2.f, 4.f);
	
}

// Called every frame
void AFireBlob::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AFireBlob::PauseFireEffect()
{
	NiagaraComponent->SetPaused(true);
}

float AFireBlob::GetTimeToBurnOut() const
{
	return BurnOutDelay;
}

float& AFireBlob::SetTimeToBurnOut()
{
	return BurnOutDelay;
}

void AFireBlob::SetFireSize(float Size)
{
	NiagaraComponent->SetRelativeScale3D(FVector(FVector2D(Size), 1.0));
}

uint8 AFireBlob::GetIsPending()
{
	return IsPending;
}

uint8 AFireBlob::GetMarkForDestroy()
{
	return MarkForDestroy;
}
