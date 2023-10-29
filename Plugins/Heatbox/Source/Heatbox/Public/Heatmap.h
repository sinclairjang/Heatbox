// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Containers/Map.h"
#include "Heatmap.generated.h"

using namespace std;

#define HITQUERY_REQ 12 
#define HITQUERY_TOLERANCE 256

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHeatBox_OnBodyHalfBurnt, AActor*, BoxOwner);

/**
**/
USTRUCT()
struct FFlammableBoxInsts
{
	GENERATED_BODY()

	FFlammableBoxInsts()
	{}

	UPROPERTY(VisibleAnywhere)
	TArray<FIntVector> Indices;


};

/**
**/
USTRUCT()
struct FBurningBoxInsts
{
	GENERATED_BODY()

	FBurningBoxInsts()
	{}

	UPROPERTY(VisibleAnywhere)
	TArray<FIntVector> Indices;
};

/**
**/
USTRUCT()
struct FFireInBox
{
	GENERATED_BODY()

	FFireInBox()
	{}

	~FFireInBox();

	explicit FFireInBox(const TArray<FIntVector>& BoxIndices)
		: BoxIndices(BoxIndices)
	{}

	bool operator==(const FFireInBox& other)
	{
		return BoxIndices == other.BoxIndices;
	}
	
	void SetFireSize(float Size);

	UPROPERTY()
	TObjectPtr<AFireBlob> FireEffect;

	UPROPERTY()
	TArray<FIntVector> BoxIndices;

	UPROPERTY()
	FVector SpawnLocation;

	UPROPERTY()
	FVector SpawnSize;
};

struct FHeatBoxInfoDefaultInit {
	float CurrTemperature	= 20.f;
	float MaxTemperature	= 1000.f;
	float IgnitionPoint		= 100.f;
	float HeatAbsorbRate	= 1.f;
	float HeatEmitRate		= 1.f;
	float FuelCount			= 40;
	float MinFireSize		= 0.4f;
	float MaxFireSize		= 2.f;
	float RadiationArea		= -1.f;
	bool IsBurning			= false;
};

/**
**/
USTRUCT()
struct FHeatBoxInfo
{
	GENERATED_BODY()

	FHeatBoxInfo()
	{}
	
	explicit FHeatBoxInfo(const FHeatBoxInfoDefaultInit& HeatBoxInfoInit) :
		CurrTemperature(HeatBoxInfoInit.CurrTemperature),
		MaxTemperature(HeatBoxInfoInit.MaxTemperature),
		IgnitionPoint(HeatBoxInfoInit.IgnitionPoint),
		HeatAbsorbRate(HeatBoxInfoInit.HeatAbsorbRate),
		HeatEmitRate(HeatBoxInfoInit.HeatEmitRate),
		FuelCount(HeatBoxInfoInit.FuelCount),
		RadiationArea(HeatBoxInfoInit.RadiationArea),
		MinFireSize(HeatBoxInfoInit.MinFireSize),
		MaxFireSize(HeatBoxInfoInit.MaxFireSize),
		IsBurning(HeatBoxInfoInit.IsBurning)
		{}


	explicit FHeatBoxInfo(float MaxTemperature,
		float IgnitionPoint,
		float HeatAbsorbRate,
		float HeatEmitRate,
		float FuelCount,
		float MinFireSize,
		float MaxFireSize,
		float RadiationArea,
		bool IsBurning) :
		MaxTemperature(MaxTemperature),
		IgnitionPoint(IgnitionPoint),
		HeatAbsorbRate(HeatAbsorbRate),
		HeatEmitRate(HeatEmitRate),
		FuelCount(FuelCount),
		RadiationArea(RadiationArea),
		MinFireSize(MinFireSize),
		MaxFireSize(MaxFireSize),
		IsBurning(IsBurning)
		{}
	
	/**
	* 열 전달 방정식
	* 온도 변화량 = 열 에너지 x 열 흡수율
	*/
	void ReceiveHeat(float HeatEnergy, float UpdateInterval);
	
