#pragma once

#include <functional>
#include <string>

#include <libchess/Tuner.h>

namespace libchess {

class TunableParameterP : public TunableParameter {
   public:
    TunableParameterP(std::string name, int *value) noexcept : TunableParameter(name, *value), name_(std::move(name)), value_(value) {
    }

    TunableParameter operator+(int rhs) const noexcept {
        return TunableParameter{name(), value() + rhs};
    }
    TunableParameter operator-(int rhs) const noexcept {
        return TunableParameter{name(), *value_ - rhs};
    }
    void operator+=(int rhs) noexcept {
        (*value_) += rhs;
    }
    void operator-=(int rhs) noexcept {
        (*value_) -= rhs;
    }

    [[nodiscard]] const std::string& name() const noexcept {
        return name_;
    }
    [[nodiscard]] int value() const noexcept {
        return *value_;
    }

    void set_value(int value) noexcept {
        *value_ = value;
    }

    [[nodiscard]] std::string to_str() const noexcept {
        return name_ + ": " + std::to_string(*value_);
    }

   private:
    std::string name_;
    int *value_;
};

}
