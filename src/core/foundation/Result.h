// Explicit success / failure values. Operations that can fail return a
// Status or a Result<T>; nothing reports success after a failed operation.
#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <utility>

namespace resonant {

class [[nodiscard]] Status {
public:
    static Status success() { return Status(); }
    static Status failure(std::string message) { return Status(std::move(message)); }

    bool ok() const { return ok_; }
    explicit operator bool() const { return ok_; }
    const std::string& message() const { return message_; }

private:
    Status() = default;
    explicit Status(std::string message) : ok_(false), message_(std::move(message)) {}

    bool ok_ = true;
    std::string message_;
};

template <typename T>
class [[nodiscard]] Result {
public:
    Result(T value) : value_(std::move(value)) {}  // NOLINT(google-explicit-constructor)
    static Result failure(std::string message)
    {
        Result result;
        result.message_ = std::move(message);
        return result;
    }

    bool ok() const { return value_.has_value(); }
    explicit operator bool() const { return ok(); }
    const std::string& message() const { return message_; }

    T& value()
    {
        assert(ok());
        return *value_;
    }
    const T& value() const
    {
        assert(ok());
        return *value_;
    }
    T take()
    {
        assert(ok());
        return std::move(*value_);
    }

private:
    Result() = default;

    std::optional<T> value_;
    std::string message_;
};

}  // namespace resonant
