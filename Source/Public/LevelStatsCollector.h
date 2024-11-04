#pragma once

#include <CoreMinimal.h>

#include "LevelStatsCollector.generated.h"

UCLASS()
class MAPMETRICSGENERATION_API ALevelStatsCollector final : public AActor
{
    GENERATED_BODY()
public:
    void PostInitializeComponents() override;
};
