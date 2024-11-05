#pragma once

#include <Commandlets/Commandlet.h>

#include "PerfGrapherCommandlet.generated.h"

UCLASS( CustomConstructor )
class MAPMETRICSGENERATION_API UPerfGrapherCommandlet final : public UCommandlet
{
    GENERATED_BODY()
public:
    UPerfGrapherCommandlet();
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

    bool RunPerfGrapher( const FString & package_name, const FMetricsParams & metrics_params ) const;
    bool ParseParams( const FString & params, FMetricsParams & out_params, TMap< FString, FString > & params_map ) const;
};
