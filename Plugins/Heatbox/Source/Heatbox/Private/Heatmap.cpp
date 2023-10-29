// Fill out your copyright notice in the Description page of Project Settings.

#include "Heatmap.h"
#include "Heatbox.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "PointEstimator.h"
#include "FireBlob.h"
#include "Kismet/KismetSystemLibrary.h"


#include <string>
#include <iostream>
#include <stdlib.h>
using namespace std;

#if WITH_EDITOR
#define TEMPERATURE_LOG_HEADER() \
	if (bShowTemperatureLog) { \
		UE_LOG(TemperatureLog, Warning, TEXT("")); \
		UE_LOG(TemperatureLog, Warning, TEXT("%17s%22s%30s%17s"), *FString("Box Owner"), *FString("Map Index"), *FString("Current Temperature"), *FString("Ignition Point")); \
	}
#define TEMPERATURE_LOG_BODY(index) \
	if (bShowTemperatureLog) { \
		UE_LOG(TemperatureLog, Log, TEXT("%25s%25s%14.2f%23.2f"), *FString(BoxOwner->GetActorLabel()), *index.ToString(), HeatBoxInfo.CurrTemperature, HeatBoxInfo.IgnitionPoint); \
	}

#else
#define TEMPERATURE_LOG_HEADER()
#define TEMPERATURE_LOG_BODY(index)
#endif

#if WITH_EDITOR
#define MISC_LOG_HEADER() \
	if (bShowMiscLog) { \
		UE_LOG(MiscLog, Warning, TEXT("")); \
		UE_LOG(MiscLog, Warning, TEXT("%18s%21s%26s%21s%14s"), *FString("Box Owner"), *FString("Map Index"), *FString("HeatAbsorbRate"), *FString("HeatEmitRate"), *FString("FuelCount")); \
	}
#define MISC_LOG_BODY(index) \
	if (bShowMiscLog) { \
		UE_LOG(MiscLog, Log, TEXT("%25s%25s%14.2f%23.2f%17.2f"), *FString(BoxOwner->GetActorLabel()), *index.ToString(), HeatBoxInfo.HeatAbsorbRate, HeatBoxInfo.HeatEmitRate, HeatBoxInfo.FuelCount); \
	}

#else
#define MISC_LOG_HEADER()
#define MISC_LOG_BODY(index)
#endif

#if WITH_EDITOR
#define HEATDMG_LOG_HEADER() \
	if (bHeatDamageLog) { \
		UE_LOG(HeatDamageLog, Warning, TEXT("")); \
		UE_LOG(HeatDamageLog, Warning, TEXT("%16s%23s%31s%24s"), *FString("Box Owner"), *FString("Map Index"), *FString("Heat Damage Applied"), *FString("Heat Damage Received")); \
	}
#define HEATDMG_LOG_BODY(index) \
	if (bHeatDamageLog) { \
		UE_LOG(HeatDamageLog, Log, TEXT("%25s%25s%14.2f%23.2f"), *FString(BoxOwner->GetActorLabel()), *index.ToString(), HeatBoxInfo.HeatDamageApplied, HeatBoxInfo.HeatDamageReceived); \
	}

#else
#define TEMPERATURE_LOG_HEADER()
#define TEMPERATURE_LOG_BODY(index)
#endif

float Sigma = 5.6703e-8; // The Stefan-Boltzmann Constant [W/m2K4]

FName Flammable = "Flammable";
FName Burning = "Burning";
FName BurntOut = "BurntOut";

FFireInBox::~FFireInBox() 
{
	if(FireEffect) {
		FireEffect->Destroy();
	}
 
}

void FFireInBox::SetFireSize(float Size)
{
	FireEffect->SetFireSize(Size);
}

void FHeatBoxInfo::ReceiveHeat(float HeatEnergy, float UpdateInterval)
{
	HeatDamageReceived = (HeatEnergy * UpdateInterval) * HeatAbsorbRate;
	CurrTemperature = CurrTemperature + HeatDamageReceived;
	CurrTemperature = (CurrTemperature < 20.f) ? 20.f : (CurrTemperature > MaxTemperature) ? MaxTemperature : CurrTemperature;
}

float FHeatBoxInfo::RadiateHeat()
{
	HeatDamageApplied = (Sigma * (pow(MaxTemperature + 273.15f, 4.f) - pow((MaxTemperature - CurrTemperature) + 273.15f, 4.f)) * (RadiationArea / 1e+4) * HeatEmitRate) / 1000.f;
	return HeatDamageApplied;
}

bool FHeatBoxInfo::IsBurntOut() const
{
	return FuelCount == 0;
}

bool FHeatBoxInfo::IsIgnitionStarting() const
{
	return CurrTemperature >= IgnitionPoint;
}

bool FHeatBoxInfo::IsExtinguished() const
{
	return CurrTemperature < IgnitionPoint;
}

FVector FHeatBoxInfo::ClampFireSize(float EstimatedArea) const
{
	EstimatedArea /= 1e+4;
	EstimatedArea = (EstimatedArea < MinFireSize) ? MinFireSize : (EstimatedArea > MaxFireSize) ? MaxFireSize : EstimatedArea;

	return FVector(FVector2D(EstimatedArea), 1.f);
}

// Sets default values
AHeatmap::AHeatmap()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	FireEffectClass = AFireBlob::StaticClass();

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root Component"));
	RootComponent = Root;

	NumDepthCells = 8;
	NumWidthCells = 8;
	NumHeightCells = 4;
	UnitSpacings = { 100.f, 100.f, 100.f };
	HeatTransferRate = 0.16f;
	UpdateInterval = 1.f;
#if WITH_EDITOR
	bShowTemperatureLog = false;
	bShowMiscLog = false;
	ShowHeatMap = VisualVerbosity::Visual_None;
#endif
	bSimHasBegun = false;

	GraphViz = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Graphics Visualizer Component"));
	GraphViz->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CubeMesh = CubeFinder.Object;

	GraphViz->SetStaticMesh(CubeMesh);
	GraphViz->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GraphViz->SetGenerateOverlapEvents(false);
	GraphViz->NumCustomDataFloats = 4;

	static ConstructorHelpers::FObjectFinder<UMaterial> VizColorFinder(TEXT("/Script/Engine.Material'/Heatbox/Material/M_Viz.M_Viz'"));
	VizColor = VizColorFinder.Object;
}

// Called when the game starts or when spawned
void AHeatmap::BeginPlay()
{
	Super::BeginPlay();
	Apply();
	bSimHasBegun = true;
}

void AHeatmap::RegisterNewBoxOwners()
{
	RegisterNewBoxOwners_Impl(FlamBoxInstsOf, BurnBoxInstsOf, HeatBoxAt);
}

