#pragma once

// Storage of measurement data next to the project (the project file holds
// only the records' metadata; ProjectSerializer.h).
//
//   <project>.data/measurements/<experiment id>/<measurement id>.h5     (HDF5)
//   <project>.data/measurements/<experiment id>/<measurement id>.rmd/   (binary fallback)
//
// Every data set is created exclusively and made read-only when it is
// finished: a raw measurement is never overwritten, and processing writes
// new data sets. A stream that is closed without being finished (a cancelled
// or failed acquisition) still receives its metadata, marked incomplete, so
// what was recorded stays readable.
//
// HDF5 (BSD-style licence) is used when the build found it
// (RESONANT_HAVE_HDF5); the binary format is the same data as little-endian
// float64 files with a JSON metadata file.
#include "experiments/MeasurementRecord.h"
#include "foundation/Result.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace resonant::experiments {

enum class StoreFormat { Hdf5, Binary };
std::string_view toString(StoreFormat format);
bool hdf5Available();

// An append-only data set being recorded.
class MeasurementStream {
public:
    virtual ~MeasurementStream() = default;
    virtual Status append(const double* samples, size_t count) = 0;
    virtual Status appendComplex(const std::complex<double>* samples, size_t count) = 0;
    // Writes the final record (with its quality) and closes the data set read-only.
    virtual Status finish(const MeasurementRecord& record) = 0;
    virtual uint64_t written() const = 0;
    virtual uint64_t bytesWritten() const = 0;
    virtual const std::string& dataRef() const = 0;
};

class MeasurementStore {
public:
    virtual ~MeasurementStore() = default;
    virtual StoreFormat format() const = 0;
    const std::filesystem::path& root() const { return root_; }

    // The reference a record gets in this store.
    virtual std::string dataRefFor(const MeasurementRecord& record) const = 0;
    // Fails when the data set exists already.
    virtual Result<std::unique_ptr<MeasurementStream>> createStream(const MeasurementRecord& record, bool complex) = 0;
    virtual Status write(const MeasurementRecord& record, const MeasurementData& data) = 0;
    virtual Result<MeasurementData> read(const std::string& dataRef) const = 0;
    virtual Result<MeasurementRecord> readRecord(const std::string& dataRef) const = 0;
    virtual bool exists(const std::string& dataRef) const = 0;

protected:
    explicit MeasurementStore(std::filesystem::path root) : root_(std::move(root)) {}
    std::filesystem::path root_;
};

// HDF5 when available, unless another format is asked for.
Result<std::unique_ptr<MeasurementStore>> openMeasurementStore(const std::filesystem::path& root,
                                                               std::optional<StoreFormat> format = std::nullopt);
// <dir>/<stem>.data/measurements for a project file <dir>/<stem>.resonant.
std::filesystem::path measurementRootFor(const std::filesystem::path& projectFile);

}  // namespace resonant::experiments
