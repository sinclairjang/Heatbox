// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FireBlob.generated.h"

class UNiagaraComponent;

UCLASS()
class HEATBOX_API AFireBlob : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFireBlob();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category="Fire Blob")
	void ShrinkToDeath();
	
	UFUNCTION()
	void PauseFireEffect();

	UFUNCTION()
	float GetTimeToBurnOut() const;

	UFUNCTION()
	float& SetTimeToBurnOut();

	UFUNCTION()
	void SetFireSize(float Size);

	UFUNCTION(BlueprintCallable, Category = "Fire Blob")
	uint8 GetIsPending();

	UFUNCTION(BlueprintCallable, Category = "Fire Blob")
	uint8 GetMarkForDestroy();



private:
	/*UPROPERTY(EditAnywhere)
	TObjectPtr<USceneComponent> Root;*/
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire Blob", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> NiagaraComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire Blob", meta = (AllowPrivateAccess = "true"))
	float BurnOutDelay;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Fire Blob", meta = (AllowPrivateAccess = "true"))
	uint8 IsPending:1;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Fire Blob", meta = (AllowPrivateAccess = "true"))
	uint8 MarkForDestroy:1;

};