void AHeatmap::ClearRegistry()
{
	FlamBoxInstsOf.Reset();
	BurnBoxInstsOf.Reset();
	HeatBoxAt.Reset();
	RegisteredBowOwners.Reset();
}

void AHeatmap::StartSim()
{
	if (SimulationStage == SimStage::None) {
		GetWorldTimerManager().SetTimer(HeatmapTimer, this, &AHeatmap::RouteUpdateHeatmap, UpdateInterval, true, 0);
		SimulationStage = SimStage::Playing;
	}
	
	else {
		UE_LOG(Firebox, Warning, TEXT("Simulation has already begun"));
		return;
	}
}

void AHeatmap::PauseSim()
{
	if (SimulationStage == SimStage::Playing) {
		GetWorldTimerManager().PauseTimer(HeatmapTimer);

		TArray<TSharedPtr<FFireInBox>> FiresToPause;
		GoingFires.GenerateValueArray(FiresToPause);
		
		for (auto FireToPause : FiresToPause) {
			FireToPause->FireEffect->PauseFireEffect();
		}

		for (auto OrphanedFire : OrphanedFires) {
			OrphanedFire->FireEffect->PauseFireEffect();
		}

		SimulationStage = SimStage::Paused;
	}

	else {
		UE_LOG(Firebox, Warning, TEXT("Simulation is not playing"));
		return;
	}
}

void AHeatmap::ResumeSim()
{
	if (SimulationStage == SimStage::Paused) {
		GetWorldTimerManager().UnPauseTimer(HeatmapTimer);
		SimulationStage = SimStage::Playing;
	}

	else {
		UE_LOG(Firebox, Warning, TEXT("Simulation has not paused"));
		return;
	}
}

void AHeatmap::EndSim()
{
	if ((uint8)SimulationStage > (uint8)SimStage::None) {
		// 초기화
		GetWorldTimerManager().ClearTimer(HeatmapTimer);
		HeatGenField.Init(0.f, HeatGenField.Num());
		GoingFires.Reset();
		OrphanedFires.Reset();

		SimulationStage = SimStage::None;
	}

	else {
		UE_LOG(Firebox, Warning, TEXT("Simulation has not begun"));
		return;
	}
}

void AHeatmap::GetHBParam(AActor* BoxOwner, SetHeatBoxFuncParamType ParamType, float& ParamValue)
{
	switch (ParamType)
	{
	case (SetHeatBoxFuncParamType::MaxTemperature):
		if (RegisteredBowOwners.Contains(BoxOwner)) {

			if (FlamBoxInstsOf.Contains(BoxOwner)) {
				if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = FlamBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].MaxTemperature;

					}
				}

			if (BurnBoxInstsOf.Contains(BoxOwner)) {
				if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = BurnBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].MaxTemperature;
					
				}
			}
		}

		else {
			ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			
		}
		break;

	case (SetHeatBoxFuncParamType::IgnitionPoint):
		if (RegisteredBowOwners.Contains(BoxOwner)) {

			if (FlamBoxInstsOf.Contains(BoxOwner)) {
				if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = FlamBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].IgnitionPoint;
					
				}
			}
			
			if (BurnBoxInstsOf.Contains(BoxOwner)) {
				if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = FlamBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].IgnitionPoint;
					
				}
			}
		}

		else {
			ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			
		}
		break;

	case (SetHeatBoxFuncParamType::HeatAbsorbRate):
		if (RegisteredBowOwners.Contains(BoxOwner)) {

			if (FlamBoxInstsOf.Contains(BoxOwner)) {
				if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = FlamBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].HeatAbsorbRate;
					
				}
			}

			if (BurnBoxInstsOf.Contains(BoxOwner)) {
				if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = FlamBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].HeatAbsorbRate;
					
				}
			}
		}

		else {
			ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			
		}
		break;

	case (SetHeatBoxFuncParamType::HeatEmitRate):
		if (RegisteredBowOwners.Contains(BoxOwner)) {

			if (FlamBoxInstsOf.Contains(BoxOwner)) {
				if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = FlamBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].HeatEmitRate;
					
				}
			}

			if (BurnBoxInstsOf.Contains(BoxOwner)) {
				if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = FlamBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].HeatEmitRate;
					
				}
			}
		}

		else {
			ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			
		}
		break;

	case (SetHeatBoxFuncParamType::FuelCount):
		if (RegisteredBowOwners.Contains(BoxOwner)) {

			if (FlamBoxInstsOf.Contains(BoxOwner)) {
				if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = FlamBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].FuelCount;
					
				}
			}

			if (BurnBoxInstsOf.Contains(BoxOwner)) {
				if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = FlamBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].FuelCount;
					
				}
			}
		}

		else {
			ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			
		}
		break;

	case (SetHeatBoxFuncParamType::MaxFireSize):
		if (RegisteredBowOwners.Contains(BoxOwner)) {

			if (FlamBoxInstsOf.Contains(BoxOwner)) {
				if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = FlamBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].MaxFireSize;
					
				}
			}

			if (BurnBoxInstsOf.Contains(BoxOwner)) {
				if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = FlamBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].MaxFireSize;
					
				}
			}
		}

		else {
			ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			
		}
		break;

	case (SetHeatBoxFuncParamType::MinFireSize):
		if (RegisteredBowOwners.Contains(BoxOwner)) {

			if (FlamBoxInstsOf.Contains(BoxOwner)) {
				if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = FlamBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].MinFireSize;
					
				}
			}

			if (BurnBoxInstsOf.Contains(BoxOwner)) {
				if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
					auto Index = FlamBoxInstsOf[BoxOwner].Indices[0];
					ParamValue = HeatBoxAt[Index].InstInfoOf[BoxOwner].MinFireSize;
					
				}
			}
		}

		else {
			ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			
		}
		break;

	default:
		break;
	}
}