	/**
	* 흑체(Black-body) 방정식
	* 열 에너지 = 열 방출율 x 슈테판-볼츠만 상수 x (최대 온도[K]^4 - (최대 온도 - 현재 온도)[K]^4) x 방열 면적[m^2]
	*/
	float RadiateHeat();

	bool IsBurntOut() const;

	bool IsIgnitionStarting() const;
	
	bool IsExtinguished() const;

	FVector ClampFireSize(float EstimatedArea) const;

	/*
	*/
	UPROPERTY(VisibleAnywhere)
	float CurrTemperature;

	/** 
	* The maximum temperature that a substance undergoing a combustion reaction can reach 
	*/
	UPROPERTY(EditAnywhere)
	float MaxTemperature;

	/** 
	* The temperature at which the combustion reaction of a substance starts 
	*/
	UPROPERTY(EditAnywhere)
	float IgnitionPoint;

	/** 
	* Inherent heat energy absorption efficiency of a material 
	*/
	UPROPERTY(EditAnywhere)
	float HeatAbsorbRate;
	
	/**
	* Inherent heat energy release efficiency of a material
	*/
	UPROPERTY(EditAnywhere)
	float HeatEmitRate;

	/**
	* Remaining fuel or combustion rate of the material in which the combustion reaction took place
	*/
	UPROPERTY(EditAnywhere)
	float FuelCount;

	/**
	* The surface area of ​​the material where the combustion reaction took place
	* This parameter is applied to the black-body equation that calculates the amount of heat that causes damage or increases the temperature of nearby players and objects.
	*/
	UPROPERTY(VisibleAnywhere)
	float RadiationArea;

	/** 
	* Adjust the minimum size of the effect to visualize the combustion reaction 
	*/
	UPROPERTY(EditAnywhere)
	float MinFireSize;

	/**
	* Adjust the maximum size of the effect visualizing the combustion reaction 
	*/
	UPROPERTY(EditAnywhere)
	float MaxFireSize;

	/**
	* Adjust the maximum size of the effect visualizing the combustion reaction
	*/
	UPROPERTY(VisibleAnywhere)
	float HeatDamageApplied;
	
	/**
	* Adjust the maximum size of the effect visualizing the combustion reaction
	*/
	UPROPERTY(VisibleAnywhere)
	float HeatDamageReceived;

	/**
	* The fire point of a substance in which a combustion reaction has taken place
	*/
	UPROPERTY(VisibleAnywhere)
	FVector IgnitionCore;

	UPROPERTY(VisibleAnywhere)
	bool IsBurning;

	UPROPERTY()
	int HitQueryCount = HITQUERY_REQ;

	UPROPERTY()
	TArray<FVector> HitPoints;

	UPROPERTY()
	bool Visited = false;
};

/**
**/
USTRUCT()
struct FHeatBox
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
	TMap<AActor*, FHeatBoxInfo> InstInfoOf;
};

/**
**/
USTRUCT()
struct FBoxVertices
{
	GENERATED_BODY()
	
	FBoxVertices()
	{
		for (int i = 0; i < 8; i++)
		{
			Offset[i] = FVector(0,0,0);
		}
	}

	FVector& operator[](int i) {
		check(i < 8);
		return Offset[i];
	}

	UPROPERTY()
	FVector Offset[8];
};

/**
**/
UENUM()
enum class BoxFaces : uint8
{
	Bottom,
	Top,
	Left,
	Right,
	Front,
	Back,
};

/**
**/
UENUM()
enum class VisualVerbosity : uint8
{
	Visual_None				= 0,
	Visual_HeatGrid			= 1 << 0,
	Visual_HeatColor		= 1 << 1,
	Visual_HeatValue		= 1 << 2,
	Visual_Trace			= 1 << 3,
	Visual_Estimator		= 1 << 4,

