// Fill out your copyright notice in the Description page of Project Settings.


#include "PointEstimator.h"

#include <algorithm>
#include <stack>

using namespace std;

PointEstimator::PointEstimator(const TArray<FVector>& Points)
	: EstimatePoints(Points) {
	SortClockwise2D();
	ConvexHull2D();
}

PointEstimator::PointEstimator(const TArray<FVector>& Points, UWorld* World, float Ladder)
	: EstimatePoints(Points) {
	SortClockwise2D();
	for (auto EstimatePoint : EstimatePoints) {
		DrawDebugPoint(World, EstimatePoint, 10.f, FColor::Cyan, false, 1.);
		//UE_LOG(LogTemp, Warning, TEXT("%s"), *EstimatePoint.ToString());
	}

	ConvexHull2D();
	for (auto EstimatePoint : EstimatePoints) {
		DrawDebugPoint(World, EstimatePoint, 10.f, FColor::Red, false, 1.);
		//UE_LOG(LogTemp, Warning, TEXT("%s"), *EstimatePoint.ToString());
	}

	float CenterPosZ = EstimateCenterPosZ();

	FVector EstimatedCenter(EstimateCenter2D(), CenterPosZ);
	DrawDebugPoint(World, EstimatedCenter, 10.f, FColor::Green, false, 1.);
	//UE_LOG(LogTemp, Warning, TEXT("RadiationArea: %f"), EstimateArea2D()); 

}

FVector2D PointEstimator::EstimateCenter2D() const
{
	switch (EstimatePoints.Num())
	{
	case 0:
		check(0);

	case 1:
		return { EstimatePoints[0].X, EstimatePoints[0].Y };

	case 2:
		return { (EstimatePoints[0].X + EstimatePoints[1].X) / 2, (EstimatePoints[0].Y + EstimatePoints[1].Y) / 2 };

	default:
		FVector2D Centroid = { 0., 0. };
		float SignedArea = 0.;
		float X0 = 0.;
		float Y0 = 0.;
		float X1 = 0.;
		float Y1 = 0.;
		float CumulativeArea = 0.;

		int i = 0;
		for (i = 0; i < EstimatePoints.Num() - 1; ++i)
		{
			X0 = EstimatePoints[i].X;
			Y0 = EstimatePoints[i].Y;
			X1 = EstimatePoints[i + 1].X;
			Y1 = EstimatePoints[i + 1].Y;
			CumulativeArea = X0 * Y1 - X1 * Y0;
			SignedArea += CumulativeArea;
			Centroid.X += (X0 + X1) * CumulativeArea;
			Centroid.Y += (Y0 + Y1) * CumulativeArea;
		}

		X0 = EstimatePoints[i].X;
		Y0 = EstimatePoints[i].Y;
		X1 = EstimatePoints[0].X;
		Y1 = EstimatePoints[0].Y;
		CumulativeArea = X0 * Y1 - X1 * Y0;
		SignedArea += CumulativeArea;
		Centroid.X += (X0 + X1) * CumulativeArea;
		Centroid.Y += (Y0 + Y1) * CumulativeArea;

		SignedArea *= 0.5;
		Centroid.X /= (6. * SignedArea);
		Centroid.Y /= (6. * SignedArea);

		return Centroid;
	}

}

float PointEstimator::EstimateArea2D() const
{
	switch (EstimatePoints.Num())
	{
	case 0:
		check(0);
		return 0;

	case 1:
		return 0;

	case 2:
		return 0;

	default:
		float Area = 0.;
		for (int i = 0; i < EstimatePoints.Num(); ++i) {
			int j = (i + 1) % EstimatePoints.Num();
			Area += EstimatePoints[i].X * EstimatePoints[j].Y;
			Area -= EstimatePoints[i].Y * EstimatePoints[j].X;
		}

		Area /= 2;
		return (Area < 0. ? -Area : Area);
	}

}

float PointEstimator::EstimateCenterPosZ() const
{
	if (EstimatePoints.Num() > 0) {
		float EstimateCenterPosZ = 0;
		TArray<float> Temp;
		
		for (auto EstimatePoint : EstimatePoints) {
			Temp.Add(EstimatePoint.Z);
			//EstimateCenterPosZ += EstimatePoint.Z;
		}
		
		Temp.Sort();
		EstimateCenterPosZ = Temp[0];
		//EstimateCenterPosZ /= EstimatePoints.Num();
		
		return EstimateCenterPosZ;
	}

	else {
		check(0);
		return 0;
	}
}

void PointEstimator::SortClockwise2D()
{
	EstimatePoints.Sort([](const FVector& A, const FVector& B) {
		if (A.X != B.X)
			return A.X < B.X;
		return A.Y < B.Y;
		});

	FVector Control = EstimatePoints[0];
	EstimatePoints.RemoveAt(0);

	EstimatePoints.Sort([Control](const FVector& A, const FVector& B) {
		FVector RelA;
		RelA.X = A.X - Control.X;
		RelA.Y = A.Y - Control.Y;

		FVector RelB;
		RelB.X = B.X - Control.X;
		RelB.Y = B.Y - Control.Y;
		return RelA.X * RelB.Y - RelA.Y * RelB.X > 0;
		});

	EstimatePoints.Insert(Control, 0);
}

void PointEstimator::ConvexHull2D()
{
	stack<int> HullStack;

	HullStack.push(0);
	HullStack.push(1);

	int Next = 2;

	while (Next < EstimatePoints.Num()) {
		while (HullStack.size() >= 2) {
			int First, Second;
			Second = HullStack.top();
			HullStack.pop();
			First = HullStack.top();

			if (CCW(EstimatePoints[First], EstimatePoints[Second], EstimatePoints[Next]) > 0) {
				HullStack.push(Second);
				break;
			}
		}

		HullStack.push(Next++);
	}

	TArray<FVector> Hull;

	int Size = HullStack.size();
	for (int i = 0; i < Size; i++)
	{
		int Top = HullStack.top();
		Hull.Add(EstimatePoints[Top]);
		HullStack.pop();
	};

	EstimatePoints.Reset();
	EstimatePoints = Hull;
	
	//for (auto CVHPoint : CVHPoints) {
	//	UE_LOG(LogTemp, Warning, TEXT("%s"), *CVHPoint.ToString());
	//}
}

// [왼손 좌표]
// 시계 방향	: 양수
// 직선		: 0
// 반시계 방향	: 음수
int PointEstimator::CCW(const FVector& A, const FVector& B, const FVector& C) const
{
	return A.X * B.Y + B.X * C.Y + C.X * A.Y - B.X * A.Y - C.X * B.Y - A.X * C.Y;
}