void AHeatmap::SetHBParam(AActor* BoxOwner, SetHeatBoxFuncParamType ParamType, float UserValue)
{
	switch (ParamType)
	{
		case (SetHeatBoxFuncParamType::MaxTemperature) :
			if (RegisteredBowOwners.Contains(BoxOwner)) {
				
				if (FlamBoxInstsOf.Contains(BoxOwner)) {
					if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : FlamBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].MaxTemperature = UserValue;
						}
					}
				}

				if(BurnBoxInstsOf.Contains(BoxOwner)) {
					if(BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : BurnBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].MaxTemperature = UserValue;
						}
					}
				}	
			}
			
			else {
				ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			}
			break;

		case (SetHeatBoxFuncParamType::IgnitionPoint):
			if (RegisteredBowOwners.Contains(BoxOwner)) {

				if (FlamBoxInstsOf.Contains(BoxOwner)) {
					if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : FlamBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].IgnitionPoint = UserValue;
						}
					}
				}

				if (BurnBoxInstsOf.Contains(BoxOwner)) {
					if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : BurnBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].IgnitionPoint = UserValue;
						}
					}
				}
			}

			else {
				ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			}
			break;
		
		case (SetHeatBoxFuncParamType::HeatAbsorbRate):
			if (RegisteredBowOwners.Contains(BoxOwner)) {

				if (FlamBoxInstsOf.Contains(BoxOwner)) {
					if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : FlamBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].HeatAbsorbRate = UserValue;
						}
					}
				}

				if (BurnBoxInstsOf.Contains(BoxOwner)) {
					if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : BurnBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].HeatAbsorbRate = UserValue;
						}
					}
				}
			}

			else {
				ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			}
			break;

		case (SetHeatBoxFuncParamType::HeatEmitRate):
			if (RegisteredBowOwners.Contains(BoxOwner)) {

				if (FlamBoxInstsOf.Contains(BoxOwner)) {
					if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : FlamBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].HeatEmitRate = UserValue;
						}
					}
				}

				if (BurnBoxInstsOf.Contains(BoxOwner)) {
					if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : BurnBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].HeatEmitRate = UserValue;
						}
					}
				}
			}

			else {
				ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			}
			break;

		case (SetHeatBoxFuncParamType::FuelCount):
			if (RegisteredBowOwners.Contains(BoxOwner)) {

				if (FlamBoxInstsOf.Contains(BoxOwner)) {
					if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : FlamBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].FuelCount = UserValue;
						}
					}
				}

				if (BurnBoxInstsOf.Contains(BoxOwner)) {
					if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : BurnBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].FuelCount = UserValue;
						}
					}
				}
			}

			else {
				ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			}
			break;

		case (SetHeatBoxFuncParamType::MaxFireSize):
			if (RegisteredBowOwners.Contains(BoxOwner)) {

				if (FlamBoxInstsOf.Contains(BoxOwner)) {
					if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : FlamBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].MaxFireSize = UserValue;
						}
					}
				}

				if (BurnBoxInstsOf.Contains(BoxOwner)) {
					if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : BurnBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].MaxFireSize = UserValue;
						}
					}
				}
			}

			else {
				ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			}
			break;

		case (SetHeatBoxFuncParamType::MinFireSize):
			if (RegisteredBowOwners.Contains(BoxOwner)) {

				if (FlamBoxInstsOf.Contains(BoxOwner)) {
					if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : FlamBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].MinFireSize = UserValue;
						}
					}
				}

				if (BurnBoxInstsOf.Contains(BoxOwner)) {
					if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
						for (auto Index : BurnBoxInstsOf[BoxOwner].Indices) {
							HeatBoxAt[Index].InstInfoOf[BoxOwner].MinFireSize = UserValue;
						}
					}
				}
			}

			else {
				ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
			}
			break;

		default:
			break;
	}
}

void AHeatmap::SetFireOn(AActor* BoxOwner)
{
	if (RegisteredBowOwners.Contains(BoxOwner)) {

		if (FlamBoxInstsOf.Contains(BoxOwner)) {
			if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
				for (auto Index : FlamBoxInstsOf[BoxOwner].Indices) {
					HeatBoxAt[Index].InstInfoOf[BoxOwner].CurrTemperature = HeatBoxAt[Index].InstInfoOf[BoxOwner].IgnitionPoint;
				}
			}
		}

		if (BurnBoxInstsOf.Contains(BoxOwner)) {
			if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
				for (auto Index : BurnBoxInstsOf[BoxOwner].Indices) {
					HeatBoxAt[Index].InstInfoOf[BoxOwner].CurrTemperature = HeatBoxAt[Index].InstInfoOf[BoxOwner].IgnitionPoint;
				}
			}
		}

		UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(BoxOwner->GetComponentByClass(UStaticMeshComponent::StaticClass()));
		StaticMeshComp->SetCollisionObjectType(ECC_GameTraceChannel2);
	}

	else {
		ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
	}
}

void AHeatmap::AddHeat(FIntVector InMapIndex, float Increment)
{
	const int CoreIdx = MapToCoreIndex(InMapIndex);
	float& CurrentHeat = HeatGenField_Accumulator[CoreIdx];
	CurrentHeat += Increment;
}

void AHeatmap::SetBodyTemperature(AActor* BoxOwner, float Temp)
{
	if (RegisteredBowOwners.Contains(BoxOwner)) {

		if (FlamBoxInstsOf.Contains(BoxOwner)) {
			if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
				for (auto Index : FlamBoxInstsOf[BoxOwner].Indices) {
					HeatBoxAt[Index].InstInfoOf[BoxOwner].CurrTemperature = Temp;
				}
			}
		}

		if (BurnBoxInstsOf.Contains(BoxOwner)) {
			if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
				for (auto Index : BurnBoxInstsOf[BoxOwner].Indices) {
					HeatBoxAt[Index].InstInfoOf[BoxOwner].CurrTemperature = Temp;
				}
			}
		}
	}

	else {
		ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
	}
}

float AHeatmap::GetBodyTemperature(AActor* BoxOwner) const
{
	float BodyTemperature = 0.f;
	float NumBoxInsts = FlamBoxInstsOf[BoxOwner].Indices.Num() + BurnBoxInstsOf[BoxOwner].Indices.Num();

	if (RegisteredBowOwners.Contains(BoxOwner)) {

		if (FlamBoxInstsOf.Contains(BoxOwner)) {
			if (FlamBoxInstsOf[BoxOwner].Indices.Num() > 0) {
				for (auto Index : FlamBoxInstsOf[BoxOwner].Indices) {
					BodyTemperature += HeatBoxAt[Index].InstInfoOf[BoxOwner].CurrTemperature;
				}
			}
		}

		if (BurnBoxInstsOf.Contains(BoxOwner)) {
			if (BurnBoxInstsOf[BoxOwner].Indices.Num() > 0) {
				for (auto Index : BurnBoxInstsOf[BoxOwner].Indices) {
					BodyTemperature += HeatBoxAt[Index].InstInfoOf[BoxOwner].CurrTemperature;
				}
			}
		}
	}

	else {
		ensureMsgf(0, TEXT("An Actor(%s) is not registered"), *BoxOwner->GetName());
	}

	if (NumBoxInsts > 0) {
		BodyTemperature /= NumBoxInsts;
	}

	return (BodyTemperature == 0.f) ? 20.f : BodyTemperature; 
}

