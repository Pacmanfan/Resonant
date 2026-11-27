#include "experiments/Acquisition.h"

#include "foundation/Time.h"

#include <cmath>
#include <limits>

namespace resonant::experiments {

bool LiveView::tryPush(const SampleBlock& block)
{
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock())
        return false;
    if (channels_.size() != block.channels)
        channels_.assign(block.channels, {});
    for (size_t f = 0; f < block.frames; ++f)
        for (size_t c = 0; c < block.channels; ++c) {
            const size_t i = f * block.channels + c;
            channels_[c].push_back(block.iq.empty() ? block.samples[i] : std::abs(block.iq[i]));
        }
    for (auto& ch : channels_)
        while (ch.size() > capacity_)
            ch.pop_front();
    lastFrame_ = block.firstFrame + block.frames;
    return true;
}

LiveView::Snapshot LiveView::snapshot() const
{
    std::lock_guard lock(mutex_);
    Snapshot s;
    s.lastFrame = lastFrame_;
    for (const auto& ch : channels_)
        s.channels.emplace_back(ch.begin(), ch.end());
    return s;
}

AcquisitionEngine::AcquisitionEngine(std::shared_ptr<HardwareDevice> device, AcquisitionPlan plan, MeasurementStore* store)
    : device_(std::move(device)), plan_(std::move(plan)), store_(store), live_(plan_.liveFrames)
{
}

AcquisitionEngine::~AcquisitionEngine()
{
    requestStop();
    if (reader_.joinable())
        reader_.join();
    if (recorder_.joinable())
        recorder_.join();
}

Status AcquisitionEngine::start()
{
    if (plan_.channels.empty())
        return Status::failure("the acquisition names no channel");
    if (plan_.queueBlocks < 2)
        return Status::failure("the acquisition buffer needs at least 2 blocks");
    const size_t n = plan_.channels.size();
    clipped_.assign(n, 0);
    clipRun_.assign(n, 0);
    saturated_.assign(n, false);
    peak_.assign(n, 0.0);
    output_.real.assign(n, {});
    output_.iq.assign(n, {});
    output_.quality.expectedFrames = plan_.frames;
    const std::string startUtc = utcNowIso8601();
    if (store_) {
        for (auto& c : plan_.channels) {
            c.record.dataRef = store_->dataRefFor(c.record);
            c.record.timeBase.startUtc = startUtc;
            auto stream = store_->createStream(c.record, plan_.complex);
            if (!stream)
                return Status::failure(stream.message());
            streams_.push_back(stream.take());
        }
    }
    auto session = device_->startAcquisition(plan_.frames);
    if (!session)
        return Status::failure(session.message());
    session_ = session.take();
    started_ = std::chrono::steady_clock::now();
    for (auto& c : plan_.channels)
        c.record.timeBase.startUtc = startUtc;
    output_.stats.queueCapacity = plan_.queueBlocks;
    output_.stats.state = "running";
    reader_ = std::thread([this] { readerLoop(); });
    recorder_ = std::thread([this] { recorderLoop(); });
    return Status::success();
}

void AcquisitionEngine::requestStop()
{
    stopRequested_.store(true);
    notFull_.notify_all();
    notEmpty_.notify_all();
}

bool AcquisitionEngine::wait(std::chrono::milliseconds timeout)
{
    std::unique_lock lock(mutex_);
    return done_.wait_for(lock, timeout, [this] { return finished_.load(); });
}

AcquisitionStats AcquisitionEngine::stats() const
{
    std::lock_guard lock(mutex_);
    AcquisitionStats s = output_.stats;
    s.queueOccupancy = queue_.size();
    s.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count();
    if (s.elapsedSeconds > 0.0)
        s.acquisitionRateHz = static_cast<double>(s.framesRead) / s.elapsedSeconds;
    return s;
}

void AcquisitionEngine::fail(const std::string& message)
{
    std::lock_guard lock(mutex_);
    output_.failed = true;
    if (output_.failure.empty())
        output_.failure = message;
    output_.quality.deviceErrors.push_back(message);
    output_.quality.complete = false;
}