	Visual_HeatGridWithColor			= Visual_HeatGrid | Visual_HeatColor,
	Visual_HeatGridWithValue			= Visual_HeatGrid | Visual_HeatValue,
	Visual_HeatGridWithColorAndValue	= Visual_HeatGrid | Visual_HeatValue | Visual_HeatColor,
	Visual_HeatGridWithTrace			= Visual_HeatGrid | Visual_Trace,
};

/**
**/
UENUM()
enum class SimStage : uint8
{
	None	= 0,
	Playing	= 1 << 0,
	Paused	= 1 << 1,
};

UENUM() 
enum class SetHeatBoxFuncParamType : uint8
{
	MaxTemperature,
	IgnitionPoint,
	HeatAbsorbRate,
	HeatEmitRate,
	FuelCount,
	MaxFireSize,
	MinFireSize,
}; 

/**
**/
UCLASS()
class HEATBOX_API AHeatmap final : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AHeatmap();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	/**
	**/
	UFUNCTION(BlueprintCallable, Category = "RtHeatmap")
	void RegisterNewBoxOwners();
	
	/**
	**/
	UFUNCTION(BlueprintCallable, Category = "RtHeatmap")
	void ClearRegistry();

	/**
	* 시뮬레이션을 시작합니다.
	**/
	UFUNCTION(BlueprintCallable, Category = "RtHeatmap")
	void StartSim();

	/**
	* 시뮬레이션을 정지합니다.
	**/
	UFUNCTION(BlueprintCallable, Category = "RtHeatmap")
	void PauseSim();

	/**
	* 시뮬레이션을 재개합니다.
	**/
	UFUNCTION(BlueprintCallable, Category = "RtHeatmap")
	void ResumeSim();

	/**
	* 시뮬레이션을 중단합니다.
	**/
	UFUNCTION(BlueprintCallable, Category = "RtHeatmap")
	void EndSim();

	/**
	**/
	UFUNCTION(BlueprintCallable, Category="RtHeatmap")
	void SetHBParam(AActor* BoxOwner, SetHeatBoxFuncParamType ParamType, float UserValue);

	/**
	**/
	UFUNCTION(BlueprintCallable, Category = "RtHeatmap")
	void GetHBParam(AActor* BoxOwner, SetHeatBoxFuncParamType ParamType, float& ParamValue);

	/**
	**/
	UFUNCTION(BlueprintCallable, Category = "RtHeatmap")
	void SetFireOn(AActor* BoxOwner);


	/**
	**/
	UFUNCTION(BlueprintCallable, Category = "RtHeatmap")
	void AddHeat(FIntVector InMapIndex, float Increment);

	/**
	**/
	UFUNCTION(BlueprintCallable, Category = "RtHeatmap")
	void SetBodyTemperature(AActor* BoxOwner, float Temp = 20.f);

	/**
	**/
	UFUNCTION(BlueprintCallable, Category = "RtHeatmap")
	float GetBodyTemperature(AActor* BoxOwner) const;

