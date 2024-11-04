#pragma once

#include <CoreMinimal.h>

#include "LevelStatsCollector.generated.h"

UCLASS()
class MAPMETRICSGENERATION_API ALevelStatsCollector final : public AActor
{
    GENERATED_BODY()

public:
    ALevelStatsCollector();

    void PostInitializeComponents() override;
    void BeginPlay() override;
    void Tick( float DeltaTime ) override;

private:
    void SetupSceneCapture() const;
    void ProcessNextCell();
    void CaptureCurrentView();
    bool TraceGroundPosition( const FVector & start_location, FVector & outhit_location ) const;
    void CalculateGridBounds();
    bool MoveToNextPosition();

    UPROPERTY()
    USceneCaptureComponent2D * CaptureComponent;

public:
    UPROPERTY()
    float CellSize = 1000.0f;

    UPROPERTY()
    FVector GridOffset = FVector::ZeroVector;

    UPROPERTY()
    float CameraHeight = 10000.0f;

    UPROPERTY()
    float CameraHeightOffset = 170.0f;

    UPROPERTY()
    float CameraRotationDelta = 90.0f;

private:
    FVector CurrentCell;
    float CurrentRotation;
    FVector GridMin;
    FVector GridMax;
    int32 CurrentCaptureCount;
    bool bIsCapturing;
};
