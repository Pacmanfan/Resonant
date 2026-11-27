#include "experiments/MeasurementStore.h"

#include <bit>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>
#include <system_error>

#ifdef RESONANT_HAVE_HDF5
#include <hdf5.h>
#endif

namespace fs = std::filesystem;

namespace resonant::experiments {

namespace {

Status validRef(const std::string& ref)
{
    if (ref.empty() || ref.find("..") != std::string::npos || ref.front() == '/')
        return Status::failure("invalid measurement data reference '" + ref + "'");
    return Status::success();
}

void makeReadOnly(const fs::path& path)
{
    std::error_code ignored;
    if (fs::is_directory(path, ignored)) {
        for (const auto& entry : fs::directory_iterator(path, ignored))
            fs::permissions(entry.path(), fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read,
                            fs::perm_options::replace, ignored);
        fs::permissions(path,
                        fs::perms::owner_read | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
                            fs::perms::others_read | fs::perms::others_exec,
                        fs::perm_options::replace, ignored);
    } else {
        fs::permissions(path, fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read, fs::perm_options::replace,
                        ignored);
    }
}

std::string incompleteMetadata(MeasurementRecord record)
{
    record.quality.complete = false;
    record.quality.notes.push_back("the recording was closed without being finished (cancelled or failed); the data written "
                                   "up to that point is kept");
    return toJson(record).dump(1);
}

// ---------------------------------------------------------------- binary format

class BinaryStream final : public MeasurementStream {
public:
    BinaryStream(fs::path directory, std::string ref, MeasurementRecord initial, bool complex)
        : directory_(std::move(directory)), ref_(std::move(ref)), initial_(std::move(initial)), complex_(complex),
          out_(directory_ / (complex ? "complex.f64" : "real.f64"), std::ios::binary)
    {
    }
    ~BinaryStream() override
    {
        if (!finished_) {
            out_.close();
            std::ofstream(directory_ / "metadata.json") << incompleteMetadata(initial_);
            makeReadOnly(directory_);
        }
    }
    Status append(const double* samples, size_t count) override
    {
        if (complex_)
            return Status::failure("this data set is complex");
        out_.write(reinterpret_cast<const char*>(samples), static_cast<std::streamsize>(count * sizeof(double)));
        written_ += count;
        return out_ ? Status::success() : Status::failure("write failed: " + (directory_ / "real.f64").string());
    }
    Status appendComplex(const std::complex<double>* samples, size_t count) override
    {
        if (!complex_)
            return Status::failure("this data set is real");
        out_.write(reinterpret_cast<const char*>(samples), static_cast<std::streamsize>(count * sizeof(std::complex<double>)));
        written_ += count;
        return out_ ? Status::success() : Status::failure("write failed: " + (directory_ / "complex.f64").string());
    }
    Status finish(const MeasurementRecord& record) override
    {
        if (finished_)
            return Status::failure("the data set is finished already");
        out_.close();
        MeasurementRecord r = record;
        r.dataRef = ref_;
        std::ofstream meta(directory_ / "metadata.json");
        meta << toJson(r).dump(1);
        meta.close();
        finished_ = true;
        makeReadOnly(directory_);
        return meta ? Status::success() : Status::failure("could not write the metadata of " + ref_);
    }
    uint64_t written() const override { return written_; }
    uint64_t bytesWritten() const override { return written_ * (complex_ ? 16 : 8); }
    const std::string& dataRef() const override { return ref_; }

private:
    fs::path directory_;
    std::string ref_;
    MeasurementRecord initial_;
    bool complex_;
    std::ofstream out_;
    uint64_t written_ = 0;
    bool finished_ = false;
};

template <typename T>
bool readVector(const fs::path& path, std::vector<T>& out)
{
    std::error_code ec;
    if (!fs::exists(path, ec))
        return true;
    const auto size = fs::file_size(path, ec);
    if (ec || size % sizeof(T) != 0)
        return false;
    out.resize(size / sizeof(T));
    std::ifstream in(path, std::ios::binary);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
    return static_cast<bool>(in);
}

template <typename T>
bool writeVector(const fs::path& path, const std::vector<T>& data)
{
    if (data.empty())
        return true;
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size() * sizeof(T)));
    return static_cast<bool>(out);
}

