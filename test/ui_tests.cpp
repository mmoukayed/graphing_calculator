#include "../src/main.cpp"
#include <iostream>
#include <cassert>
namespace hardware {
TFT_eSPI display;bool touchFound=true,keyboardFound=true;
void begin(){}void brightness(uint8_t){}char key(){return 0;}
bool touch(int&,int&,bool&){return false;}void ball(int& x,int& y,bool& click,bool& held){x=y=0;click=held=false;}
}
int checks=0;
void check(bool ok,const char* label){++checks;if(!ok){std::cerr<<"FAIL: "<<label<<"\n";std::exit(1);}}
void type(const string& s){for(char ch:s)keyboard(ch);}
string previewDirectory="/tmp";
void screenshot(const char* path){render();string name=path;name=name.substr(name.find_last_of('/')+1);check(canvas.writePPM((previewDirectory+"/"+name).c_str()),"write preview");}
int main(int argc,char** argv){
    if(argc>1)previewDirectory=argv[1];
    setup();check(screen==Screen::Menu,"boot menu");screenshot("/private/tmp/deck-menu.ppm");
    keyboard('1');check(screen==Screen::Run,"keyboard menu shortcut");type("2+3*4\r");check(history.back().output=="14","keyboard EXE");
    click(20,181);check(screen==Screen::Editor,"touch editor");click(25,110);check(runEntry=="7","touch keypad");soft(0);check(screen==Screen::Catalog,"catalog");soft(5);check(screen==Screen::Editor,"catalog back");soft(4);type("sqrt(-1)");soft(5);keyboard('\r');check(history.back().output=="1i","complex workflow");screenshot("/private/tmp/deck-run.ppm");
    keyboard('$');keyboard('2');check(screen==Screen::Functions,"graph menu");click(150,40);check(screen==Screen::Editor,"formula edit");soft(4);type("x^2-2");soft(5);check(graphErrors[0].empty(),"compiled formula");soft(0);check(screen==Screen::Graph,"draw");soft(0);check(trace,"trace toggle");click(GX+GW/2,GY+GH/2);check(std::abs(traceX)<0.1,"touch trace");screenshot("/private/tmp/deck-graph.ppm");
    double oldSpan=xmax-xmin;soft(1);check(std::abs(xmax-xmin-oldSpan/2)<1e-9,"zoom");
    showForm(Tool::Root);fields[0].value="x^2-2";fields[1].value="0";fields[2].value="2";execute();check(screen==Screen::Results&&resultTitle!="Calculation error","root form");check(resultLines[0].find("1.414")!=string::npos,"root value");
    for(Tool tool:{Tool::Minimum,Tool::Maximum,Tool::Derivative,Tool::Integral,Tool::Quadratic,Tool::Cubic,Tool::Linear,Tool::Stats,Tool::Regression,Tool::Det,Tool::Inverse,Tool::Transpose,Tool::Multiply,Tool::Add,Tool::Base,Tool::Payment,Tool::Future,Tool::Present}){showForm(tool);execute();check(screen==Screen::Results&&resultTitle!="Calculation error","default tool calculation");render();}
    showForm(Tool::Intersection);fields[0].value="x";fields[1].value="2-x";execute();check(resultTitle!="Calculation error","intersection");
    showForm(Tool::Window);fields[0].value="10";fields[1].value="-10";execute();check(resultTitle=="Calculation error","invalid window rejected");
    showForm(Tool::TableSetup);fields[1].value="0";execute();check(resultTitle=="Calculation error","zero table step rejected");
    showForm(Tool::TableSetup);fields[0].value="-2";fields[1].value="0.5";execute();check(screen==Screen::Table,"table setup");screenshot("/private/tmp/deck-table.ppm");soft(1);check(tableRow==8,"table paging");
    showForm(Tool::Stats);fields[0].value="1,2,3,4,5";execute();soft(1);check(resultOffset>0,"results paging");
    open(Screen::Run);for(int j=0;j<8;++j){runEntry=std::to_string(j);run();}render();click(260,155);check(historyOffset>0,"history paging");
    context.ans=99;soft(2);check(history.back().output=="7","fraction conversion preserves historical result");
    graphType=1;functions[0]="2";compileGraphs();double x,y;check(graphPoint(0,0,x,y)&&std::abs(x-2)<1e-9&&std::abs(y)<1e-9,"polar plot");
    graphType=2;functions[0]="cos(t)";functions[1]="sin(t)";compileGraphs();check(graphPoint(0,calc::Pi/2,x,y)&&std::abs(x)<1e-9&&std::abs(y-1)<1e-9,"parametric plot");
    context.vars[0]={3,4};context.ans=42;context.angle=calc::Angle::Degrees;save();context={};functions[0]="";graphType=0;load();check(context.vars[0]==Value(3,4)&&context.ans==Value(42)&&graphType==2&&functions[0]=="cos(t)","persistence");
    showEditor(runEntry,Screen::Run);screenshot("/private/tmp/deck-editor.ppm");
    // Dedicated SHIFT -> CALC flow, including retained equation and initial guess.
    open(Screen::Run);runEntry="2x+5=17";cursor=runEntry.size();render();
    click(174,11);check(shifted,"touch SHIFT latch");click(239,11);
    check(screen==Screen::Form&&activeTool==Tool::ShiftSolve&&!shifted,"touch SHIFT CALC");
    check(fields[0].value=="2x+5=17"&&fieldIndex==1,"solve uses current equation and focuses initial X");
    fields[1].value="0";execute();check(resultTitle!="Calculation error"&&std::abs(context.x-6)<1e-8,"solve stores X");
    check(!resultEquation.empty()&&resultLines[1].find("L-R")!=string::npos,"solve equation and residual result");screenshot("deck-shift-calc.ppm");
    soft(4);soft(4);check(screen==Screen::Run&&runEntry=="2x+5=17","solver back retains equation");
    runEntry="x^2+1";cursor=runEntry.size();keyboard('@');
    check(activeTool==Tool::Substitute,"ordinary CALC shortcut");fields[1].value="3";execute();check(std::abs(context.ans-Value(10))<1e-9&&context.x==3,"CALC substitution");
    open(Screen::Run);runEntry="x^2=4";keyboard('#');keyboard('@');check(activeTool==Tool::ShiftSolve,"keyboard SHIFT CALC sequence");fields[1].value="-3";execute();check(std::abs(context.x+2)<1e-8,"negative root from initial guess");
    open(Screen::Run);runEntry="x^2+1=0";keyboard('~');auto beforeX=context.x;auto beforeAns=context.ans;execute();check(resultTitle=="Calculation error"&&context.x==beforeX&&context.ans==beforeAns,"failed solve preserves state");
    open(Screen::Run);runEntry="x+1=5";keyboard('\r');check(activeTool==Tool::ShiftSolve&&screen==Screen::Form,"EXE routes equation to solve");
    open(Screen::Run);runEntry="a=x+1";cursor=runEntry.size();run();check(screen==Screen::Run&&context.vars[0]==Value(context.x+1),"assignments still work with X");
    runEntry.clear();showEditor(runEntry,Screen::Run);render();click(181,110);check(runEntry=="()/()"&&cursor==1,"fraction template insertion");type("1");
    // Move to the denominator and fill its placeholder without changing numerator.
    cursor=5;type("2");check(runEntry=="(1)/(2)","fraction template editing");render();
    auto denominatorCaret=editorLayout.carets[5];click(editorMathX+denominatorCaret.x,editorMathY+denominatorCaret.y+2);check(cursor==5,"tap denominator caret");
    soft(5);run();check(history.back().value==Value(.5),"formatted fraction evaluates source");soft(2);if(history.back().output!="1/2")soft(2);check(history.back().output=="1/2","fraction result notation");
    runEntry="sqrt(1+x^2)/(1+x)";cursor=runEntry.size();showEditor(runEntry,Screen::Run);screenshot("deck-math-editor.ppm");
    runEntry="(1+sqrt(5))/2";cursor=runEntry.size();open(Screen::Run);run();runEntry="x^2+1=5";cursor=runEntry.size();screenshot("deck-math-run.ppm");
    // Math layout geometry and syntax preservation, independent of drawing.
    auto measure=[](const string& s,int f){return canvas.textWidth(s.c_str(),f);};
    auto frac=mathlayout::typeset("1/2",measure);check(frac.formatted&&frac.height>20,"stacked fraction height");
    check(frac.carets[0].y<frac.carets[2].y,"numerator above denominator");
    auto power=mathlayout::typeset("x^2",measure);check(power.formatted&&power.carets[2].y<power.carets[0].y,"raised exponent");
    auto radical=mathlayout::typeset("sqrt(x+1)",measure);int segments=0;for(auto& s:radical.strokes)if(s.kind==mathlayout::Kind::Line)++segments;check(segments>=4,"radical drawn with vinculum");
    auto implicit=mathlayout::typeset("2(x+1)",measure);check(implicit.formatted,"variable parentheses remain multiplication");
    auto unary=mathlayout::typeset("-2^2",measure);check(unary.carets[0].y>unary.carets[3].y,"minus remains outside exponent");
    auto chained=mathlayout::typeset("1/2x",measure);check(chained.carets[3].y<chained.carets[2].y,"implicit product outside denominator");
    for(const string& s:{"sqrt(","(1+", "1 2", "x==2"})check(!mathlayout::typeset(s,measure).formatted,"incomplete/invalid syntax falls back visibly");
    for(const string& s:{"()/()","sqrt()","x^()"})check(mathlayout::typeset(s,measure).formatted,"empty template boxes");
    string deep="1";for(int j=0;j<80;++j)deep+="+1";check(!mathlayout::typeset(deep,measure).formatted,"over-deep layout preserves literal source");
    // Every source boundary stays reachable and layout dimensions remain bounded.
    for(const string& s:{"(x+1)/(x-1)","sqrt(1+x^2)/(2x)","2^3^2","sin(pi/6)","1e-6","x=2","cbrt(x)"}){
        auto l=mathlayout::typeset(s,measure);check(l.formatted&&l.carets.size()==s.size()+1,"layout source mapping");
        for(auto& c:l.carets)check(c.x>=0&&c.x<=l.width&&c.y>=0&&c.y+c.height<=l.height+1,"caret within layout");
    }
    save();double savedX=context.x;context.x=0;load();check(context.x==savedX,"persist solved X");
    std::cout<<"PASS: "<<checks<<" UI checks; previews in "<<previewDirectory<<"\n";
}