void AcquisitionEngine::readerLoop()
{
    const auto timeout = std::chrono::milliseconds(static_cast<long>(plan_.readTimeoutSeconds * 1000.0));
    const double fs = device_->configuration().sampleRateHz;
    uint64_t expectedFrame = 0;
    bool first = true;
    double firstTimestamp = 0.0, lastTimestamp = -std::numeric_limits<double>::infinity();
    uint64_t firstFrame = 0;
    uint64_t total = 0;
    while (true) {
        if (stopRequested_.load()) {
            session_->stop();
            std::lock_guard lock(mutex_);
            output_.cancelled = true;
            output_.quality.complete = false;
            output_.quality.notes.push_back("acquisition stopped by request after " + std::to_string(total) + " frames");
            break;
        }
        auto block = session_->read(timeout);
        if (!block) {
            fail("device error: " + block.message());
            session_->stop();
            break;
        }
        SampleBlock b = block.take();
        if (b.frames == 0 && b.endOfStream)
            break;
        if (b.channels != plan_.channels.size() ||
            (plan_.complex ? b.iq.size() : b.samples.size()) != b.frames * b.channels) {
            fail("the device delivered a block of " + std::to_string(b.channels) + " channel(s) and " +
                 std::to_string(b.frames) + " frame(s) that does not match the configuration");
            session_->stop();
            break;
        }
        // Continuity of the frame counter, and overruns the device reports.
        uint64_t dropped = 0;
        {
            std::lock_guard lock(mutex_);
            if (first) {
                firstFrame = b.firstFrame;
                expectedFrame = b.firstFrame;
            }
            if (b.firstFrame > expectedFrame)
                dropped = b.firstFrame - expectedFrame;
            else if (b.firstFrame < expectedFrame) {
                ++output_.quality.invalidTimestamps;
                output_.quality.notes.push_back("frame counter went back from " + std::to_string(expectedFrame) + " to " +
                                                std::to_string(b.firstFrame));
            }
            dropped = std::max<uint64_t>(dropped, b.droppedBefore);
            if (dropped) {
                ++output_.quality.gaps;
                output_.quality.droppedFrames += dropped;
                output_.stats.framesDropped += dropped;
                ++output_.stats.gaps;
                output_.quality.notes.push_back("gap of " + std::to_string(dropped) + " frame(s) before frame " +
                                                std::to_string(b.firstFrame - firstFrame) + " (stored as NaN)");
            }
            // Timestamps.
            const double ts = b.hardwareTimestamp ? *b.hardwareTimestamp : b.softwareTimestamp;
            if (!std::isfinite(ts) || ts < lastTimestamp)
                ++output_.quality.invalidTimestamps;
            else if (b.hardwareTimestamp) {
                if (first)
                    firstTimestamp = ts;
                const double expected = firstTimestamp + static_cast<double>(b.firstFrame - firstFrame) / fs;
                if (std::abs(ts - expected) > std::max(2.0 / fs, 1e-6))
                    ++output_.quality.timestampDiscontinuities;
            }
            if (std::isfinite(ts))
                lastTimestamp = ts;
            ++output_.stats.blocksRead;
            output_.stats.framesRead += b.frames;
        }
        first = false;
        expectedFrame = b.firstFrame + b.frames;
        total += b.frames;
        b.droppedBefore = dropped;  // what the recorder fills with NaN
        const bool end = b.endOfStream || (plan_.frames > 0 && total >= plan_.frames);
        // Backpressure: wait for room; past the limit the block is counted as dropped.
        {
            std::unique_lock lock(mutex_);
            const bool room = notFull_.wait_for(lock, std::chrono::duration<double>(plan_.maxBackpressureSeconds),
                                                [this] { return queue_.size() < plan_.queueBlocks || stopRequested_.load(); });
            if (!room) {
                ++output_.quality.gaps;
                output_.quality.droppedFrames += b.frames;
                output_.stats.framesDropped += b.frames;
                output_.quality.notes.push_back("the recorder fell behind by more than " +
                                                std::to_string(plan_.maxBackpressureSeconds) + " s: " + std::to_string(b.frames) +
                                                " frame(s) discarded and counted");
            } else {
                queue_.push_back(std::move(b));
                output_.stats.maxQueueOccupancy = std::max(output_.stats.maxQueueOccupancy, queue_.size());
            }
        }
        notEmpty_.notify_one();
        if (end) {
            session_->stop();
            break;
        }
    }
    {
        std::lock_guard lock(mutex_);
        readerDone_ = true;
    }
    notEmpty_.notify_all();
}

