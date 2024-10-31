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
        float CellSize;
        FVector GridOffset;
        float CameraHeight;
        float CameraHeightOffset;
        float CameraRotationDelta;
        FString ScreenshotPattern;
    };

    bool ParseParams( const FString & ParamsStr, FMetricsParams & OutParams ) const;
};
