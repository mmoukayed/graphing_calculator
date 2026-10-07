#include "CalcCore.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numeric>

namespace calc {
bool isFunction(const std::string& name) {
    static const char* functions[] = {"sin","cos","tan","asin","acos","atan","sinh","cosh","tanh","asinh","acosh","atanh","sqrt","cbrt","ln","log","log2","exp","abs","arg","re","im","conj","floor","ceil","round","sign","min","max","mod","gcd","lcm","ncr","npr","atan2","hypot","pow","normalpdf","normalcdf","binompdf","binomcdf","poissonpdf","poissoncdf","diff","integral","solve"};
    for(const char* f:functions) if(name==f) return true;
    return false;
}
bool Expression::usesParameter() const {
    for(const auto& n:nodes_) if(n.op=='v' && (n.name=="x" || n.name=="t")) return true;
    return false;
}
bool finite(Value v) { return std::isfinite(v.real()) && std::isfinite(v.imag()); }
bool real(Value v) { return v.imag()==0 || std::abs(v.imag()) < 1e-12*std::abs(v.real()); }
static std::string number(double v, int digits) {
    if (v==0) v = 0; // Suppress negative zero without rounding tiny numbers to zero.
    char b[48]; std::snprintf(b, sizeof(b), "%.*g", digits, v); return b;
}
std::string format(Value v, int digits) {
    if (!finite(v)) return "undefined";
    if (real(v)) return number(v.real(), digits);
    std::string im = number(std::abs(v.imag()), digits) + "i";
    if (v.real()==0 || std::abs(v.real()) < 1e-12*std::abs(v.imag())) return (v.imag() < 0 ? "-" : "") + im;
    return number(v.real(), digits) + (v.imag() < 0 ? "-" : "+") + im;
}
std::string fraction(double v) {
    if (!std::isfinite(v) || std::abs(v) > 1e9) return number(v, 10);
    // Bounded continued fraction; no integer overflow for accepted magnitudes.
    double x = std::abs(v); long long p0 = 0, p1 = 1, q0 = 1, q1 = 0;
    for (int n = 0; n < 24; ++n) {
        if (x > 1e9) break;
        long long a = static_cast<long long>(std::floor(x));
        long long p = a*p1+p0, q = a*q1+q0;
        if (q > 1000000 || p > 1000000000000000LL) break;
        p0=p1; p1=p; q0=q1; q1=q;
        if (std::abs(std::abs(v)-double(p)/q) < 1e-12 || x-a < 1e-14) break;
        x = 1/(x-a);
    }
    if (!q1) return number(v, 10);
    return (v < 0 ? "-" : "") + std::to_string(p1) + (q1==1 ? "" : "/"+std::to_string(q1));
}
void Expression::space() { while(pos_<source_.size() && std::isspace((unsigned char)source_[pos_])) ++pos_; }
bool Expression::take(char c) { space(); if(pos_<source_.size() && source_[pos_]==c) { ++pos_; return true; } return false; }
int Expression::add(Node n) {
    if(nodes_.size()>=256) { error_="Expression too complex"; return -1; }
    for(int child:n.args) if(child>=0) n.height=std::max(n.height,nodes_[child].height+1);
    if(n.height>32) {error_="Expression tree too deep";return -1;}
    nodes_.push_back(std::move(n)); return int(nodes_.size())-1;
}
bool Expression::compile(const std::string& s) {
    source_=s; pos_=0; depth_=0; nodes_.clear(); error_.clear(); root_=-1;
    if(s.size()>384) { error_="Maximum 384 characters"; return false; }
    root_=sum(); space();
    if(error_.empty() && pos_!=source_.size()) error_="Unexpected character at "+std::to_string(pos_+1);
    return error_.empty() && root_>=0;
}
int Expression::sum() {
    if(++depth_>32) { error_="Nesting too deep"; --depth_; return -1; }
    int n=product();
    while(error_.empty()) {
        char op=take('+')?'+':take('-')?'-':0; if(!op) break;
        int rhs=product(); n=add({op,{},"",{n,rhs}});
    }
    --depth_; return n;
}
int Expression::product() {
    int n=unary();
    while(error_.empty()) {
        char op=take('*')?'*':take('/')?'/':0;
        space();
        // Implicit multiplication: 2x, 2pi, 2(3+4), (x+1)(x-1).
        if(!op && pos_<source_.size() && (source_[pos_]=='(' || std::isalpha((unsigned char)source_[pos_]))) op='*';
        if(!op) break;
        int rhs=unary(); n=add({op,{},"",{n,rhs}});
    }
    return n;
}
int Expression::unary() {
    if(++depth_>32) {error_="Nesting too deep"; --depth_; return -1;}
    int n;
    if(take('+')) n=unary();
    else if(take('-')) { int child=unary(); n=add({'~',{},"",{child}}); }
    else n=power();
    --depth_; return n;
}
int Expression::power() {
    int n=atom();
    while(error_.empty()) {
        if(take('!')) n=add({'!',{},"",{n}});
        else if(take('%')) n=add({'%',{},"",{n}});
        else break;
    }
    if(error_.empty() && take('^')) { int rhs=unary(); n=add({'^',{},"",{n,rhs}}); }
    return n;
}
int Expression::atom() {
    space();
    if(take('(')) { int n=sum(); if(!take(')') && error_.empty()) error_="Missing closing parenthesis"; return n; }
    if(pos_>=source_.size()) { error_="Expected a number or expression"; return -1; }
    char ch=source_[pos_];
    if(std::isdigit((unsigned char)ch) || ch=='.') {
        const char* start=source_.c_str()+pos_; char* end=nullptr;
        double v=std::strtod(start,&end);
        if(end==start || !std::isfinite(v)) { error_="Invalid number"; return -1; }
        pos_+=end-start; return add({'#',v,"",{}});
    }
    if(std::isalpha((unsigned char)ch)) {
        std::string name;
        while(pos_<source_.size() && std::isalnum((unsigned char)source_[pos_])) name+=char(std::tolower((unsigned char)source_[pos_++]));
        if(isFunction(name)) {
            if(!take('(')) { error_="Use "+name+"(...)"; return -1; }
            std::vector<int> args;
            do { args.push_back(sum()); if(args.size()>3) {error_="Too many arguments"; break;} } while(error_.empty() && take(','));
            if(!take(')') && error_.empty()) error_="Missing closing parenthesis";
            return add({'f',{},name,args});
        }
        if(name=="pi") return add({'#',Pi,"",{}});
        if(name=="e") return add({'#',std::exp(1.0),"",{}});
        if(name=="i") return add({'#',Value(0,1),"",{}});
        if(name=="ans" || name.size()==1) return add({'v',{},name,{}});
        error_="Unknown name: "+name; return -1;
    }
    error_="Unexpected character at "+std::to_string(pos_+1); return -1;
}
static double angleScale(Angle a) { return a==Angle::Degrees?Pi/180:a==Angle::Gradians?Pi/200:1; }
static bool integer(double x) { return std::isfinite(x) && std::floor(x)==x; }
Value Expression::eval(int index, const Context& c, std::string& err, int& budget) const {
    if(!err.empty()) return {};
    if(--budget<0) {err="Calculation limit reached"; return {};}
    if(index<0 || index>=int(nodes_.size())) {err="Invalid expression"; return {};}
    const Node& n=nodes_[index];
    if(n.op=='#') return n.value;
    if(n.op=='v') {
        if(n.name=="ans") return c.ans;
        if(n.name=="x" || n.name=="t") return c.x;
        return c.vars[n.name[0]-'a'];
    }
    if(n.op=='f' && (n.name=="diff" || n.name=="integral" || n.name=="solve")) {
        int count=n.name=="diff"?2:3;
        if(int(n.args.size())!=count) {err="Wrong number of arguments"; return {};}
        Value av=eval(n.args[1],c,err,budget), bv=count==3?eval(n.args[2],c,err,budget):Value{};
        if(!real(av)||!real(bv)) {err="Real bounds required"; return {};}
        auto at=[&](double x) { Context cc=c; cc.x=x; return eval(n.args[0],cc,err,budget); };
        double a=av.real(),b=bv.real();
        if(n.name=="diff") { double h=1e-5*std::max(1.0,std::abs(a)); return (at(a+h)-at(a-h))/(2*h); }
        if(n.name=="integral") {
            Value previous{};
            for(int count=32;count<=1024&&err.empty();count*=2){
                Value total=at(a)+at(b);double h=(b-a)/count;
                for(int j=1;j<count&&err.empty();++j)total+=double(j%2?4:2)*at(a+j*h);
                Value result=total*h/3.0;
                if(count>32&&std::abs(result-previous)<1e-8*(1+std::abs(result)))return result;
                previous=result;
            }
            if(err.empty())err="Integral did not converge";return {};
        }
        Value va=at(a),vb=at(b);
        if(!real(va)||!real(vb)||!finite(va)||!finite(vb)||a>=b) {err="Invalid real interval"; return {};}
        if(std::abs(va)<1e-10) return a;
        if(std::abs(vb)<1e-10) return b;
        if(std::signbit(va.real())==std::signbit(vb.real())) {err="Bounds must bracket a root"; return {};}
        for(int j=0;j<80 && err.empty();++j) {
            double m=(a+b)/2; Value vm=at(m);
            if(!finite(vm)||!real(vm)) {err="Discontinuous interval"; return {};}
            if(std::abs(vm)<1e-9) return m;
            if(std::signbit(va.real())!=std::signbit(vm.real())) b=m; else {a=m; va=vm;}
        }
        err="No converged root (check interval)"; return {};
    }
    std::vector<Value> v; for(int child:n.args) v.push_back(eval(child,c,err,budget));
    if(!err.empty()) return {};
    Value a=v.empty()?Value{}:v[0], b=v.size()>1?v[1]:Value{};
    switch(n.op) {
        case '+': return a+b; case '-': return a-b; case '*': return a*b;
        case '/': if(std::abs(b)==0) {err="Division by zero";return {};} return a/b;
        case '^': return std::pow(a,b);
        case '~': return Value(-a.real(),a.imag()==0?0:-a.imag());
        case '%': return a/100.0;
        case '!': if(!real(a)||!integer(a.real())||a.real()<0||a.real()>170) {err="Factorial needs integer 0..170";return {};} return std::tgamma(a.real()+1);
    }
    std::string f=n.name; int arity=1;
    if(f=="min"||f=="max"||f=="mod"||f=="gcd"||f=="lcm"||f=="ncr"||f=="npr"||f=="atan2"||f=="hypot"||f=="pow"||f=="poissonpdf"||f=="poissoncdf") arity=2;
    if(f=="binompdf"||f=="binomcdf") arity=3;
    if(int(v.size())!=arity) {err="Wrong number of arguments: "+f;return {};}
    double s=angleScale(c.angle);
    if(f=="sin") return std::sin(a*s); if(f=="cos") return std::cos(a*s);
    if(f=="tan") {if(std::abs(std::cos(a*s))<1e-14) {err="Undefined tangent"; return {};} return std::tan(a*s);}
    if(f=="asin") return std::asin(a)/s; if(f=="acos") return std::acos(a)/s; if(f=="atan") return std::atan(a)/s;
    if(f=="sinh") return std::sinh(a); if(f=="cosh") return std::cosh(a); if(f=="tanh") return std::tanh(a);
    if(f=="asinh") return std::asinh(a); if(f=="acosh") return std::acosh(a); if(f=="atanh") return std::atanh(a);
    if(f=="sqrt") return std::sqrt(a); if(f=="cbrt") return real(a)?Value(std::cbrt(a.real())):std::pow(a,1.0/3);
    if(f=="ln") return std::log(a); if(f=="log") return std::log(a)/std::log(10.0); if(f=="log2") return std::log(a)/std::log(2.0);
    if(f=="exp") return std::exp(a); if(f=="abs") return std::abs(a); if(f=="arg") return std::arg(a)/s;
    if(f=="re") return a.real(); if(f=="im") return a.imag(); if(f=="conj") return std::conj(a); if(f=="pow") return std::pow(a,b);
    for(Value item:v) if(!real(item)) {err="Real arguments required: "+f;return {};}
    double x=a.real(),y=b.real();
    if(f=="floor") return std::floor(x); if(f=="ceil") return std::ceil(x); if(f=="round") return std::round(x); if(f=="sign") return (x>0)-(x<0);
    if(f=="min") return std::min(x,y); if(f=="max") return std::max(x,y);
    if(f=="mod") {if(y==0) {err="Division by zero";return {};} return std::fmod(x,y);}
    if(f=="atan2") return std::atan2(x,y)/s; if(f=="hypot") return std::hypot(x,y);
    if(f=="gcd"||f=="lcm") {
        if(!integer(x)||!integer(y)||std::abs(x)>1e9||std::abs(y)>1e9) {err="Integers within +/-1e9 required";return {};}
        long long aa=std::llabs((long long)x),bb=std::llabs((long long)y),g=std::gcd(aa,bb);
        return double(f=="gcd"?g:g?aa/g*bb:0);
    }
    if(f=="ncr"||f=="npr") {
        if(!integer(x)||!integer(y)||x<0||y<0||y>x||x>10000) {err="Require integers 0<=r<=n<=10000";return {};}
        if(f=="ncr") y=std::min(y,x-y);
        double r=1; for(int k=1;k<=y;++k) r*= (x-k+1)/(f=="ncr"?k:1); return r;
    }
    if(f=="normalpdf") return std::exp(-x*x/2)/std::sqrt(2*Pi);
    if(f=="normalcdf") return 0.5*std::erfc(-x/std::sqrt(2.0));
    if(f=="binompdf"||f=="binomcdf") {
        double k=v[2].real();
        if(!integer(x)||x<0||x>1000||y<0||y>1||!integer(k)||k<0||k>x) {err="Use binom(n,p,k), n<=1000";return {};}
        auto pdf=[&](double z) { if(y==0) return z==0?1.0:0.0; if(y==1) return z==x?1.0:0.0; return std::exp(std::lgamma(x+1)-std::lgamma(z+1)-std::lgamma(x-z+1)+z*std::log(y)+(x-z)*std::log1p(-y)); };
        if(f=="binompdf") return pdf(k);
        double r=0; for(int j=0;j<=k;++j) r+=pdf(j); return std::min(1.0,r);
    }
    if(f=="poissonpdf"||f=="poissoncdf") {
        if(x<0||!integer(y)||y<0||y>1000) {err="Use poisson(lambda,k), 0<=k<=1000";return {};}
        auto pdf=[&](double k){return x==0?(k==0?1.0:0.0):std::exp(k*std::log(x)-x-std::lgamma(k+1));};
        if(f=="poissonpdf") return pdf(y);
        double r=0; for(int j=0;j<=y;++j) r+=pdf(j); return std::min(1.0,r);
    }
    err="Unknown function"; return {};
}
Result Expression::evaluate(const Context& c,int operationBudget) const {
    Result r; if(!error_.empty()||root_<0) {r.error=error_.empty()?"Empty expression":error_;return r;}
    int budget=operationBudget; r.value=eval(root_,c,r.error,budget);
    if(r.error.empty()&&!finite(r.value)) r.error="Math domain or overflow";
    return r;
}
Result calculate(const std::string& source, Context& c) {
    auto eq=source.find('='); int dest=-1; std::string rhs=source;
    if(eq!=std::string::npos) {
        std::string lhs=source.substr(0,eq); lhs.erase(std::remove_if(lhs.begin(),lhs.end(),[](unsigned char ch){return std::isspace(ch);}),lhs.end());
        if(lhs.size()!=1 || !std::isalpha((unsigned char)lhs[0])) return {{},"Assignment: a=expression"};
        char ch=std::tolower((unsigned char)lhs[0]);
        if(ch=='e'||ch=='i'||ch=='x'||ch=='t') return {{},"e, i, x and t are reserved"};
        dest=ch-'a'; rhs=source.substr(eq+1);
    }
    Expression e; e.compile(rhs); Result r=e.evaluate(c);
    if(r) {c.ans=r.value; if(dest>=0) c.vars[dest]=r.value;}
    return r;
}
static Result at(const Expression& e, Context c, double x) {c.x=x; return e.evaluate(c);}
Result derivative(const Expression& e,const Context& c,double x) {
    double h=1e-5*std::max(1.0,std::abs(x)); auto a=at(e,c,x+h),b=at(e,c,x-h);
    if(!a) return a; if(!b) return b; return {(a.value-b.value)/(2*h),""};
}
Result integrate(const Expression& e,const Context& c,double a,double b) {
    // Composite Simpson with refinement and convergence check; bounded work.
    Value previous{};
    for(int n=32;n<=2048;n*=2) {
        Value sum{}; double h=(b-a)/n;
        for(int j=0;j<=n;++j) {auto v=at(e,c,a+j*h); if(!v) return v; sum+=v.value*double(j==0||j==n?1:j%2?4:2);}
        Value result=sum*h/3.0;
        if(n>32 && std::abs(result-previous)<1e-8*(1+std::abs(result))) return {result,""};
        previous=result;
    }
    return {{},"Integral did not converge"};
}
Result root(const Expression& e,const Context& c,double a,double b) {
    auto l=at(e,c,a),r=at(e,c,b);
    if(!l) return l; if(!r) return r;
    if(!real(l.value)||!real(r.value)||a>=b) return {{},"Invalid real interval"};
    if(std::abs(l.value)<1e-10) return {a,""}; if(std::abs(r.value)<1e-10) return {b,""};
    if(std::signbit(l.value.real())==std::signbit(r.value.real())) return {{},"Bounds must bracket a root"};
    for(int j=0;j<100;++j) {
        double m=(a+b)/2; auto v=at(e,c,m); if(!v) return v;
        if(!real(v.value)) return {{},"Real function required"};
        if(std::abs(v.value)<1e-9) return {m,""};
        if(std::signbit(l.value.real())!=std::signbit(v.value.real())) b=m; else {a=m;l=v;}
    }
    return {{},"No converged root (check interval)"};
}
Result extremum(const Expression& e,const Context& c,double a,double b,bool maximum) {
    if(a>=b) return {{},"Require lower < upper"};
    const double g=(std::sqrt(5.0)-1)/2;
    for(int j=0;j<80;++j) {
        double x=b-g*(b-a),y=a+g*(b-a); auto l=at(e,c,x),r=at(e,c,y);
        if(!l) return l; if(!r) return r; if(!real(l.value)||!real(r.value)) return {{},"Real function required"};
        if((l.value.real()<r.value.real())!=maximum) b=y; else a=x;
    }
    return {(a+b)/2,""};
}
static double medianRange(const std::vector<double>& d,size_t a,size_t b) {
    size_t n=b-a; if(!n) return d[a? a-1:0];
    return n%2?d[a+n/2]:(d[a+n/2-1]+d[a+n/2])/2;
}
Statistics statistics(std::vector<double> d) {
    Statistics s; s.n=d.size(); if(d.empty()) return s;
    std::sort(d.begin(),d.end()); double m2=0;size_t n=0;
    for(double v:d) {s.sum+=v;s.sumSq+=v*v;++n;double delta=v-s.mean;s.mean+=delta/n;m2+=delta*(v-s.mean);}
    s.populationSD=std::sqrt(std::max(0.0,m2/s.n)); s.sampleSD=s.n>1?std::sqrt(std::max(0.0,m2/(s.n-1))):std::numeric_limits<double>::quiet_NaN();
    s.minimum=d.front();s.maximum=d.back();s.median=medianRange(d,0,s.n);
    s.q1=medianRange(d,0,s.n/2);s.q3=medianRange(d,(s.n+1)/2,s.n);return s;
}
Regression regression(const std::vector<double>& x,const std::vector<double>& y) {
    Regression r;if(x.size()!=y.size()||x.size()<2) {r.error="Need equal lists of at least 2 values";return r;}
    double mx=statistics(x).mean,my=statistics(y).mean,sxx=0,sxy=0,syy=0;
    for(size_t j=0;j<x.size();++j) {double a=x[j]-mx,b=y[j]-my;sxx+=a*a;sxy+=a*b;syy+=b*b;}
    if(sxx<1e-20) {r.error="X values have zero variance";return r;}
    r.slope=sxy/sxx;r.intercept=my-r.slope*mx;r.r=syy>0?sxy/std::sqrt(sxx*syy):std::numeric_limits<double>::quiet_NaN();return r;
}
std::vector<std::string> split(const std::string& s,char d) {
    std::vector<std::string> out;size_t start=0;int depth=0;
    for(size_t j=0;j<s.size();++j) {if(s[j]=='(')++depth; if(s[j]==')')--depth;if(s[j]==d&&depth==0) {out.push_back(s.substr(start,j-start));start=j+1;}}
    out.push_back(s.substr(start));return out;
}
bool parseList(const std::string& s,const Context& c,std::vector<double>& out,std::string& error) {
    out.clear();error.clear();auto items=split(s,',');
    if(items.size()>128) {error="Maximum 128 data values";return false;}
    for(auto& item:items) {Expression e;e.compile(item);auto r=e.evaluate(c);if(!r||!real(r.value)) {error=r?"Real values required":r.error;return false;}out.push_back(r.value.real());}return true;
}
bool parseMatrix(const std::string& s,const Context& c,Matrix& m,std::string& error) {
    m.clear();auto rows=split(s,';');if(rows.size()>6) {error="Maximum 6 rows";return false;}
    for(auto& row:rows) {std::vector<double> v;if(!parseList(row,c,v,error))return false;if(v.size()>7||(!m.empty()&&v.size()!=m[0].size())) {error="Use equal rows, at most 7 columns";return false;}m.push_back(v);}return true;
}
static bool rectangular(const Matrix& m) {if(m.empty()||m[0].empty())return false;for(auto& r:m)if(r.size()!=m[0].size())return false;return true;}
double determinant(Matrix m) {
    if(!rectangular(m)||m.size()!=m[0].size())return std::numeric_limits<double>::quiet_NaN();
    double det=1;size_t n=m.size();
    for(size_t k=0;k<n;++k) {size_t p=k;for(size_t j=k+1;j<n;++j)if(std::abs(m[j][k])>std::abs(m[p][k]))p=j;
        if(m[p][k]==0)return 0;if(p!=k){std::swap(m[k],m[p]);det=-det;}det*=m[k][k];
        for(size_t j=k+1;j<n;++j) {double f=m[j][k]/m[k][k];for(size_t l=k+1;l<n;++l)m[j][l]-=f*m[k][l];}}
    return det;
}
static bool reduce(Matrix& m,size_t n,std::string& error) {
    double scale=0;for(auto& r:m)for(size_t j=0;j<n;++j)scale=std::max(scale,std::abs(r[j]));
    for(size_t k=0;k<n;++k) {size_t p=k;for(size_t j=k+1;j<n;++j)if(std::abs(m[j][k])>std::abs(m[p][k]))p=j;
        if(std::abs(m[p][k])<=1e-13*scale) {error="Singular or ill-conditioned matrix";return false;}
        std::swap(m[k],m[p]);double f=m[k][k];for(double& v:m[k])v/=f;
        for(size_t j=0;j<n;++j)if(j!=k){f=m[j][k];for(size_t l=0;l<m[j].size();++l)m[j][l]-=f*m[k][l];}}
    return true;
}
bool inverse(const Matrix& m,Matrix& out,std::string& error) {
    if(!rectangular(m)||m.size()!=m[0].size()) {error="Square matrix required";return false;}
    size_t n=m.size();Matrix aug=m;for(size_t j=0;j<n;++j){aug[j].resize(2*n);aug[j][n+j]=1;}
    if(!reduce(aug,n,error))return false;out.assign(n,std::vector<double>(n));for(size_t j=0;j<n;++j)for(size_t k=0;k<n;++k)out[j][k]=aug[j][n+k];return true;
}
Matrix transpose(const Matrix& m) {if(!rectangular(m))return {};Matrix out(m[0].size(),std::vector<double>(m.size()));for(size_t j=0;j<m.size();++j)for(size_t k=0;k<m[0].size();++k)out[k][j]=m[j][k];return out;}
bool multiply(const Matrix& a,const Matrix& b,Matrix& out) {
    if(!rectangular(a)||!rectangular(b)||a[0].size()!=b.size())return false;out.assign(a.size(),std::vector<double>(b[0].size()));
    for(size_t j=0;j<a.size();++j)for(size_t k=0;k<b[0].size();++k)for(size_t l=0;l<b.size();++l)out[j][k]+=a[j][l]*b[l][k];return true;
}
bool linearSolve(const Matrix& a,std::vector<double>& out,std::string& error) {
    if(!rectangular(a)||a[0].size()!=a.size()+1) {error="Use n rows of n coefficients + RHS";return false;}
    Matrix m=a;if(!reduce(m,a.size(),error))return false;out.clear();for(auto& row:m)out.push_back(row.back());return true;
}
std::vector<Value> quadratic(double a,double b,double c) {
    if(a==0)return b==0?std::vector<Value>{}:std::vector<Value>{Value(-c/b)};
    Value disc=std::sqrt(Value(b*b-4*a*c));
    Value q=-0.5*(b+(b>=0?disc:-disc));
    if(std::abs(q)==0)return {Value(0),Value(0)};
    return {q/a,c/q};
}
std::vector<Value> cubic(double a,double b,double c,double d) {
    if(a==0)return quadratic(b,c,d);
    b/=a;c/=a;d/=a;double p=c-b*b/3,q=2*b*b*b/27-b*c/3+d;
    Value disc=std::sqrt(Value(q*q/4+p*p*p/27));
    Value u=std::pow(-q/2+disc,1.0/3);
    if(std::abs(u)==0)u=std::pow(-q/2-disc,1.0/3);
    if(std::abs(u)==0)return {Value(-b/3),Value(-b/3),Value(-b/3)};
    Value v=-p/(3.0*u),w(-0.5,std::sqrt(3.0)/2);
    return {u+v-b/3,u*w+v/std::complex<double>(w)-b/3,u*w*w+v/(w*w)-b/3};
}
}