private:
	/**
	* 시뮬레이션을 적용할 액터를 등록합니다.
	**/
	void RegisterNewBoxOwners_Impl(TMap<AActor*, FFlammableBoxInsts>& OutFlamBoxInstsOf,
						TMap<AActor*, FBurningBoxInsts>& OutBurnBoxInstsOf,
						TMap<FIntVector, FHeatBox>& OutHeatBoxAt);
	
	/**
	**/
	UFUNCTION(Category="Helper Functions", CallInEditor, meta = (EditCondition = "!bSimHasBegun"))
	void Apply();

	/**
	**/
	void RouteUpdateHeatmap();
	
	/** 
	**/
	void PreUpdateHeatmap();

	/**
	**/
	void UpdateHeatmap();

	/**
	**/
	void PostUpdateHeatmap();

	/**
	**/
	void TraceBoxOwner(const FIntVector& BoxIndex, AActor* const BoxOwner, int& CurrentHitCount, ECollisionChannel Boxtype);

	/**
	**/
	void AggregateHitPoints(const FIntVector& BaseIndex, const AActor* BoxOwner, TArray<FIntVector>& AggregatorIndices, TArray<FVector>& AggregatedHitPoints);

	/**
	**/
	void Diffuse(TArray<float>& Field);

	/**
	**/
	UFUNCTION(BlueprintCallable, Category = "RtHeatmap")
	bool GetMapIndex(const FVector& WorldPos, FIntVector& OutMapIndex) const;

	/**
	**/
	UFUNCTION(BlueprintCallable, Category = "RtHeatmap")
	void GetMapWorldPos(const FIntVector& MapIndex, FVector& WorldPos) const;

	/**
	**/
	int CoreIndex(int i, int j, int k) const;

	/**
	**/
	int MapToCoreIndex(const FIntVector& MapIndex) const;

	/**
	**/
	bool IsWithinMap(const FIntVector& IndexToCheck) const;

	/**
	**/
	void GetBoxVertices(const FVector& Center, const FVector& Spacings, FBoxVertices& OutVertices);

	/**
	**/
	FVector RandRangeFromBoxVolume(const FVector& Center, const FVector& Spacings);

	/**
	**/
	FVector RandRangeFromBoxSurface(const FVector& Center, const FVector& Spacings);
	
	/**
	**/
	float BoxDiagLenth(const FVector& Spacings);

	UPROPERTY(EditAnywhere, BlueprintAssignable)
	FHeatBox_OnBodyHalfBurnt OnBodyHalfBurnt; 

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FVector UnitSpacings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true", AllowPrivateAccess = "true", EditCondition = "!bSimHasBegun"))
	int NumDepthCells;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true", AllowPrivateAccess = "true", EditCondition = "!bSimHasBegun"))
	int NumWidthCells;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true", AllowPrivateAccess = "true", EditCondition = "!bSimHasBegun"))
	int NumHeightCells;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", 
	ExposeOnSpawn = "true", AllowPrivateAccess = "true", EditCondition = "!bSimHasBegun"))
	float HeatTransferRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true", AllowPrivateAccess = "true", EditCondition = "!bSimHasBegun"))
	float UpdateInterval;

#if WITH_EDITORONLY_DATA	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	bool bShowTemperatureLog; 
#endif

#if WITH_EDITORONLY_DATA	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	bool bShowMiscLog;
#endif

#if WITH_EDITORONLY_DATA	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	bool bHeatDamageLog;
#endif

	UPROPERTY(VisibleAnywhere)
	bool bSimHasBegun;

	UPROPERTY(VisibleAnywhere)
	SimStage SimulationStage;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true", AllowPrivateAccess = "true", EditCondition = "!bSimHasBegun"))
	VisualVerbosity ShowHeatMap;
#endif

	UPROPERTY(VisibleAnywhere)
	TArray<float> HeatGenField;

	UPROPERTY()
	TArray<float> HeatGenField_Accumulator;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Effect", meta = (ExposeOnSpawn = "true", AllowPrivateAccess = "true"))
	TSubclassOf<AActor> FireEffectClass;
	
	UPROPERTY()
	FTimerHandle HeatmapTimer;
	
	UPROPERTY()
	USceneComponent* Root;

	UPROPERTY()
	UInstancedStaticMeshComponent* GraphViz;

	UPROPERTY()
	UMaterialInterface* VizColor;

	UPROPERTY(VisibleAnywhere)
	TMap<AActor*, FFlammableBoxInsts> FlamBoxInstsOf;

	UPROPERTY(VisibleAnywhere)
	TMap<AActor*, FBurningBoxInsts> BurnBoxInstsOf;

	UPROPERTY(VisibleAnywhere)
	TMap<FIntVector, FHeatBox> HeatBoxAt;
	
	UPROPERTY(VisibleAnywhere)
	TArray<AActor*> RegisteredBowOwners;

	TMultiMap<AActor*, TSharedPtr<FFireInBox>> GoingFires;

	TArray<TSharedPtr<FFireInBox>> OrphanedFires;

};