class BinaryStore final : public MeasurementStore {
public:
    explicit BinaryStore(fs::path root) : MeasurementStore(std::move(root)) {}
    StoreFormat format() const override { return StoreFormat::Binary; }
    std::string dataRefFor(const MeasurementRecord& r) const override { return r.experimentId + "/" + r.id + ".rmd"; }

    Result<fs::path> createDirectory(const std::string& ref)
    {
        if (Status ok = validRef(ref); !ok)
            return Result<fs::path>::failure(ok.message());
        const fs::path dir = root_ / ref;
        std::error_code ec;
        fs::create_directories(dir.parent_path(), ec);
        if (!fs::create_directory(dir, ec))
            return Result<fs::path>::failure("the measurement data set " + ref + " exists already (raw data is never overwritten)" +
                                             (ec ? ": " + ec.message() : ""));
        return dir;
    }

    Result<std::unique_ptr<MeasurementStream>> createStream(const MeasurementRecord& record, bool complex) override
    {
        if (std::endian::native != std::endian::little)
            return Result<std::unique_ptr<MeasurementStream>>::failure("the binary measurement format needs a little-endian host");
        const std::string ref = dataRefFor(record);
        auto dir = createDirectory(ref);
        if (!dir)
            return Result<std::unique_ptr<MeasurementStream>>::failure(dir.message());
        return std::unique_ptr<MeasurementStream>(std::make_unique<BinaryStream>(dir.value(), ref, record, complex));
    }

    Status write(const MeasurementRecord& record, const MeasurementData& data) override
    {
        const std::string ref = dataRefFor(record);
        auto dir = createDirectory(ref);
        if (!dir)
            return Status::failure(dir.message());
        MeasurementRecord r = record;
        r.dataRef = ref;
        nlohmann::json meta = toJson(r);
        meta["labels"] = data.labels;
        bool ok = writeVector(dir.value() / "real.f64", data.real) && writeVector(dir.value() / "complex.f64", data.complex) &&
                  writeVector(dir.value() / "axis.f64", data.axis) && writeVector(dir.value() / "auxiliary.f64", data.auxiliary);
        std::ofstream(dir.value() / "metadata.json") << meta.dump(1);
        makeReadOnly(dir.value());
        return ok ? Status::success() : Status::failure("could not write the data set " + ref);
    }

    Result<MeasurementData> read(const std::string& ref) const override
    {
        if (Status ok = validRef(ref); !ok)
            return Result<MeasurementData>::failure(ok.message());
        const fs::path dir = root_ / ref;
        MeasurementData d;
        if (!fs::is_directory(dir))
            return Result<MeasurementData>::failure("the measurement data set " + ref + " is missing from " + root_.string());
        if (!readVector(dir / "real.f64", d.real) || !readVector(dir / "complex.f64", d.complex) ||
            !readVector(dir / "axis.f64", d.axis) || !readVector(dir / "auxiliary.f64", d.auxiliary))
            return Result<MeasurementData>::failure("the measurement data set " + ref + " is damaged (a file size is not a whole "
                                                    "number of values)");
        std::ifstream meta(dir / "metadata.json");
        try {
            const auto j = nlohmann::json::parse(meta);
            d.labels = j.value("labels", std::vector<std::string>{});
        } catch (...) {
        }
        return d;
    }

    Result<MeasurementRecord> readRecord(const std::string& ref) const override
    {
        if (Status ok = validRef(ref); !ok)
            return Result<MeasurementRecord>::failure(ok.message());
        std::ifstream meta(root_ / ref / "metadata.json");
        if (!meta)
            return Result<MeasurementRecord>::failure("the metadata of " + ref + " is missing");
        try {
            return measurementRecordFromJson(nlohmann::json::parse(meta));
        } catch (const std::exception& e) {
            return Result<MeasurementRecord>::failure("the metadata of " + ref + " is not valid JSON: " + e.what());
        }
    }

    bool exists(const std::string& ref) const override { return validRef(ref) && fs::exists(root_ / ref); }
};

// ---------------------------------------------------------------- HDF5

#ifdef RESONANT_HAVE_HDF5

// The serial HDF5 library is not thread-safe: every call goes through this lock.
std::mutex& hdf5Mutex()
{
    static std::mutex m;
    return m;
}

void quietHdf5()
{
    static std::once_flag once;
    std::call_once(once, [] { H5Eset_auto2(H5E_DEFAULT, nullptr, nullptr); });
}