void AHeatmap::RegisterNewBoxOwners_Impl(TMap<AActor*, FFlammableBoxInsts>& OutFlamBoxInstsOf,
	TMap<AActor*, FBurningBoxInsts>& OutBurnBoxInstsOf,
	TMap<FIntVector, FHeatBox>& OutHeatBoxAt)
{
	TArray<UBoxComponent*> HeatCells;
	GetComponents(HeatCells);

	for (auto HeatCell : HeatCells) {
		TArray<AActor*> OverlapActors;

		TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = { UCollisionProfile::Get()->ConvertToObjectType(ECC_GameTraceChannel1) };
		TArray<AActor*> ActorsToIgnore = {};

		UKismetSystemLibrary::ComponentOverlapActors(HeatCell, HeatCell->GetComponentTransform(), ObjectTypes, NULL, ActorsToIgnore, OverlapActors);

		if (OverlapActors.Num() > 0) {

			for (auto Actor : OverlapActors) {

				if (RegisteredBowOwners.Contains(Actor))
					continue;

				FIntVector MapIdx;

				GetMapIndex(HeatCell->GetRelativeLocation(), MapIdx);

				if (FFlammableBoxInsts* Found = OutFlamBoxInstsOf.Find(Actor)) {
					Found->Indices.AddUnique(MapIdx);
				}
				else {
					OutFlamBoxInstsOf.Emplace(Actor, FFlammableBoxInsts());
					OutFlamBoxInstsOf[Actor].Indices.Add(MapIdx);
				}

				FHeatBoxInfoDefaultInit HeatBoxInfoInit;

				FHeatBox& Found = OutHeatBoxAt.FindOrAdd(MapIdx);
				Found.InstInfoOf.Add(Actor, FHeatBoxInfo(HeatBoxInfoInit));
			}
		}

		if (HeatCell == HeatCells.Last()) {
			for (auto& InstOf : FlamBoxInstsOf) {
				AActor* BoxOwner = InstOf.Key;
				if (RegisteredBowOwners.Contains(BoxOwner) == false) {
					RegisteredBowOwners.Add(BoxOwner);
				}
			}
		}
	}
}

void AHeatmap::Apply()
{
	UnitSpacings = 100 * GetActorScale3D();
	
	HeatGenField.Init(0., (NumDepthCells + 2) * (NumWidthCells + 2) * (NumHeightCells + 2));

	HeatGenField_Accumulator.Init(0., (NumDepthCells + 2) * (NumWidthCells + 2) * (NumHeightCells + 2));

	UMaterialInstanceDynamic* DynVizColor = UMaterialInstanceDynamic::Create(VizColor, this);
	GraphViz->SetMaterial(0, DynVizColor);
	
#if WITH_EDITOR	
	TArray<FTransform> Transforms;
	if ((uint8)ShowHeatMap & (uint8)VisualVerbosity::Visual_HeatColor) {
		Transforms.Empty(NumDepthCells * NumWidthCells * NumHeightCells);
	}
#endif

	for (int i = 0; i < NumDepthCells; i++) {
		for (int j = 0; j < NumWidthCells; j++) {
			for (int k = 0; k < NumHeightCells; k++) {
				FVector CompRelLoc(100.f * i, 100.f * j, 100.f * k);

				UBoxComponent* HeatCell = Cast<UBoxComponent>(AddComponentByClass(UBoxComponent::StaticClass(), false, FTransform(CompRelLoc), false));
				HeatCell->SetBoxExtent(FVector{ 50. }); // 1m^3 in Unreal
				HeatCell->SetLineThickness(3.0f);
				HeatCell->SetCollisionResponseToAllChannels(ECR_Ignore);
				HeatCell->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap); //ECC_GameTraceChannel1 == 'Firebox'

				HeatCell->SetVisibility(false);
				HeatCell->bHiddenInGame = 1;

#if WITH_EDITOR	
				if ((uint8)ShowHeatMap & (uint8)VisualVerbosity::Visual_HeatGrid) {
					HeatCell->SetVisibility(true);
					HeatCell->bHiddenInGame = 0;
				}

				if ((uint8)ShowHeatMap & (uint8)VisualVerbosity::Visual_HeatColor) {
					Transforms.Emplace(FVector(100.f * i, 100.f * j, 100.f * k));
				}

				if ((uint8)ShowHeatMap & (uint8)VisualVerbosity::Visual_HeatValue) {
					AddComponentByClass(UTextRenderComponent::StaticClass(), false, FTransform(FRotator(0., 180., 0.), CompRelLoc + FVector(0., -25., -15.)), false);
				}
#endif
			}
		}
	}

#if WITH_EDITOR	
	if ((uint8)ShowHeatMap & (uint8)VisualVerbosity::Visual_HeatColor) {
		GraphViz->AddInstances(Transforms, false, false);
	}
#endif
}

void AHeatmap::RouteUpdateHeatmap()
{
	PreUpdateHeatmap();
	UpdateHeatmap();
	PostUpdateHeatmap();
}


void AHeatmap::TraceBoxOwner(const FIntVector& BoxIndex, AActor* const BoxOwner, int& CurrentHitCount, ECollisionChannel BoxType)
{
	FVector BoxRelPos;
	GetMapWorldPos(BoxIndex, BoxRelPos);

	// BurningActor의 스테틱 메쉬 랜덤 표면에 이펙트를 스폰합니다. 
	FHitResult HitResult;
	FVector TrajNode1 = BoxRelPos;
	FVector TrajNode2 = RandRangeFromBoxSurface(BoxRelPos, UnitSpacings) + (10. * FMath::VRand());
	TArray<FVector> HitTrajectory;

	//int SelectInt = FMath::RandRange(0, 1);
	int SelectInt = 1;

	if (SelectInt == 0) {
		HitTrajectory = { TrajNode1, TrajNode2 }; // 중심에서 박스 표면 방향
	}
	else {
		HitTrajectory = { TrajNode2, TrajNode1 }; // 박스 표면에서 중심 방향
	}

	FCollisionObjectQueryParams CollObjQueryParams;
	CollObjQueryParams.AddObjectTypesToQuery(BoxType);
	
	FCollisionQueryParams CollQueryParams;
	TArray<AActor*> BurningActors;
	BurnBoxInstsOf.GenerateKeyArray(BurningActors);
	BurningActors.Remove(BoxOwner);

	CollQueryParams.AddIgnoredActors(BurningActors);

#if WITH_EDITOR
	if ((uint8)ShowHeatMap & (uint8)VisualVerbosity::Visual_Trace) {
		const FName TraceTag("Debug Trace");
		GetWorld()->DebugDrawTraceTag = TraceTag;
		CollQueryParams.TraceTag = TraceTag;
	}
#endif

	GetWorld()->LineTraceSingleByObjectType(HitResult, HitTrajectory[0], HitTrajectory[1], CollObjQueryParams, CollQueryParams);

	if (HitResult.bBlockingHit) {
		if (BoxOwner == HitResult.GetActor()) {

#if WITH_EDITOR
			if ((uint8)ShowHeatMap & (uint8)VisualVerbosity::Visual_Trace) {
				// 충돌된 오브젝트 표면을 표시합니다.
				DrawDebugSphere(GetWorld(), HitResult.Location, 1., 12, FColor::Magenta, false, 1., 5.);
			}		
#endif			
			HeatBoxAt[BoxIndex].InstInfoOf[BoxOwner].HitPoints.AddUnique(HitResult.Location);
/*
#if WITH_EDITOR
			if (bShowLogMsg) {
			// 라인 트레이싱 결과를 출력합니다.
			UE_LOG(LogTemp, Warning, TEXT("%s"), *HitResult.ToString());
			}
#endif
*/
			--CurrentHitCount;
		}
	}
}
		
