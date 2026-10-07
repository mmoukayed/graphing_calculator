#include <CalcCore.h>
#include <Equation.h>
#include <cassert>
#include <cmath>
#include <iostream>

int assertions=0;
void check(bool ok,const char* label){++assertions;if(!ok){std::cerr<<"FAIL: "<<label<<"\n";std::exit(1);}}
void near(calc::Value got,calc::Value expected,const char* label,double tol=1e-8){check(std::abs(got-expected)<tol,label);}
calc::Value eval(const std::string& s,calc::Context c={}){calc::Expression e;check(e.compile(s),s.c_str());auto r=e.evaluate(c);if(!r)std::cerr<<r.error<<"\n";check(bool(r),s.c_str());return r.value;}
void invalid(const std::string& s){calc::Context c;auto r=calc::calculate(s,c);check(!r,s.c_str());}
int main(){
    near(eval("2+3*4"),14,"precedence");near(eval("-2^2"),-4,"unary precedence");near(eval("2^3^2"),512,"right associative power");near(eval("2^-3"),.125,"negative exponent");
    near(eval("2(3+4)"),14,"implicit parentheses");near(eval("2pi"),2*calc::Pi,"implicit constant");near(eval("3!^2"),36,"factorial precedence");near(eval("25%"),.25,"percent");
    near(eval("sqrt(-1)"),{0,1},"complex square root");near(eval("(2+3i)/(1-i)"),{-.5,2.5},"complex division");near(eval("ln(-1)"),{0,calc::Pi},"complex log");
    near(eval("sin(pi/6)"),.5,"radian sine");calc::Context c;c.angle=calc::Angle::Degrees;near(eval("sin(30)",c),.5,"degree sine");near(eval("asin(.5)",c),30,"degree inverse");c.angle=calc::Angle::Gradians;near(eval("cos(100)",c),0,"gradians");
    near(eval("ncr(20,3)"),1140,"combinations");near(eval("npr(5,2)"),20,"permutations");near(eval("gcd(84,30)"),6,"gcd");near(eval("lcm(12,18)"),36,"lcm");near(eval("normalcdf(0)"),.5,"normal cdf");near(eval("binompdf(10,.5,3)"),.1171875,"binomial");near(eval("binomcdf(2,.5,1)"),.75,"binomial cdf");near(eval("poissonpdf(2,0)"),std::exp(-2),"poisson");
    c={};auto r=calc::calculate("a=5",c);check(bool(r),"assignment");near(eval("2a+ans",c),15,"variables and ans");invalid("x=2");invalid("i=4");invalid("pi=2");invalid("1/0");invalid("(-1)!");invalid("sqrt()");invalid("sin(1,2)");invalid("bogus(2)");invalid("2+(");invalid("1 2");invalid("ncr(4,5)");invalid("gcd(1.5,2)");invalid(std::string(80,'-')+"1");invalid(std::string(400,'1'));
    near(eval("diff(x^3,2)"),12,"inline derivative",1e-5);near(eval("integral(x^2,0,3)"),9,"inline integral");near(eval("solve(x^2-2,0,2)"),std::sqrt(2),"inline solve");invalid("solve(1/x,-1,1)");
    calc::Expression e;e.compile("x^3");near(calc::derivative(e,c,2).value,12,"derivative",1e-5);near(calc::integrate(e,c,0,2).value,4,"integral");near(calc::integrate(e,c,2,0).value,-4,"reversed integral");
    e.compile("x^2-2");near(calc::root(e,c,0,2).value,std::sqrt(2),"bisection");check(!calc::root(e,c,2,3),"no bracket");e.compile("1/x");check(!calc::root(e,c,-1,1),"root at pole rejected");e.compile("(x-2)^2+1");near(calc::extremum(e,c,-1,5,false).value,2,"minimum",1e-6);
    auto s=calc::statistics({1,2,3,4,5});near(s.mean,3,"mean");near(s.sampleSD,std::sqrt(2.5),"sample sd");near(s.q1,1.5,"q1");near(s.q3,4.5,"q3");auto reg=calc::regression({1,2,3},{3,5,7});near(reg.slope,2,"regression slope");near(reg.intercept,1,"intercept");near(reg.r,1,"correlation");check(!calc::regression({1,1},{2,3}).error.empty(),"zero variance");
    calc::Matrix m,out,product;std::string error;check(calc::parseMatrix("1,2;3,4",c,m,error),"matrix parse");near(calc::determinant(m),-2,"det");check(calc::inverse(m,out,error),"inverse");check(calc::multiply(m,out,product),"multiply");near(product[0][0],1,"identity 00");near(product[0][1],0,"identity 01");near(product[1][1],1,"identity 11");check(!calc::inverse({{1,2},{2,4}},out,error),"singular matrix");check(!calc::parseMatrix("1,2;3",c,m,error),"ragged matrix");
    std::vector<double> x;check(calc::linearSolve({{2,1,5},{1,-1,1}},x,error),"linear solve");near(x[0],2,"linear x");near(x[1],1,"linear y");check(calc::parseList("ncr(5,2),sqrt(9),2pi",c,x,error),"nested comma list");near(x[0],10,"list first");
    auto q=calc::quadratic(1,-3,2);for(auto v:q)near(v*v-3.0*v+2.0,0,"quadratic residual");q=calc::quadratic(1,0,1);for(auto v:q)near(v*v+1.0,0,"complex quadratic residual");
    for(auto coefficients:std::vector<std::array<double,4>>{{1,-6,11,-6},{1,0,0,1},{1,0,0,0},{1,0,-3,2},{0,1,-3,2}}){q=calc::cubic(coefficients[0],coefficients[1],coefficients[2],coefficients[3]);for(auto v:q)near(((coefficients[0]*v+coefficients[1])*v+coefficients[2])*v+coefficients[3],0,"cubic residual",1e-6);}
    check(calc::fraction(1.0/3)=="1/3","fraction");check(calc::fraction(-2.5)=="-5/2","negative fraction");
    check(calc::format(1e-20)!="0","small real not rounded away");check(calc::format({0,1e-20})!="0","small imaginary not rounded away");check(!calc::real({0,1e-20}),"tiny imaginary is complex");
    std::string chain="1";for(int j=0;j<60;++j)chain+="+1";invalid(chain);
    check(std::abs(calc::determinant({{1e-20,0},{0,1e-20}})/1e-40-1)<1e-12,"small determinant retained");
    auto tinyRoots=calc::cubic(1,0,0,1e-60);for(auto v:tinyRoots)check(std::abs(v)>0.5e-20&&std::abs(v)<2e-20,"tiny cubic root retained");
    calc::Equation equation;
    auto solve=[&](const std::string& input,double guess,double expected){
        check(equation.compile(input),"equation parse");auto r=calc::solveGuess(equation,c,guess);
        if(!r)std::cerr<<input<<": "<<r.error<<"\n";
        check(bool(r),input.c_str());near(r.x,expected,"initial-guess solve",1e-6);
        check(std::abs(r.residual)<1e-7,"verified solve residual");
    };
    solve("2x+5=17",0,6);solve("x^2=4",0,2);solve("x^2=4",-3,-2);
    solve("(x-1)^2=0",3,1);solve("sqrt(x)=2",-1,4);solve("sqrt(x)=0",-1,0);solve("exp(x)=2",0,std::log(2));
    solve("x^3-2",1,std::cbrt(2));
    c.vars[0]=3;solve("a*x=12",0,4);
    for(const char* noRoot:{"x^2+1=0","1/x=0","exp(-x)=0","x=x","floor(x)^2+1=0"}){
        check(equation.compile(noRoot),"no-root parse");auto r=calc::solveGuess(equation,c,1);check(!r,noRoot);check(r.evaluations<=768,"bounded solve work");
    }
    check(!equation.compile("x==2"),"reject repeated equals");check(!equation.compile("x="),"reject missing RHS");
    check(equation.compile("2=3"),"constant equation parse");check(!calc::solveGuess(equation,c,0),"reject equation without X");
    check(equation.compile("x+1=3"),"substitution parse");c.x=2;auto substitution=equation.evaluate(c);check(bool(substitution),"substitution");near(substitution.left,3,"LHS");near(substitution.residual(),0,"L-R");
    auto stopped=calc::solveGuess(equation,c,0,[]{return true;});check(!stopped&&stopped.evaluations==0,"solver deadline stops evaluation");
    std::cout<<"PASS: "<<assertions<<" checks\n";
}