void AcquisitionEngine::recorderLoop()
{
    const size_t n = plan_.channels.size();
    std::vector<double> column;
    std::vector<std::complex<double>> columnIq;
    uint64_t bytes = 0;
    while (true) {
        SampleBlock b;
        {
            std::unique_lock lock(mutex_);
            notEmpty_.wait(lock, [this] { return !queue_.empty() || readerDone_; });
            if (queue_.empty())
                break;
            b = std::move(queue_.front());
            queue_.pop_front();
        }
        notFull_.notify_one();
        // Gap fill (NaN), then the block, per channel.
        for (size_t c = 0; c < n; ++c) {
            const double limit = plan_.clipFraction * std::max(std::abs(plan_.channels[c].rangeMin), std::abs(plan_.channels[c].rangeMax));
            if (plan_.complex) {
                columnIq.assign(b.droppedBefore, {std::nan(""), std::nan("")});
                for (size_t f = 0; f < b.frames; ++f) {
                    const auto v = b.iq[f * n + c];
                    columnIq.push_back(v);
                    const double a = std::max(std::abs(v.real()), std::abs(v.imag()));
                    peak_[c] = std::max(peak_[c], a);
                    if (a >= limit) {
                        ++clipped_[c];
                        if (++clipRun_[c] >= plan_.saturationRun)
                            saturated_[c] = true;
                    } else
                        clipRun_[c] = 0;
                }
                if (!streams_.empty())
                    if (Status ok = streams_[c]->appendComplex(columnIq.data(), columnIq.size()); !ok)
                        fail(ok.message());
                if (plan_.keepInMemory)
                    output_.iq[c].insert(output_.iq[c].end(), columnIq.begin(), columnIq.end());
                bytes += columnIq.size() * 16;
            } else {
                column.assign(b.droppedBefore, std::nan(""));
                for (size_t f = 0; f < b.frames; ++f) {
                    const double v = b.samples[f * n + c];
                    column.push_back(v);
                    peak_[c] = std::max(peak_[c], std::abs(v));
                    if (std::abs(v) >= limit) {
                        ++clipped_[c];
                        if (++clipRun_[c] >= plan_.saturationRun)
                            saturated_[c] = true;
                    } else
                        clipRun_[c] = 0;
                }
                if (!streams_.empty())
                    if (Status ok = streams_[c]->append(column.data(), column.size()); !ok)
                        fail(ok.message());
                if (plan_.keepInMemory)
                    output_.real[c].insert(output_.real[c].end(), column.begin(), column.end());
                bytes += column.size() * 8;
            }
        }
        if (!live_.tryPush(b)) {
            std::lock_guard lock(mutex_);
            ++output_.stats.liveUpdatesSkipped;
        }
        std::lock_guard lock(mutex_);
        output_.stats.framesRecorded += b.frames + b.droppedBefore;
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count();
        output_.stats.diskBytesPerSecond = (elapsed > 0.0 && !streams_.empty()) ? static_cast<double>(bytes) / elapsed : 0.0;
    }
    // Close the data sets with the final records.
    {
        std::lock_guard lock(mutex_);
        output_.quality.recordedFrames = output_.stats.framesRecorded;
        if (plan_.frames > 0 && output_.stats.framesRecorded < plan_.frames)
            output_.quality.complete = false;
        for (size_t c = 0; c < n; ++c) {
            MeasurementRecord r = plan_.channels[c].record;
            r.quality = output_.quality;
            r.quality.clippedSamples = clipped_[c];
            r.quality.saturated = saturated_[c];
            r.quality.peakAbsolute = peak_[c];
            r.shape = {static_cast<size_t>(output_.stats.framesRecorded)};
            output_.quality.clippedSamples += clipped_[c];
            output_.quality.saturated = output_.quality.saturated || saturated_[c];
            output_.quality.peakAbsolute = std::max(output_.quality.peakAbsolute, peak_[c]);
            if (!streams_.empty()) {
                r.dataRef = streams_[c]->dataRef();
                if (Status ok = streams_[c]->finish(r); !ok) {
                    output_.failed = true;
                    output_.failure = ok.message();
                }
            }
            output_.records.push_back(r);
        }
        output_.stats.state = output_.failed ? "failed" : output_.cancelled ? "cancelled" : "finished";
        output_.stats.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count();
        streams_.clear();
        finished_.store(true);
    }
    done_.notify_all();
}

AcquisitionOutput AcquisitionEngine::takeOutput()
{
    if (reader_.joinable())
        reader_.join();
    if (recorder_.joinable())
        recorder_.join();
    std::lock_guard lock(mutex_);
    return std::move(output_);
}

}  // namespace resonant::experiments