// UpdateInterval 간격의 업데이트 로직을 담고있는 함수입니다.
void AHeatmap::PreUpdateHeatmap()
{
	TMap<AActor*, FFlammableBoxInsts> Temp_FlamBoxInstsOf = FlamBoxInstsOf;
	for (auto& InstOf : Temp_FlamBoxInstsOf) {
		AActor* BoxOwner = InstOf.Key;
		const TArray<FIntVector>& FlammableBoxIndices = InstOf.Value.Indices;
		for (FIntVector FlammableBoxIndex : FlammableBoxIndices) {
			FHeatBoxInfo& HeatBoxInfo = HeatBoxAt[FlammableBoxIndex].InstInfoOf[BoxOwner];
			const float CurrTemperature = HeatBoxInfo.CurrTemperature;
			const float IgnitionPoint = HeatBoxInfo.IgnitionPoint;
			
			if (HeatBoxInfo.IsIgnitionStarting()) {
				BoxOwner->Tags.AddUnique(Burning);

				// 위상 변화
				BurnBoxInstsOf.FindOrAdd(BoxOwner).Indices.Add(FlammableBoxIndex);
				
				UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(BoxOwner->GetComponentByClass(UStaticMeshComponent::StaticClass()));
				StaticMeshComp->SetCollisionObjectType(ECC_GameTraceChannel2);
				
				FlamBoxInstsOf[BoxOwner].Indices.Remove(FlammableBoxIndex);
				HeatBoxInfo.IsBurning = true;

				if (FlamBoxInstsOf[BoxOwner].Indices.Num() == 0)
				{
					BoxOwner->Tags.Remove("Flammable");
				}
			}
		}	
	}

	TMap<AActor*, FBurningBoxInsts> Temp_BurnBoxInstsOf = BurnBoxInstsOf;
	for (auto& InstOf : Temp_BurnBoxInstsOf) {
		AActor* BoxOwner = InstOf.Key;
		const TArray<FIntVector>& BurningBoxIndices = InstOf.Value.Indices;
		for (FIntVector BurningBoxIndex : BurningBoxIndices) {
			FHeatBoxInfo& HeatBoxInfo = HeatBoxAt[BurningBoxIndex].InstInfoOf[BoxOwner];
			int& CurrentHitQueryCount = HeatBoxInfo.HitQueryCount;
			
			if (HeatBoxInfo.IsBurntOut()) {
				BoxOwner->Tags.AddUnique(BurntOut);
				// 위상 변화
				BurnBoxInstsOf[BoxOwner].Indices.Remove(BurningBoxIndex);				
				if (BurnBoxInstsOf[BoxOwner].Indices.Num() == 0) {
					BoxOwner->Tags.Remove("Burning");

				}
			}

			else if (HeatBoxInfo.IsExtinguished()) {
				
				// 위상 변화
				FlamBoxInstsOf.FindOrAdd(BoxOwner).Indices.Add(BurningBoxIndex);

				BurnBoxInstsOf[BoxOwner].Indices.Remove(BurningBoxIndex);
				HeatBoxInfo.IsBurning = false;

				if (BurnBoxInstsOf[BoxOwner].Indices.Num() == 0) {
					BoxOwner->Tags.Remove("Burning");
					
					UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(BoxOwner->GetComponentByClass(UStaticMeshComponent::StaticClass()));
					StaticMeshComp->SetCollisionObjectType(ECC_GameTraceChannel1);
				}
			}

			else {
				for (int i = 0; CurrentHitQueryCount > 0; ++i) {
					
					if (i >= HITQUERY_TOLERANCE) {
/*
#if WITH_EDITOR
						if (bShowLogMsg) {
							UE_LOG(Firebox, Error, TEXT("Hit query attempt at %d (out of %d) exceeded the limit of %d"), HITQUERY_REQ -CurrentHitQueryCount, HITQUERY_REQ, HITQUERY_TOLERANCE);
						}
#endif
*/						
						// 등록 취소
						BurnBoxInstsOf[BoxOwner].Indices.Remove(BurningBoxIndex);
						HeatBoxAt[BurningBoxIndex].InstInfoOf.Remove(BoxOwner);
						
						if (HeatBoxAt[BurningBoxIndex].InstInfoOf.Num() == 0) {
							HeatBoxAt.Remove(BurningBoxIndex);
						}

						break;
					}

					TraceBoxOwner(BurningBoxIndex, BoxOwner, CurrentHitQueryCount, ECC_GameTraceChannel2);
				} //ECC_GameTraceChannel1 == 'Firebox'

				/*UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(BoxOwner->GetComponentByClass(UStaticMeshComponent::StaticClass()));
				StaticMeshComp->SetCollisionObjectType(ECC_GameTraceChannel3);*/
			}
		}
	}
}

