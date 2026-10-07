#pragma once
#include <array>
#include <complex>
#include <string>
#include <vector>

namespace calc {
using Value = std::complex<double>;
constexpr double Pi = 3.14159265358979323846;
enum class Angle { Radians, Degrees, Gradians };
struct Context {
    Angle angle = Angle::Radians;
    std::array<Value, 26> vars{};
    Value ans{};
    double x = 0;
};
struct Result {
    Value value{};
    std::string error;
    explicit operator bool() const { return error.empty(); }
};
bool finite(Value v);
bool real(Value v);
std::string format(Value v, int digits = 10);
std::string fraction(double v);

// Parsed once, evaluated repeatedly while graphing. Input and recursion are bounded.
class Expression {
public:
    bool compile(const std::string& source);
    Result evaluate(const Context& context, int operationBudget = 100000) const;
    const std::string& error() const { return error_; }
    bool usesParameter() const;
private:
    struct Node {
        char op = 0;
        Value value{};
        std::string name;
        std::vector<int> args;
        int height = 1;
    };
    std::vector<Node> nodes_;
    std::string source_, error_;
    size_t pos_ = 0;
    int root_ = -1, depth_ = 0;
    void space();
    bool take(char c);
    int add(Node node);
    int sum();
    int product();
    int unary();
    int power();
    int atom();
    Value eval(int index, const Context& c, std::string& error, int& budget) const;
};
Result calculate(const std::string& source, Context& context);
Result derivative(const Expression& e, const Context& c, double x);
Result integrate(const Expression& e, const Context& c, double a, double b);
Result root(const Expression& e, const Context& c, double a, double b);
bool isFunction(const std::string& name);
Result extremum(const Expression& e, const Context& c, double a, double b, bool maximum);

struct Statistics {
    size_t n = 0;
    double sum = 0, sumSq = 0, mean = 0, populationSD = 0, sampleSD = 0;
    double minimum = 0, q1 = 0, median = 0, q3 = 0, maximum = 0;
};
Statistics statistics(std::vector<double> data);
struct Regression { double intercept = 0, slope = 0, r = 0; std::string error; };
Regression regression(const std::vector<double>& x, const std::vector<double>& y);
using Matrix = std::vector<std::vector<double>>;
bool parseMatrix(const std::string& input, const Context& c, Matrix& m, std::string& error);
bool inverse(const Matrix& m, Matrix& out, std::string& error);
double determinant(Matrix m);
Matrix transpose(const Matrix& m);
bool multiply(const Matrix& a, const Matrix& b, Matrix& out);
bool linearSolve(const Matrix& augmented, std::vector<double>& out, std::string& error);
std::vector<Value> quadratic(double a, double b, double c);
std::vector<Value> cubic(double a, double b, double c, double d);
std::vector<std::string> split(const std::string& s, char delimiter);
bool parseList(const std::string& s, const Context& c, std::vector<double>& out, std::string& error);
}
