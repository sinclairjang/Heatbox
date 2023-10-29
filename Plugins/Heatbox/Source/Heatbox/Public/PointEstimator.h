// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
**/
class HEATBOX_API PointEstimator
{
public:
	PointEstimator()
	{}

	PointEstimator(const TArray<FVector>& Points);
	
	//Debug
	PointEstimator(const TArray<FVector>& Points, UWorld* World, float Ladder);
	
	/**
	**/
	FVector2D EstimateCenter2D() const;

	/**
	**/
	float EstimateArea2D() const;

	/** 
	**/
	float EstimateCenterPosZ() const;

private:
	/**
	**/
	void SortClockwise2D();

	/**
	**/
	void ConvexHull2D();

	/** 
	**/
	int CCW(const FVector& A, const FVector& B, const FVector& C) const;

private:
	TArray<FVector> EstimatePoints
	;
};