void AHeatmap::UpdateHeatmap()
{
	// 화염 확산/축소 로직
	TMultiMap<AActor*, TSharedPtr<FFireInBox>> NewGoingFires;
	for (auto& InstOf : BurnBoxInstsOf) {
		AActor* BoxOwner = InstOf.Key;
		const TArray<FIntVector>& BurningBoxIndices = InstOf.Value.Indices;
		
		for (FIntVector BurningBoxIndex : BurningBoxIndices) {
			TArray<FIntVector> AggregateIndices;
			TArray<FVector> AggregateHits;

			FHeatBoxInfo& HeatBoxInfo = HeatBoxAt[BurningBoxIndex].InstInfoOf[BoxOwner];

			// 각 연소 오브젝트의 현재 화염 확산 범위 깊이 우선 탐색(DFS)
			if (HeatBoxInfo.Visited != true) {
				AggregateIndices.Add(BurningBoxIndex);
				AggregateHits.Append(HeatBoxInfo.HitPoints);
				
				FVector BoxWorldPos;
				GetMapWorldPos(BurningBoxIndex, BoxWorldPos);
				AggregateHitPoints(BurningBoxIndex, BoxOwner, AggregateIndices, AggregateHits);
				
				// 현재 화염 범위 생성
				NewGoingFires.Add(BoxOwner, MakeShared<FFireInBox
				>(AggregateIndices));

				PointEstimator PtEstimator;
#if WITH_EDITOR	
				if ((uint8)ShowHeatMap & (uint8)VisualVerbosity::Visual_Estimator) {
					PointEstimator Temp_PtEstimator(AggregateHits, GetWorld(), BoxWorldPos.Z);
					PtEstimator = Temp_PtEstimator;
				}
				
				else {
					PointEstimator Temp_PtEstimator(AggregateHits);
					PtEstimator = Temp_PtEstimator;
				}
#else
				PointEstimator Temp_PointEstimator(AggregateHits);
				PtEstimator = Temp_PointEstimator;
#endif
				FVector2D EstimatedCenter = PtEstimator.EstimateCenter2D();
				float EstimateCenterPosZ = PtEstimator.EstimateCenterPosZ();
				float EstimatedArea = PtEstimator.EstimateArea2D();
				
				TArray<TSharedPtr<FFireInBox>*> OldFiresToCheck;
				GoingFires.MultiFindPointer(BoxOwner, OldFiresToCheck);

				// 현재 화염 확산 범위와 기존 화염 확산 범위를 비교 및 업데이트
				//Note: Null Pointer 반환시 기존 화염 확산 범위와 동일
				bool OldFireChecked = OldFiresToCheck.ContainsByPredicate([&](TSharedPtr<FFireInBox>*& FireToCheck){
					return (*FireToCheck)->BoxIndices == AggregateIndices;
					});
				if(!OldFireChecked) {
					TArray<TSharedPtr<FFireInBox>*> NewFiresToCheck;
					NewGoingFires.MultiFindPointer(BoxOwner, NewFiresToCheck);

					auto NewFireChecked = NewFiresToCheck.FindByPredicate([&](TSharedPtr<FFireInBox>*& FireToCheck) {
						return (*FireToCheck)->BoxIndices == AggregateIndices;
						});
					check(NewFireChecked);
					(**NewFireChecked)->SpawnLocation = FVector(EstimatedCenter, EstimateCenterPosZ);
					
					// 이펙트 최초 생성 
					(**NewFireChecked)->SpawnSize = HeatBoxInfo.ClampFireSize(EstimatedArea);

					(**NewFireChecked)->FireEffect = GetWorld()->SpawnActor<AFireBlob>(FireEffectClass, FTransform(FRotator(), (**NewFireChecked)->SpawnLocation, (**NewFireChecked)->SpawnSize));
				}

				// 연소로 인해 방출된 열에너지를 히트맵에 반영합니다.
				for (auto AggregateIndex : AggregateIndices) {
					const int CoreIdx = MapToCoreIndex(AggregateIndex);
					float& CurrentHeat = HeatGenField[CoreIdx];
					
					FHeatBoxInfo& AggHeatBoxInfo = HeatBoxAt[AggregateIndex].InstInfoOf[BoxOwner];
					AggHeatBoxInfo.RadiationArea = EstimatedArea;
					AggHeatBoxInfo.IgnitionCore = FVector(EstimatedCenter, EstimateCenterPosZ);
					
					float RadiatedHeatEnergy = AggHeatBoxInfo.RadiateHeat();
					CurrentHeat += RadiatedHeatEnergy;
				}
			}
		}
	}
		 
	for (auto GoingFire : GoingFires) {
		TArray<TSharedPtr<FFireInBox>*> StayingFiresCheck;
		NewGoingFires.MultiFindPointer(GoingFire.Key, StayingFiresCheck);
		auto StayingFireChecked = StayingFiresCheck.FindByPredicate([&](TSharedPtr<FFireInBox>*& FireToCheck) {
			return (*FireToCheck)->BoxIndices == GoingFire.Value->BoxIndices;
			});
		// 현재 화염 범위와 동일한 기존 화염 범위 데이터 보존
		if (StayingFireChecked) {
			(**StayingFireChecked) = GoingFire.Value;
		}
		// 현재 화염 범위와 동일하지 않은 기존 화염 범위 소멸
		else  {
			OrphanedFires.Add(GoingFire.Value);
		} // StayingFireChecked == nullptr
	}

	if (OrphanedFires.Num() > 0) {
		TArray<TSharedPtr<FFireInBox>> TempOrphanedFires = OrphanedFires;
		for (auto TempOrphanedFire : TempOrphanedFires) {
			
			if(!TempOrphanedFire->FireEffect->GetIsPending()) {
				TempOrphanedFire->FireEffect->ShrinkToDeath();
				
			}
			
			else {
				if (TempOrphanedFire->FireEffect->GetMarkForDestroy()) {
					OrphanedFires.Remove(TempOrphanedFire);
				}
			}
		}
	}

	// 화염 범위 최신화
	GoingFires = NewGoingFires;
	
	for (int i = 0; i < HeatGenField.Num(); i++) {
		HeatGenField[i] += HeatGenField_Accumulator[i];
	}

	HeatGenField_Accumulator.Init(0., (NumDepthCells + 2)* (NumWidthCells + 2)* (NumHeightCells + 2));
	
	// 히트필드 업데이트
	Diffuse(HeatGenField);

#if WITH_EDITOR
	if ((uint8)ShowHeatMap & (uint8)VisualVerbosity::Visual_HeatValue) {
		TArray<UTextRenderComponent*> TextComponents;
		GetComponents(TextComponents);
		int InstIndex = -1;
		for (int i = 0; i < NumDepthCells; i++) {
			for (int j = 0; j < NumWidthCells; j++) {
				for (int k = 0; k < NumHeightCells; k++) {
					InstIndex++;
					
					FVector TextLoc(100.f * i, 100.f * j, 100.f * k);
					FIntVector MapIdx;
					GetMapIndex(TextLoc, MapIdx);
					int CoreIdx = MapToCoreIndex(MapIdx);
					float Heat = HeatGenField[CoreIdx];		

					if ((uint8)ShowHeatMap & (uint8)VisualVerbosity::Visual_HeatValue) {
						FString HeatStr = FString::Printf(TEXT("%.2f"), Heat);
						TextComponents[InstIndex]->SetText(FText::FromString(HeatStr));
					}
				}
			}
		}
	}
	if ((uint8)ShowHeatMap & (uint8)VisualVerbosity::Visual_HeatColor) {
		int InstIndex = -1;
		for (int i = 0; i < NumDepthCells; i++) {
			for (int j = 0; j < NumWidthCells; j++) {
				for (int k = 0; k < NumHeightCells; k++) {
					InstIndex++;
					
					FVector VizLoc(100.f * i, 100.f * j, 100.f * k);
					FIntVector MapIdx;
					GetMapIndex(VizLoc, MapIdx);
					int CoreIdx = MapToCoreIndex(MapIdx);
					float Heat = HeatGenField[CoreIdx];
					
					TArray<float> CustomData;
					CustomData.Init(0.f, 4);
					CustomData[0] = 1.f;
					CustomData[1] = 0.f;
					CustomData[2] = 0.f;
					CustomData[3] = Heat;
					GraphViz->SetCustomData(InstIndex, CustomData);
					
				}
			}
		}
	}

	if ((uint8)ShowHeatMap & (uint8)VisualVerbosity::Visual_HeatColor) {
		GraphViz->MarkRenderStateDirty();
	}
#endif

#if WITH_EDITOR
	TEMPERATURE_LOG_HEADER()
	MISC_LOG_HEADER()
	HEATDMG_LOG_HEADER()
#endif

	// FlammableActor 데미지 수용
	for (auto& InstOf : FlamBoxInstsOf) {
		AActor* BoxOwner = InstOf.Key;
		const TArray<FIntVector>& FlammableBoxIndices = InstOf.Value.Indices;
		for (FIntVector FlammableBoxIndex : FlammableBoxIndices) {
			const int CoreIdx = MapToCoreIndex(FlammableBoxIndex);
			float& CurrentHeat = HeatGenField[CoreIdx];

			FHeatBoxInfo& HeatBoxInfo = HeatBoxAt[FlammableBoxIndex].InstInfoOf[BoxOwner];

			HeatBoxInfo.ReceiveHeat(CurrentHeat, UpdateInterval);

#if WITH_EDITOR
			TEMPERATURE_LOG_BODY(FlammableBoxIndex)
			MISC_LOG_BODY(FlammableBoxIndex)
			HEATDMG_LOG_BODY(FlammableBoxIndex)
#endif
		}
	}

	// BurningActor 데미지 수용
	for (auto& InstOf : BurnBoxInstsOf) {
		AActor* BoxOwner = InstOf.Key;
		const TArray<FIntVector>& BurningBoxIndices = InstOf.Value.Indices;
		for (FIntVector BurningBoxIndex : BurningBoxIndices) {
			const int CoreIdx = MapToCoreIndex(BurningBoxIndex);
			float& CurrentHeat = HeatGenField[CoreIdx];
			
			FHeatBoxInfo& HeatBoxInfo = HeatBoxAt[BurningBoxIndex].InstInfoOf[BoxOwner];

			HeatBoxInfo.ReceiveHeat(CurrentHeat, UpdateInterval);

#if WITH_EDITOR
			TEMPERATURE_LOG_BODY(BurningBoxIndex)
			MISC_LOG_BODY(BurningBoxIndex)
			HEATDMG_LOG_BODY(BurningBoxIndex)
#endif
		}
	}
}

