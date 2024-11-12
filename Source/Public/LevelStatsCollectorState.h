#pragma once

#include <CoreMinimal.h>

class FPerformanceMetricsCapture;
class ALevelStatsCollector;

class FLevelStatsCollectorState
{
public:
    explicit FLevelStatsCollectorState( ALevelStatsCollector * collector );
    virtual ~FLevelStatsCollectorState() = default;

    virtual void Enter();
    virtual void Tick( float delta_time ) = 0;
    virtual void Exit();

protected:
    ALevelStatsCollector * Collector;
};

class FIdleState final : public FLevelStatsCollectorState
{
public:
    explicit FIdleState( ALevelStatsCollector * collector );

    void Enter() override;
    void Tick( float delta_time ) override;
    void Exit() override;

private:
    float CurrentDelay;
};

class FWaitingForSnapshotState final : public FLevelStatsCollectorState
{
public:
    explicit FWaitingForSnapshotState( ALevelStatsCollector * collector );

    void Enter() override;
    void Tick( float delta_time ) override;
    void Exit() override;

private:
    float CurrentDelay;
    USceneCaptureComponent2D * CaptureComponent;
    int32 CurrentCellIndex;
};

class FProcessingNextRotationState final : public FLevelStatsCollectorState
{
public:
    void Enter() override;
    explicit FProcessingNextRotationState( ALevelStatsCollector * collector );
    void Tick( float delta_time ) override;
    void Exit() override;

private:
    float CurrentRotation;
};

class FProcessingNextCellState final : public FLevelStatsCollectorState
{
public:
    explicit FProcessingNextCellState( ALevelStatsCollector * collector );

    void Enter() override;
    void Tick( float delta_time ) override;
    void Exit() override;
};

class FCapturingMetricsState final : public FLevelStatsCollectorState
{
public:
    explicit FCapturingMetricsState( ALevelStatsCollector * collector );

    void Enter() override;
    void Tick( float delta_time ) override;
    void Exit() override;

private:
    TSharedPtr< FPerformanceMetricsCapture > CurrentPerformanceChart;
    float CurrentCaptureTime;
    int32 CurrentCellIndex;
    float CurrentRotation;
};