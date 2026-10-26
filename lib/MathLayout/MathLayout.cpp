#include "MathLayout.h"
#include <CalcCore.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

namespace mathlayout {
namespace {
struct Node {
    char kind='t';
    std::string text;
    size_t begin=0,end=0,token=0;
    std::vector<int> children;
    int height=1;
};
class Parser {
    const std::string& s;
    size_t p=0;
    int depth=0;
    void space(){while(p<s.size()&&std::isspace(static_cast<unsigned char>(s[p])))++p;}
    int add(Node n){
        if(nodes.size()>=384){ok=false;return -1;}
        for(int child:n.children)if(child>=0)n.height=std::max(n.height,nodes[child].height+1);
        if(n.height>32){ok=false;return -1;}
        nodes.push_back(std::move(n));return int(nodes.size())-1;
    }
    int join(char kind,int a,int b,size_t token,const std::string& text=""){
        if(a<0||b<0)return -1;
        return add({kind,text,nodes[a].begin,nodes[b].end,token,{a,b}});
    }
    int sum(){
        if(++depth>28){ok=false;--depth;return -1;}
        int a=product();space();
        while(ok&&p<s.size()&&(s[p]=='+'||s[p]=='-')){size_t t=p++;int b=product();a=join('r',a,b,t,s.substr(t,1));space();}
        --depth;return a;
    }
    int product(){
        int a=unary();space();
        while(ok&&p<s.size()){
            size_t t=p;char op=s[p];bool implicit=op=='('||std::isalpha(static_cast<unsigned char>(op));
            if(op!='*'&&op!='/'&&!implicit)break;
            if(!implicit)++p;
            int b=unary();a=join(op=='/'?'f':'r',a,b,t,implicit?"":"*");space();
        }
        return a;
    }
    int unary(){
        if(++depth>28){ok=false;--depth;return -1;}space();int a;
        if(p<s.size()&&(s[p]=='+'||s[p]=='-')){size_t t=p++;int child=unary();a=child<0?-1:add({'u',s.substr(t,1),t,nodes[child].end,t,{child}});}
        else a=power();
        --depth;return a;
    }
    int power(){
        int a=atom();space();
        while(ok&&a>=0&&p<s.size()&&(s[p]=='!'||s[p]=='%')){size_t t=p++;a=add({'o',s.substr(t,1),nodes[a].begin,p,t,{a}});space();}
        if(ok&&p<s.size()&&s[p]=='^'){size_t t=p++;int b=unary();a=join('p',a,b,t);}
        return a;
    }
    int atom(){
        space();size_t start=p;
        if(p==s.size()||s[p]==')'||s[p]==','||s[p]=='=')return add({'h',"",p,p,p,{}});
        if(s[p]=='('){++p;int child=sum();space();if(p<s.size()&&s[p]==')')++p;else ok=false;return child<0?-1:add({'g',"",start,p,start,{child}});}
        if(std::isdigit(static_cast<unsigned char>(s[p]))||s[p]=='.'){
            char* end=nullptr;std::strtod(s.c_str()+p,&end);
            if(end==s.c_str()+p){ok=false;return -1;}
            p=size_t(end-s.c_str());return add({'t',s.substr(start,p-start),start,p,start,{}});
        }
        if(std::isalpha(static_cast<unsigned char>(s[p]))){
            while(p<s.size()&&std::isalnum(static_cast<unsigned char>(s[p])))++p;
            std::string name=s.substr(start,p-start),lower=name;
            for(char& ch:lower)ch=std::tolower(static_cast<unsigned char>(ch));
            if(!calc::isFunction(lower))return add({'t',name,start,p,start,{}});
            space();if(p==s.size()||s[p]!='('){ok=false;return -1;}
            size_t opening=p++;std::vector<int> args;
            do {if(!args.empty())++p;args.push_back(sum());space();}while(ok&&p<s.size()&&s[p]==','&&args.size()<4);
            if(p<s.size()&&s[p]==')')++p;else ok=false;
            char kind=(lower=="sqrt"||lower=="cbrt")&&args.size()==1?'s':'c';
            return add({kind,name,start,p,opening,args});
        }
        ok=false;return -1;
    }
public:
    bool ok=true;
    std::vector<Node> nodes;
    explicit Parser(const std::string& source):s(source){}
    int parse(){
        if(s.size()>384){ok=false;return -1;}
        int a=sum();space();
        if(ok&&p<s.size()&&s[p]=='='){size_t t=p++;int b=sum();a=join('r',a,b,t,"=");space();}
        if(p!=s.size())ok=false;
        return a;
    }
};
std::string latex(const Parser& parser,int index,bool& valid,int depth=0){
    if(index<0||depth>32){valid=false;return {};}
    const Node& n=parser.nodes[index];
    auto child=[&](size_t j){return latex(parser,n.children[j],valid,depth+1);};
    if(n.kind=='t'){
        std::string lower=n.text;for(char& c:lower)c=std::tolower(static_cast<unsigned char>(c));
        if(lower=="pi")return "\\pi";
        if(lower=="ans")return "\\operatorname{Ans}";
        size_t e=n.text.find_first_of("eE");
        if(e!=std::string::npos&&e>0&&e+1<n.text.size()&&(std::isdigit(static_cast<unsigned char>(n.text[0]))||n.text[0]=='.'))
            return n.text.substr(0,e)+"\\times 10^{"+n.text.substr(e+1)+"}";
        return n.text;
    }
    if(n.kind=='h'){valid=false;return "\\square";}
    if(n.kind=='r'){
        std::string a=child(0),b=child(1);
        if(n.text.empty())return a+" "+b;
        if(n.text=="*")return a+" \\times "+b;
        return a+n.text+b;
    }
    if(n.kind=='u')return n.text+child(0);
    if(n.kind=='o')return child(0)+(n.text=="%"?"\\%":n.text);
    if(n.kind=='g')return "\\left("+child(0)+"\\right)";
    if(n.kind=='f')return "\\frac{"+child(0)+"}{"+child(1)+"}";
    if(n.kind=='p')return "{"+child(0)+"}^{"+child(1)+"}";
    if(n.kind=='s')return std::tolower(static_cast<unsigned char>(n.text[0]))=='c'?"\\sqrt[3]{"+child(0)+"}":"\\sqrt{"+child(0)+"}";
    std::string name=n.text;for(char& c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::vector<std::string> args;for(size_t j=0;j<n.children.size();++j)args.push_back(child(j));
    if(name=="abs"&&args.size()==1)return "\\left|"+args[0]+"\\right|";
    if(name=="conj"&&args.size()==1)return "\\overline{"+args[0]+"}";
    if(name=="re"&&args.size()==1)return "\\Re\\left("+args[0]+"\\right)";
    if(name=="im"&&args.size()==1)return "\\Im\\left("+args[0]+"\\right)";
    if(name=="sqrt"&&args.size()==1)return "\\sqrt{"+args[0]+"}";
    if(name=="cbrt"&&args.size()==1)return "\\sqrt[3]{"+args[0]+"}";
    if(name=="ncr"&&args.size()==2)return "\\binom{"+args[0]+"}{"+args[1]+"}";
    static const std::pair<const char*,const char*> names[]={
        {"sin","\\sin"},{"cos","\\cos"},{"tan","\\tan"},{"asin","\\arcsin"},{"acos","\\arccos"},{"atan","\\arctan"},
        {"sinh","\\sinh"},{"cosh","\\cosh"},{"tanh","\\tanh"},{"asinh","\\operatorname{asinh}"},{"acosh","\\operatorname{acosh}"},{"atanh","\\operatorname{atanh}"},
        {"ln","\\ln"},{"log","\\log_{10}"},{"log2","\\log_{2}"},{"exp","\\exp"},{"min","\\min"},{"max","\\max"}
    };
    std::string op;
    for(auto& entry:names)if(name==entry.first){op=entry.second;break;}
    if(op.empty())op="\\operatorname{"+name+"}";
    std::string joined;
    for(size_t j=0;j<args.size();++j){if(j)joined+=", ";joined+=args[j];}
    return op+"\\left("+joined+"\\right)";
}
struct Mark {size_t index;Caret caret;};
struct Box {
    int width=0,above=0,below=0;
    std::vector<Stroke> strokes;
    std::vector<Mark> marks;
};
void line(Box& b,int x,int y,int x2,int y2){b.strokes.push_back({Kind::Line,x,y,x2,y2,1,""});}
void place(Box& dest,const Box& source,int x,int y){
    for(auto stroke:source.strokes){stroke.x+=x;stroke.y+=y;stroke.x2+=x;stroke.y2+=y;dest.strokes.push_back(std::move(stroke));}
    for(auto mark:source.marks){mark.caret.x+=x;mark.caret.y+=y;dest.marks.push_back(mark);}
}
Box token(const std::string& s,size_t begin,const Measure& measure,int font){
    Box b;int height=font==1?8:16;b.above=height-3;b.below=3;b.width=measure(s,font);
    b.strokes.push_back({Kind::Text,0,-b.above,0,0,font,s});
    for(size_t i=0;i<=s.size();++i)b.marks.push_back({begin+i,{measure(s.substr(0,i),font),-b.above,height}});
    return b;
}
Box row(const Box& a,const Box& b,int gap=0){
    Box out;out.width=a.width+b.width+gap;out.above=std::max(a.above,b.above);out.below=std::max(a.below,b.below);
    place(out,a,0,0);place(out,b,a.width+gap,0);return out;
}
Box operatorBox(const std::string& op,size_t begin,const Measure& measure,int font){
    Box b=token(op,begin,measure,font);
    if(op=="*"){
        b.strokes.clear();int center=-b.above/2;
        line(b,2,center-3,8,center+3);line(b,2,center+3,8,center-3);
        b.width=11;b.marks.back().caret.x=b.width;
    }
    return b;
}
Box grouped(Box child,size_t begin,size_t end){
    Box b;b.width=child.width+12;b.above=child.above+1;b.below=child.below+1;
    place(b,child,6,0);
    line(b,4,-b.above,1,-b.above+3);line(b,1,-b.above+3,1,b.below-3);line(b,1,b.below-3,4,b.below);
    line(b,b.width-5,-b.above,b.width-2,-b.above+3);line(b,b.width-2,-b.above+3,b.width-2,b.below-3);line(b,b.width-2,b.below-3,b.width-5,b.below);
    b.marks.push_back({begin,{0,-b.above,b.above+b.below}});
    b.marks.push_back({end,{b.width,-b.above,b.above+b.below}});
    return b;
}
Box build(const Parser& parser,int index,const Measure& measure,int font,int depth=0){
    if(index<0||depth>32)return {};
    const Node& n=parser.nodes[index];
    auto child=[&](size_t which,int f){return build(parser,n.children[which],measure,f,depth+1);};
    if(n.kind=='t'){
        std::string lower=n.text;for(char& c:lower)c=std::tolower(static_cast<unsigned char>(c));
        if(lower=="pi"){
            Box b=token(n.text,n.begin,measure,font);b.strokes.clear();b.width=font==1?9:13;
            int top=font==1?-5:-10,bottom=1;
            line(b,1,top,b.width-1,top);line(b,3,top,2,bottom);line(b,b.width-4,top,b.width-4,bottom);line(b,b.width-4,bottom,b.width-1,bottom);
            b.marks.clear();for(size_t i=0;i<=2;++i)b.marks.push_back({n.begin+i,{int(i)*b.width/2,-b.above,b.above+b.below}});return b;
        }
        // Scientific notation is displayed as mantissa x 10^exponent.
        auto e=n.text.find_first_of("eE");
        if(e!=std::string::npos && e>0 && e+1<n.text.size() && (std::isdigit(static_cast<unsigned char>(n.text[0]))||n.text[0]=='.')){
            Box mantissa=token(n.text.substr(0,e),n.begin,measure,font);
            Box base=token("10",n.begin+e,measure,font),exponent=token(n.text.substr(e+1),n.begin+e+1,measure,1);
            // Synthetic "10" has no independent source boundaries.
            base.marks.clear();int rise=base.above-1+exponent.below;
            Box power;power.width=base.width+exponent.width+1;power.above=std::max(base.above,exponent.above+rise);power.below=base.below;
            place(power,base,0,0);place(power,exponent,base.width+1,-rise);
            return row(row(mantissa,operatorBox("*",n.begin+e,measure,font),2),power,2);
        }
        return token(n.text,n.begin,measure,font);
    }
    if(n.kind=='h'){
        Box b;b.width=9;b.above=font==1?6:12;b.below=2;
        line(b,1,-b.above,7,-b.above);line(b,7,-b.above,7,1);line(b,7,1,1,1);line(b,1,1,1,-b.above);
        b.marks.push_back({n.begin,{1,-b.above,b.above+b.below}});return b;
    }
    if(n.kind=='r'){
        Box a=child(0,font),b=child(1,font);
        if(n.text.empty())return row(a,b,2);
        return row(row(a,operatorBox(n.text,n.token,measure,font),2),b,2);
    }
    if(n.kind=='u')return row(operatorBox(n.text,n.token,measure,font),child(0,font));
    if(n.kind=='o')return row(child(0,font),operatorBox(n.text,n.token,measure,font));
    if(n.kind=='g')return grouped(child(0,font),n.begin,n.end);
    if(n.kind=='f'){
        int f=depth>2?1:font;
        Box a=child(0,f),b=child(1,f),out;
        int axis=font==1?-2:-4;
        int ay=axis-3-a.below,by=axis+4+b.above;
        out.width=std::max(a.width,b.width)+8;out.above=a.above-ay;out.below=by+b.below;
        place(out,a,(out.width-a.width)/2,ay);place(out,b,(out.width-b.width)/2,by);
        line(out,1,axis,out.width-2,axis);
        // The slash boundary remains reachable between numerator and denominator.
        out.marks.push_back({n.token,{out.width-1,ay-a.above,a.above+a.below}});
        return out;
    }
    if(n.kind=='p'){
        Box a=child(0,font),b=child(1,1),out;int rise=a.above-2+b.below;
        out.width=a.width+b.width+2;out.above=std::max(a.above,b.above+rise);out.below=a.below;
        place(out,a,0,0);place(out,b,a.width+2,-rise);
        out.marks.push_back({n.token,{a.width,-a.above,a.above+a.below}});return out;
    }
    if(n.kind=='s'){
        Box a=child(0,font),b;int prefix=15;
        b.width=a.width+prefix+3;b.above=a.above+4;b.below=a.below+1;
        place(b,a,prefix,0);
        line(b,1,-2,4,-4);line(b,4,-4,7,b.below-1);line(b,7,b.below-1,12,-b.above);line(b,12,-b.above,b.width-1,-b.above);
        if(std::tolower(static_cast<unsigned char>(n.text[0]))=='c')place(b,token("3",n.begin,measure,1),0,-b.above+6);
        for(size_t i=n.begin;i<=n.token;++i)b.marks.push_back({i,{int(i-n.begin)*prefix/int(n.token-n.begin+1),-b.above,b.above+b.below}});
        b.marks.push_back({n.end,{b.width,-b.above,b.above+b.below}});return b;
    }
    Box arguments;
    for(size_t j=0;j<n.children.size();++j){
        if(j)arguments=row(arguments,token(",",parser.nodes[n.children[j-1]].end,measure,font),1);
        arguments=row(arguments,child(j,font));
    }
    return row(token(n.text,n.begin,measure,font),grouped(arguments,n.token,n.end),1);
}
}

Layout typeset(const std::string& source,const Measure& measure,int font){
    Layout out;Parser p(source);int root=p.parse();Box box;
    if(p.ok&&root>=0){box=build(p,root,measure,font);out.formatted=true;}
    else box=token(source,0,measure,font); // Never prettify invalid syntax into a different valid expression.
    out.width=box.width;out.height=box.above+box.below+1;out.baseline=box.above;
    out.carets.resize(source.size()+1);std::vector<bool> mapped(source.size()+1,false);
    for(auto stroke:box.strokes){stroke.y+=box.above;stroke.y2+=box.above;out.strokes.push_back(std::move(stroke));}
    for(auto mark:box.marks)if(mark.index<out.carets.size()){
        mark.caret.y+=box.above;out.carets[mark.index]=mark.caret;mapped[mark.index]=true;
    }
    Caret previous{0,0,font==1?8:16};
    for(size_t j=0;j<out.carets.size();++j){if(mapped[j])previous=out.carets[j];else out.carets[j]=previous;}
    return out;
}
bool toLatex(const std::string& source,std::string& output){
    Parser parser(source);int root=parser.parse();if(!parser.ok||root<0){output.clear();return false;}
    bool valid=true;std::string expression=latex(parser,root,valid);
    if(!valid){output.clear();return false;}
    output="\\displaystyle "+expression;return true;
}
size_t Layout::hitTest(int x,int y) const {
    size_t best=0;long long distance=std::numeric_limits<long long>::max();
    for(size_t j=0;j<carets.size();++j){
        const auto& c=carets[j];int dy=y<c.y?c.y-y:y>c.y+c.height?y-c.y-c.height:0;
        long long dx=x-c.x,d=dx*dx+4LL*dy*dy;
        if(d<distance){distance=d;best=j;}
    }
    return best;
}
}