void AHeatmap::PostUpdateHeatmap()
{
	// FlammableActor 포스트-프로세싱
	//for (auto & InstOf : FlamBoxInstsOf) {
	//	AActor* BoxOwner = InstOf.Key;
	//	const TArray<FIntVector>& FlammableBoxIndices = InstOf.Value.Indices;
	//	for (FIntVector FlammableBoxIndex : FlammableBoxIndices) {
	//		const int CoreIdx = MapToCoreIndex(FlammableBoxIndex);
	//		float& CurrentHeat = HeatGenField[CoreIdx];

	//		FHeatBoxInfo& HeatBoxInfo = HeatBoxAt[FlammableBoxIndex].InstInfoOf[BoxOwner];
	//	}
	//}

	// BurningActor 포스트-프로세싱
	for (auto& InstOf : BurnBoxInstsOf) {
		AActor* BoxOwner = InstOf.Key;
		const TArray<FIntVector>& BurningBoxIndices = InstOf.Value.Indices;
		for (FIntVector BurningBoxIndex : BurningBoxIndices) {
			// 초기화
			const int CoreIdx = MapToCoreIndex(BurningBoxIndex);
			float& CurrentHeat = HeatGenField[CoreIdx];
			
			FHeatBoxInfo& HeatBoxInfo = HeatBoxAt[BurningBoxIndex].InstInfoOf[BoxOwner];
			
			HeatBoxInfo.Visited = false;
			HeatBoxInfo.FuelCount--;
			
			CurrentHeat = 0.;
		}

		TArray<TSharedPtr<FFireInBox>> Fires;
		GoingFires.MultiFind(BoxOwner, Fires);
		
		for (auto Fire : Fires) {
			// 평균 영역 온도 구하기
			TArray<FIntVector> FireDomain = Fire->BoxIndices;
			int NumSubdomain = FireDomain.Num();
			check(NumSubdomain);
			float AggregateTemp = 0;
			float AverageTemp = 0;
			for (auto FireSubdomain : FireDomain) {
				AggregateTemp += HeatBoxAt[FireSubdomain].InstInfoOf[BoxOwner].CurrTemperature;
			
				if (FireSubdomain == FireDomain.Last()) {
					AverageTemp = AggregateTemp / NumSubdomain;

					float TempA = HeatBoxAt[FireSubdomain].InstInfoOf[BoxOwner].IgnitionPoint;
					float TempB = HeatBoxAt[FireSubdomain].InstInfoOf[BoxOwner].MaxTemperature;
					float SizeA = Fire->SpawnSize[0];
					float SizeB = HeatBoxAt[FireSubdomain].InstInfoOf[BoxOwner].MaxFireSize;

					// 이펙트 크기에 반영하기
					float NewFireSize = FMath::GetMappedRangeValueClamped(FVector2D(TempA, TempB), FVector2D(SizeA, SizeB), AverageTemp);
					
					Fire->SetFireSize(NewFireSize);
				}
			}
		}

		float BodyTemp = GetBodyTemperature(BoxOwner);
		float MaxTemp;
		GetHBParam(BoxOwner, SetHeatBoxFuncParamType::MaxTemperature, MaxTemp);
		float BodyHalfFullTemp = MaxTemp / 2;

		if (BodyTemp >= BodyHalfFullTemp) {
			OnBodyHalfBurnt.Broadcast(BoxOwner);
		}
		


	}
}

void AHeatmap::Diffuse(TArray<float>& Field)
{
	TArray<float> NewField = Field;

	for (int n = 0; n < 20; n++) {
		for (int i = 1; i <= NumDepthCells; i++) {
			for (int j = 1; j <= NumWidthCells; j++) {
				for (int k = 1; k <= NumHeightCells; k++) {
					NewField[CoreIndex(i, j, k)] = (
						Field[CoreIndex(i, j, k)] +
						HeatTransferRate * (
							NewField[CoreIndex(i - 1, j, k)] + NewField[CoreIndex(i + 1, j, k)] +
							NewField[CoreIndex(i, j - 1, k)] + NewField[CoreIndex(i, j + 1, k)] +
							NewField[CoreIndex(i, j, k - 1)] + NewField[CoreIndex(i, j, k + 1)])
						) / (1 + 6 * HeatTransferRate);
				}
			}
		}
	}

	Field = NewField;
}

bool AHeatmap::GetMapIndex(const FVector& RelPos, FIntVector& OutMapIndex) const
{
	FIntVector IndexToCheck = { (int)FMath::RoundHalfFromZero(RelPos.X / 100),
								(int)FMath::RoundHalfFromZero(RelPos.Y / 100),
								(int)FMath::RoundHalfFromZero(RelPos.Z / 100) };

	if (IsWithinMap(IndexToCheck)) {
		OutMapIndex = IndexToCheck;
		return true;
	}

	else {
		return false;
	}
}