Status writeBytes(hid_t file, const char* name, const std::string& text)
{
    const hsize_t dims[1] = {text.size()};
    const hid_t space = H5Screate_simple(1, dims, nullptr);
    const hid_t set = H5Dcreate2(file, name, H5T_NATIVE_UCHAR, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    herr_t status = -1;
    if (set >= 0) {
        status = text.empty() ? 0 : H5Dwrite(set, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, text.data());
        H5Dclose(set);
    }
    H5Sclose(space);
    return status >= 0 ? Status::success() : Status::failure(std::string("could not write the HDF5 data set ") + name);
}

Status writeDoubles(hid_t file, const char* name, const double* data, hsize_t rows, hsize_t columns)
{
    if (rows == 0)
        return Status::success();
    const hsize_t dims[2] = {rows, columns};
    const hid_t space = H5Screate_simple(columns == 1 ? 1 : 2, dims, nullptr);
    const hid_t set = H5Dcreate2(file, name, H5T_NATIVE_DOUBLE, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    herr_t status = -1;
    if (set >= 0) {
        status = H5Dwrite(set, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);
        H5Dclose(set);
    }
    H5Sclose(space);
    return status >= 0 ? Status::success() : Status::failure(std::string("could not write the HDF5 data set ") + name);
}

bool readDoubles(hid_t file, const char* name, std::vector<double>& out, hsize_t& rows, hsize_t& columns)
{
    rows = columns = 0;
    if (H5Lexists(file, name, H5P_DEFAULT) <= 0)
        return true;
    const hid_t set = H5Dopen2(file, name, H5P_DEFAULT);
    if (set < 0)
        return false;
    const hid_t space = H5Dget_space(set);
    hsize_t dims[2] = {0, 1};
    const int rank = H5Sget_simple_extent_dims(space, dims, nullptr);
    rows = dims[0];
    columns = rank == 2 ? dims[1] : 1;
    out.resize(static_cast<size_t>(rows * columns));
    const herr_t status = out.empty() ? 0 : H5Dread(set, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, out.data());
    H5Sclose(space);
    H5Dclose(set);
    return status >= 0;
}

bool readBytes(hid_t file, const char* name, std::string& out)
{
    if (H5Lexists(file, name, H5P_DEFAULT) <= 0)
        return false;
    const hid_t set = H5Dopen2(file, name, H5P_DEFAULT);
    if (set < 0)
        return false;
    const hid_t space = H5Dget_space(set);
    hsize_t dims[1] = {0};
    H5Sget_simple_extent_dims(space, dims, nullptr);
    out.resize(static_cast<size_t>(dims[0]));
    const herr_t status = out.empty() ? 0 : H5Dread(set, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, out.data());
    H5Sclose(space);
    H5Dclose(set);
    return status >= 0;
}

class Hdf5Stream final : public MeasurementStream {
public:
    Hdf5Stream(hid_t file, hid_t set, fs::path path, std::string ref, MeasurementRecord initial, bool complex)
        : file_(file), set_(set), path_(std::move(path)), ref_(std::move(ref)), initial_(std::move(initial)), complex_(complex)
    {
    }
    ~Hdf5Stream() override
    {
        if (!finished_) {
            std::lock_guard lock(hdf5Mutex());
            (void)writeBytes(file_, "metadata", incompleteMetadata(initial_));
            close();
            makeReadOnly(path_);
        }
    }
    Status append(const double* samples, size_t count) override
    {
        if (complex_)
            return Status::failure("this data set is complex");
        return extend(samples, count);
    }
    Status appendComplex(const std::complex<double>* samples, size_t count) override
    {
        if (!complex_)
            return Status::failure("this data set is real");
        return extend(reinterpret_cast<const double*>(samples), count);
    }
    Status finish(const MeasurementRecord& record) override
    {
        if (finished_)
            return Status::failure("the data set is finished already");
        MeasurementRecord r = record;
        r.dataRef = ref_;
        std::lock_guard lock(hdf5Mutex());
        Status ok = writeBytes(file_, "metadata", toJson(r).dump(1));
        close();
        finished_ = true;
        makeReadOnly(path_);
        return ok;
    }
    uint64_t written() const override { return written_; }
    uint64_t bytesWritten() const override { return written_ * (complex_ ? 16 : 8); }
    const std::string& dataRef() const override { return ref_; }

private:
    Status extend(const double* data, size_t count)
    {
        if (count == 0)
            return Status::success();
        std::lock_guard lock(hdf5Mutex());
        const int rank = complex_ ? 2 : 1;
        const hsize_t newSize[2] = {written_ + count, 2};
        if (H5Dset_extent(set_, newSize) < 0)
            return Status::failure("could not extend " + ref_);
        const hid_t fileSpace = H5Dget_space(set_);
        const hsize_t offset[2] = {written_, 0};
        const hsize_t extent[2] = {count, 2};
        H5Sselect_hyperslab(fileSpace, H5S_SELECT_SET, offset, nullptr, extent, nullptr);
        const hid_t memSpace = H5Screate_simple(rank, extent, nullptr);
        const herr_t status = H5Dwrite(set_, H5T_NATIVE_DOUBLE, memSpace, fileSpace, H5P_DEFAULT, data);
        H5Sclose(memSpace);
        H5Sclose(fileSpace);
        if (status < 0)
            return Status::failure("could not write to " + ref_);
        written_ += count;
        if (++appends_ % 32 == 0)
            H5Fflush(file_, H5F_SCOPE_LOCAL);  // what is recorded survives a crash
        return Status::success();
    }
    void close()
    {
        if (set_ >= 0)
            H5Dclose(set_);
        if (file_ >= 0)
            H5Fclose(file_);
        set_ = file_ = -1;
    }

    hid_t file_, set_;
    fs::path path_;
    std::string ref_;
    MeasurementRecord initial_;
    bool complex_;
    uint64_t written_ = 0;
    uint64_t appends_ = 0;
    bool finished_ = false;
};

class Hdf5Store final : public MeasurementStore {
public:
    explicit Hdf5Store(fs::path root) : MeasurementStore(std::move(root)) { quietHdf5(); }
    StoreFormat format() const override { return StoreFormat::Hdf5; }
    std::string dataRefFor(const MeasurementRecord& r) const override { return r.experimentId + "/" + r.id + ".h5"; }

    Result<std::unique_ptr<MeasurementStream>> createStream(const MeasurementRecord& record, bool complex) override
    {
        using R = Result<std::unique_ptr<MeasurementStream>>;
        const std::string ref = dataRefFor(record);
        if (Status ok = validRef(ref); !ok)
            return R::failure(ok.message());
        const fs::path path = root_ / ref;
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        std::lock_guard lock(hdf5Mutex());
        const hid_t file = H5Fcreate(path.c_str(), H5F_ACC_EXCL, H5P_DEFAULT, H5P_DEFAULT);
        if (file < 0)
            return R::failure("the measurement data set " + ref + " exists already or cannot be created (raw data is never "
                              "overwritten)");
        const int rank = complex ? 2 : 1;
        const hsize_t initial[2] = {0, 2};
        const hsize_t maximum[2] = {H5S_UNLIMITED, 2};
        const hsize_t chunk[2] = {complex ? 8192u : 16384u, 2};
        const hid_t space = H5Screate_simple(rank, initial, maximum);
        const hid_t properties = H5Pcreate(H5P_DATASET_CREATE);
        H5Pset_chunk(properties, rank, chunk);
        const hid_t set = H5Dcreate2(file, complex ? "complex" : "real", H5T_NATIVE_DOUBLE, space, H5P_DEFAULT, properties, H5P_DEFAULT);
        H5Pclose(properties);
        H5Sclose(space);
        if (set < 0) {
            H5Fclose(file);
            return R::failure("could not create the HDF5 data set in " + ref);
        }
        return std::unique_ptr<MeasurementStream>(std::make_unique<Hdf5Stream>(file, set, path, ref, record, complex));
    }

    Status write(const MeasurementRecord& record, const MeasurementData& data) override
    {
        const std::string ref = dataRefFor(record);
        if (Status ok = validRef(ref); !ok)
            return ok;
        const fs::path path = root_ / ref;
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        MeasurementRecord r = record;
        r.dataRef = ref;
        nlohmann::json meta = toJson(r);
        meta["labels"] = data.labels;
        std::lock_guard lock(hdf5Mutex());
        const hid_t file = H5Fcreate(path.c_str(), H5F_ACC_EXCL, H5P_DEFAULT, H5P_DEFAULT);
        if (file < 0)
            return Status::failure("the measurement data set " + ref + " exists already or cannot be created");
        Status ok = writeDoubles(file, "real", data.real.data(), data.real.size(), 1);
        if (ok)
            ok = writeDoubles(file, "complex", reinterpret_cast<const double*>(data.complex.data()), data.complex.size(), 2);
        if (ok)
            ok = writeDoubles(file, "axis", data.axis.data(), data.axis.size(), 1);
        if (ok)
            ok = writeDoubles(file, "auxiliary", data.auxiliary.data(), data.auxiliary.size(), 1);
        if (ok)
            ok = writeBytes(file, "metadata", meta.dump(1));
        H5Fclose(file);
        makeReadOnly(path);
        return ok;
    }

    Result<MeasurementData> read(const std::string& ref) const override
    {
        using R = Result<MeasurementData>;
        if (Status ok = validRef(ref); !ok)
            return R::failure(ok.message());
        const fs::path path = root_ / ref;
        if (!fs::exists(path))
            return R::failure("the measurement data set " + ref + " is missing from " + root_.string());
        std::lock_guard lock(hdf5Mutex());
        const hid_t file = H5Fopen(path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
        if (file < 0)
            return R::failure("the measurement data set " + ref + " is not a readable HDF5 file");
        MeasurementData d;
        hsize_t rows = 0, columns = 0;
        std::vector<double> complexValues;
        bool ok = readDoubles(file, "real", d.real, rows, columns) && readDoubles(file, "complex", complexValues, rows, columns) &&
                  readDoubles(file, "axis", d.axis, rows, columns);
        hsize_t auxRows = 0, auxColumns = 0;
        ok = ok && readDoubles(file, "auxiliary", d.auxiliary, auxRows, auxColumns);
        d.complex.resize(complexValues.size() / 2);
        for (size_t i = 0; i < d.complex.size(); ++i)
            d.complex[i] = {complexValues[2 * i], complexValues[2 * i + 1]};
        std::string meta;
        if (readBytes(file, "metadata", meta)) {
            try {
                d.labels = nlohmann::json::parse(meta).value("labels", std::vector<std::string>{});
            } catch (...) {
            }
        }
        H5Fclose(file);
        if (!ok)
            return R::failure("the measurement data set " + ref + " could not be read completely");
        return d;
    }

    Result<MeasurementRecord> readRecord(const std::string& ref) const override
    {
        using R = Result<MeasurementRecord>;
        if (Status ok = validRef(ref); !ok)
            return R::failure(ok.message());
        std::string meta;
        {
            std::lock_guard lock(hdf5Mutex());
            const hid_t file = H5Fopen((root_ / ref).c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
            if (file < 0)
                return R::failure("the measurement data set " + ref + " is missing or not HDF5");
            const bool ok = readBytes(file, "metadata", meta);
            H5Fclose(file);
            if (!ok)
                return R::failure("the measurement data set " + ref + " has no metadata");
        }
        try {
            return measurementRecordFromJson(nlohmann::json::parse(meta));
        } catch (const std::exception& e) {
            return R::failure("the metadata of " + ref + " is not valid JSON: " + e.what());
        }
    }

    bool exists(const std::string& ref) const override { return validRef(ref) && fs::exists(root_ / ref); }
};

#endif  // RESONANT_HAVE_HDF5

}  // namespace

std::string_view toString(StoreFormat format) { return format == StoreFormat::Hdf5 ? "hdf5" : "binary"; }

bool hdf5Available()
{
#ifdef RESONANT_HAVE_HDF5
    return true;
#else
    return false;
#endif
}

Result<std::unique_ptr<MeasurementStore>> openMeasurementStore(const fs::path& root, std::optional<StoreFormat> format)
{
    using R = Result<std::unique_ptr<MeasurementStore>>;
    std::error_code ec;
    fs::create_directories(root, ec);
    if (!fs::is_directory(root))
        return R::failure("cannot create the measurement directory " + root.string() + (ec ? ": " + ec.message() : ""));
    const StoreFormat chosen = format.value_or(hdf5Available() ? StoreFormat::Hdf5 : StoreFormat::Binary);
    if (chosen == StoreFormat::Hdf5) {
#ifdef RESONANT_HAVE_HDF5
        return std::unique_ptr<MeasurementStore>(std::make_unique<Hdf5Store>(root));
#else
        return R::failure("this build has no HDF5 support; use the binary measurement format");
#endif
    }
    return std::unique_ptr<MeasurementStore>(std::make_unique<BinaryStore>(root));
}

fs::path measurementRootFor(const fs::path& projectFile)
{
    return projectFile.parent_path() / (projectFile.stem().string() + ".data") / "measurements";
}

}  // namespace resonant::experiments
