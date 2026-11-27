#pragma once

// The acquisition engine: reads a device's input stream on one thread,
// records it on another, and keeps a live view for displays.
//
//   device --(reader thread)--> bounded queue --(recorder thread)--> store
//                                                     |--> memory (optional)
//                                                     '--> live view (never blocks)
//
// Backpressure: when the queue is full the reader waits (the device then
// buffers, and reports an overrun if its own buffer fills). If the
// recorder stays behind for longer than the plan allows, the block is
// counted as dropped and the gap recorded - never silently discarded.
//
// Integrity: frame-counter gaps, device-reported overruns, timestamp
// discontinuities and invalid timestamps, clipping and saturation, and
// device errors are detected and written into the records' quality. Missing
// frames are stored as NaN so every stored sample keeps its time.
//
// The display reads the live view with a short copy; the recorder skips a
// live-view update rather than wait for a display. Visualization can never
// slow down acquisition.
#include "experiments/Hardware.h"
#include "experiments/MeasurementRecord.h"
#include "experiments/MeasurementStore.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace resonant::experiments {

struct ChannelPlan {
    MeasurementRecord record;  // the raw record's identity and metadata (id, binding, sensor, quantity, pose, ...)
    double rangeMin = -1.0, rangeMax = 1.0;
};

struct AcquisitionPlan {
    size_t frames = 0;                 // 0 = until stopped
    size_t queueBlocks = 64;
    double maxBackpressureSeconds = 2.0;
    double readTimeoutSeconds = 2.0;
    double clipFraction = 0.999;
    size_t saturationRun = 3;          // consecutive clipped samples that count as saturation
    bool complex = false;
    bool keepInMemory = true;
    size_t liveFrames = 8192;          // per channel
    std::vector<ChannelPlan> channels; // in the device's block order
};

struct AcquisitionStats {
    uint64_t blocksRead = 0, framesRead = 0, framesRecorded = 0, framesDropped = 0, gaps = 0;
    size_t queueOccupancy = 0, queueCapacity = 0, maxQueueOccupancy = 0;
    double elapsedSeconds = 0.0;
    double acquisitionRateHz = 0.0;     // frames per second actually read
    double diskBytesPerSecond = 0.0;
    uint64_t liveUpdatesSkipped = 0;    // the display was busy; acquisition went on
    std::string state;                  // running, finished, cancelled, failed
};

class LiveView {
public:
    explicit LiveView(size_t framesPerChannel = 8192) : capacity_(framesPerChannel) {}
    struct Snapshot {
        std::vector<std::vector<double>> channels;  // oldest first; magnitudes for I/Q
        uint64_t lastFrame = 0;
    };
    // Writer side: does nothing when a reader holds the lock.
    bool tryPush(const SampleBlock& block);
    Snapshot snapshot() const;

private:
    size_t capacity_;
    mutable std::mutex mutex_;
    std::vector<std::deque<double>> channels_;
    uint64_t lastFrame_ = 0;
};

struct AcquisitionOutput {
    std::vector<MeasurementRecord> records;   // one raw record per channel
    std::vector<std::vector<double>> real;    // in memory, per channel (if kept)
    std::vector<std::vector<std::complex<double>>> iq;
    DataQuality quality;                      // the stream as a whole
    AcquisitionStats stats;
    bool cancelled = false;
    bool failed = false;
    std::string failure;
};

class AcquisitionEngine {
public:
    AcquisitionEngine(std::shared_ptr<HardwareDevice> device, AcquisitionPlan plan, MeasurementStore* store);
    ~AcquisitionEngine();
    AcquisitionEngine(const AcquisitionEngine&) = delete;
    AcquisitionEngine& operator=(const AcquisitionEngine&) = delete;

    Status start();
    // Cancellation: the device stops, everything read so far is recorded and
    // the data sets are closed as incomplete.
    void requestStop();
    bool wait(std::chrono::milliseconds timeout);
    bool finished() const { return finished_.load(); }
    AcquisitionStats stats() const;
    const LiveView& live() const { return live_; }
    // After finished(): the records and data.
    AcquisitionOutput takeOutput();

private:
    void readerLoop();
    void recorderLoop();
    void fail(const std::string& message);

    std::shared_ptr<HardwareDevice> device_;
    AcquisitionPlan plan_;
    MeasurementStore* store_;
    std::unique_ptr<AcquisitionSession> session_;
    std::vector<std::unique_ptr<MeasurementStream>> streams_;

    mutable std::mutex mutex_;
    std::condition_variable notEmpty_, notFull_, done_;
    std::deque<SampleBlock> queue_;
    bool closed_ = false;
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> finished_{false};
    bool readerDone_ = false;

    AcquisitionOutput output_;
    std::vector<uint64_t> clipped_;
    std::vector<size_t> clipRun_;
    std::vector<bool> saturated_;
    std::vector<double> peak_;
    std::chrono::steady_clock::time_point started_;
    LiveView live_;
    std::thread reader_, recorder_;
};

}  // namespace resonant::experiments