void AHeatmap::GetMapWorldPos(const FIntVector& MapIndex, FVector& WorldPos) const
{
	FMatrix Offset = FTranslationMatrix::Make(FVector(100.f * MapIndex.X, 100.f * MapIndex.Y, 100.f * MapIndex.Z));
	
	FTransform HeatmapWorldTransfm = GetActorTransform();
	FMatrix HeatmapWorldMat = HeatmapWorldTransfm.ToMatrixWithScale();

	FMatrix ResultMat = Offset * HeatmapWorldMat;
	FTransform ResultTransfm = FTransform(ResultMat);

	WorldPos = ResultTransfm.GetLocation();
}

int AHeatmap::CoreIndex(int i, int j, int k) const
{
	return j + ((NumWidthCells + 2) * i) + ((NumDepthCells + 2) * (NumWidthCells + 2) * k);
}

int AHeatmap::MapToCoreIndex(const FIntVector& MapIndex) const
{
	return (MapIndex.Y + 1) + ((NumWidthCells + 2) * (MapIndex.X + 1)) + (((NumDepthCells + 2) * (NumWidthCells + 2)) * (MapIndex.Z + 1));
}

bool AHeatmap::IsWithinMap(const FIntVector& IndexToCheck) const
{
	return IndexToCheck.X >= 0 && IndexToCheck.X < NumDepthCells &&
			IndexToCheck.Y >= 0 && IndexToCheck.Y < NumWidthCells &&
			IndexToCheck.Z >= 0 && IndexToCheck.Z < NumHeightCells;
}


void AHeatmap::GetBoxVertices(const FVector& Center, const FVector& Spacings, FBoxVertices& OutVertices)
{
	FBoxVertices Offsets;
	for (int i = 0; i < 8; i++) {
		Offsets[i].X = ((i & 1) ? (Spacings.X / 2) : (-Spacings.X / 2));
		Offsets[i].Y = ((i & 2) ? (Spacings.Y / 2) : (-Spacings.Y / 2));
		Offsets[i].Z = ((i & 4) ? (Spacings.Z / 2) : (-Spacings.Z / 2));

		OutVertices[i] = Center + Offsets[i];
	}
}

FVector AHeatmap::RandRangeFromBoxVolume(const FVector& Center, const FVector& Spacings)
{
	FBoxVertices BoxVertices;
	GetBoxVertices(Center, Spacings, BoxVertices);

	float RandX = FMath::FRandRange(BoxVertices[0].X, BoxVertices[1].X);
	float RandY = FMath::FRandRange(BoxVertices[0].Y, BoxVertices[2].Y);
	float RandZ = FMath::FRandRange(BoxVertices[0].Z, BoxVertices[4].Z);

	FVector RandXYZ = { RandX, RandY, RandZ };

	return RandXYZ;
}

FVector AHeatmap::RandRangeFromBoxSurface(const FVector& Center, const FVector& Spacings)
{
	FBoxVertices BoxVertices;
	GetBoxVertices(Center, Spacings, BoxVertices);
	
	BoxFaces RandFace = (BoxFaces)FMath::RandRange(0, 5);
	float RandX;
	float RandY;
	float RandZ;
	FVector RandXYZ;;

	switch (RandFace)
	{
		case BoxFaces::Bottom:
			RandX = FMath::FRandRange(BoxVertices[0].X, BoxVertices[1].X);
			RandY = FMath::FRandRange(BoxVertices[0].Y, BoxVertices[2].Y);
			RandXYZ = { RandX, RandY, BoxVertices[0].Z };
			break;
		
		case BoxFaces::Top:
			RandX = FMath::FRandRange(BoxVertices[0].X, BoxVertices[1].X);
			RandY = FMath::FRandRange(BoxVertices[0].Y, BoxVertices[2].Y);
			RandXYZ = { RandX, RandY, BoxVertices[4].Z };
			break;
		
		case BoxFaces::Left:
			RandX = FMath::FRandRange(BoxVertices[0].X, BoxVertices[1].X);
			RandZ = FMath::FRandRange(BoxVertices[0].Z, BoxVertices[4].Z);
			RandXYZ = { RandX, BoxVertices[0].Y, RandZ };
			break;

		case BoxFaces::Right:
			RandX = FMath::FRandRange(BoxVertices[0].X, BoxVertices[1].X);
			RandZ = FMath::FRandRange(BoxVertices[0].Z, BoxVertices[4].Z);
			RandXYZ = { RandX, BoxVertices[2].Y, RandZ };
			break;
	
		case BoxFaces::Front:
			RandY = FMath::FRandRange(BoxVertices[0].Y, BoxVertices[2].Y);
			RandZ = FMath::FRandRange(BoxVertices[0].Z, BoxVertices[4].Z);
			RandXYZ = { BoxVertices[0].X, RandY, RandZ };
			break;
		
		case BoxFaces::Back:
			RandY = FMath::FRandRange(BoxVertices[0].Y, BoxVertices[2].Y);
			RandZ = FMath::FRandRange(BoxVertices[0].Z, BoxVertices[4].Z);
			RandXYZ = { BoxVertices[1].X, RandY, RandZ };
			break;
		
		default:
			check(0);
			break;
	}
	
	return RandXYZ;
}


float AHeatmap::BoxDiagLenth(const FVector& Spacings)
{
	return FMath::Sqrt( FMath::Square(Spacings.X) + FMath::Square(Spacings.Y) + FMath::Square(Spacings.Z) );
}

void AHeatmap::AggregateHitPoints(const FIntVector& BaseIndex, const AActor* BoxOwner, TArray<FIntVector>& AggregatorIndices, TArray<FVector>& AggregatedHitPoints)
{
	HeatBoxAt[BaseIndex].InstInfoOf[BoxOwner].Visited = true;

		for (int i = 0; i < 4; ++i) {
			FIntVector Offset = { 0, 0, 0 };
			Offset[i % 2] = 1 - 2 * (i / 2);
			FIntVector IndexToSearch = BaseIndex + Offset;
			if (BurnBoxInstsOf[BoxOwner].Indices.Contains(IndexToSearch)) {
				FHeatBoxInfo& HeatBoxInfo = HeatBoxAt[IndexToSearch].InstInfoOf[BoxOwner];
				if (!HeatBoxInfo.Visited) {
					AggregatedHitPoints.Append(HeatBoxInfo.HitPoints);
					AggregatorIndices.Add(IndexToSearch);
					AggregateHitPoints(IndexToSearch, BoxOwner, AggregatorIndices, AggregatedHitPoints);
				}
			}
		}
}

