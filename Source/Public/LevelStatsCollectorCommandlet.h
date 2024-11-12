#pragma once

#include <Commandlets/Commandlet.h>

#include "LevelStatsCollectorCommandlet.generated.h"

UCLASS( CustomConstructor )
class MAPMETRICSGENERATION_API ULevelStatsCollectorCommandlet final : public UCommandlet
{
    GENERATED_BODY()
public:
    ULevelStatsCollectorCommandlet();
    int32 Main( const FString & params ) override;

private:
    struct FMetricsParams
    {
        FString MapName;
        float CellSize;
        FVector GridOffset;
        float CameraHeight;
        float CameraHeightOffset;
        float CameraRotationDelta;
        float CameraFOVAngle;
        FString ScreenshotPattern;
    };

    bool RunLevelStatsCommandlet( const FString & package_name, const FMetricsParams & metrics_params ) const;
    bool ParseParams( const FString & params, FMetricsParams & out_params, TMap< FString, FString > & params_map ) const;
};
