#pragma once
#include "CalcCore.h"
#include <functional>

namespace calc {
struct EquationValue {
    Value left{}, right{};
    std::string error;
    explicit operator bool() const { return error.empty(); }
    Value residual() const { return left-right; }
};

class Equation {
public:
    bool compile(const std::string& source);
    EquationValue evaluate(const Context& context) const;
    const std::string& error() const { return error_; }
    bool hasEquals() const { return equals_; }
    bool usesParameter() const { return left_.usesParameter() || right_.usesParameter(); }
private:
    Expression left_, right_;
    std::string error_;
    bool equals_=false;
};

struct SolveResult {
    double x=0, residual=0;
    int evaluations=0;
    std::string error;
    explicit operator bool() const { return error.empty(); }
};
// Real, local solve from an initial guess. Bounded damped Newton with an
// automatic bracketing fallback. Never changes the caller's variables/Ans.
SolveResult solveGuess(const Equation& equation, const Context& context, double guess,
                       const std::function<bool()>& shouldStop = {});
}
