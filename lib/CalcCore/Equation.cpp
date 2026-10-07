#include "Equation.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace calc {
bool Equation::compile(const std::string& source) {
    error_.clear();
    const auto equal=source.find('=');
    equals_=equal!=std::string::npos;
    if(source.size()>384) { error_="Maximum 384 characters"; return false; }
    if(equals_ && source.find('=',equal+1)!=std::string::npos) {
        error_="Use one equation: left = right"; return false;
    }
    if(!left_.compile(equals_?source.substr(0,equal):source)) {
        error_="Left: "+left_.error(); return false;
    }
    if(!right_.compile(equals_?source.substr(equal+1):"0")) {
        error_="Right: "+right_.error(); return false;
    }
    return true;
}
EquationValue Equation::evaluate(const Context& c) const {
    if(!error_.empty()) return {{},{},error_};
    auto l=left_.evaluate(c,4096),r=right_.evaluate(c,4096);
    if(!l || !r) return {{},{},!l?l.error:r.error};
    if(!finite(l.value-r.value)) return {{},{},"Equation residual overflow"};
    return {l.value,r.value,""};
}

SolveResult solveGuess(const Equation& equation,const Context& context,double guess,
                       const std::function<bool()>& shouldStop) {
    SolveResult out;
    if(!equation.error().empty()) { out.error=equation.error(); return out; }
    if(!equation.usesParameter()) { out.error="Equation must contain X"; return out; }
    if(!std::isfinite(guess) || std::abs(guess)>1e12) {
        out.error="Initial X must be within +/-1e12"; return out;
    }
    constexpr int MaxEvaluations=768;
    bool stopped=false;
    struct Sample { double x=0,f=0,scale=1; bool ok=false; };
    auto sample=[&](double x) {
        Sample s; s.x=x;
        if(shouldStop&&shouldStop()){stopped=true;return s;}
        if(out.evaluations>=MaxEvaluations || !std::isfinite(x) || std::abs(x)>1e12) return s;
        ++out.evaluations;
        Context cc=context;cc.x=x;
        const auto v=equation.evaluate(cc);
        if(!v || !real(v.left) || !real(v.right)) return s;
        s.f=v.residual().real();
        s.scale=std::max({1.0,std::abs(v.left.real()),std::abs(v.right.real())});
        s.ok=std::isfinite(s.f);return s;
    };
    // A small residual alone is insufficient: exp(-x) and 1/x approach zero
    // without a finite root. Require a small Newton correction as well.
    auto closeResidual=[](const Sample& s) { return s.ok && std::abs(s.f)<=1e-10*s.scale; };
    auto success=[&](const Sample& s) {out.x=s.x;out.residual=s.f;return out;};
    Sample current=sample(guess);
    double trust=std::max(1.0,std::abs(guess)*0.25);
    for(int iteration=0;iteration<64 && !stopped && out.evaluations<MaxEvaluations/2;++iteration) {
        if(!current.ok) break;
        const double h=1e-5*std::max(1.0,std::abs(current.x));
        Sample left=sample(current.x-h),right=sample(current.x+h);
        double slope=std::numeric_limits<double>::quiet_NaN();
        if(left.ok&&right.ok) slope=(right.f-left.f)/(2*h);
        else if(right.ok) slope=(right.f-current.f)/h;
        else if(left.ok) slope=(current.f-left.f)/h;
        // Exact zero is accepted only if the local function is not constant.
        if(current.f==0 && ((left.ok&&left.f!=0)||(right.ok&&right.f!=0))) return success(current);
        if(std::isfinite(slope) && slope!=0) {
            const double correction=current.f/slope;
            if(closeResidual(current) && std::abs(correction)<=1e-8*(1+std::abs(current.x))) return success(current);
            double step=std::max(-trust,std::min(trust,correction));
            bool advanced=false;
            for(int damp=0;damp<12&&!stopped;++damp) {
                Sample next=sample(current.x-step);
                if(next.ok && std::abs(next.f)<std::abs(current.f)) {
                    current=next;trust=std::min(1e12,trust*2);advanced=true;break;
                }
                step*=0.5;
            }
            if(advanced) continue;
        }
        // Escape a flat derivative, including initial X=0 for X^2=4.
        Sample plus=sample(current.x+trust*0.1),minus=sample(current.x-trust*0.1);
        if(plus.ok&&std::abs(plus.f)<std::abs(current.f)) current=plus;
        else if(minus.ok&&std::abs(minus.f)<std::abs(current.f)) current=minus;
        else break;
    }
    // Expand adjacent intervals around the initial guess, checking the residual
    // at every bisection step so poles are never accepted just by interval width.
    Sample center=sample(guess),lastLeft=center,lastRight=center;
    double radius=0.25*std::max(1.0,std::abs(guess));
    for(int expand=0;expand<42 && !stopped && out.evaluations<MaxEvaluations;++expand,radius*=2) {
        Sample left=sample(guess-radius),right=sample(guess+radius);
        for(const Sample& endpoint:{left,right}) {
            if(!endpoint.ok||endpoint.f!=0)continue;
            double h=1e-5*std::max(1.0,std::abs(endpoint.x));
            Sample before=sample(endpoint.x-h),after=sample(endpoint.x+h);
            if((before.ok&&before.f!=0)||(after.ok&&after.f!=0))return success(endpoint);
        }
        const Sample pairs[4]={left,lastLeft,lastRight,right};
        for(int side=0;side<2;++side) {
            Sample a=pairs[side*2],b=pairs[side*2+1];
            if(!a.ok||!b.ok||std::signbit(a.f)==std::signbit(b.f)) continue;
            for(int n=0;n<80 && !stopped && out.evaluations<MaxEvaluations;++n) {
                Sample m=sample(a.x+(b.x-a.x)/2);
                if(!m.ok) break;
                if(closeResidual(m) && std::abs(b.x-a.x)<=1e-8*(1+std::abs(m.x))) return success(m);
                if(m.x==a.x||m.x==b.x) break;
                if(std::signbit(a.f)!=std::signbit(m.f)) b=m;else a=m;
            }
        }
        lastLeft=left;lastRight=right;
    }
    out.error=stopped?"Solve time limit. Simplify the equation.":"Cannot solve. Try a different initial X.";
    return out;
}
}
