#include "../src/main.cpp"
#include <iostream>
#include <cassert>
namespace hardware {
TFT_eSPI display;bool touchFound=true,keyboardFound=true;
int ballX=0,ballY=0;bool ballClick=false;
void begin(){}void brightness(uint8_t){}char key(){return 0;}
void sampleBattery(){}BatteryInfo batteryInfo(){return {3900,1950,2400,true};}
bool generateQr(const char*){return true;}int qrSize(){return 21;}bool qrModule(int x,int y){return x==0||y==0||x==20||y==20;}
bool touch(int&,int&,bool&){return false;}void ball(int& x,int& y,bool& click,bool& held){x=ballX;y=ballY;click=ballClick;held=false;ballX=ballY=0;ballClick=false;}
}
int checks=0;
void check(bool ok,const char* label){++checks;if(!ok){std::cerr<<"FAIL: "<<label<<"\n";std::exit(1);}}
void type(const string& s){for(char ch:s)keyboard(ch);}
string previewDirectory="/tmp";
void screenshot(const char* path){render();string name=path;name=name.substr(name.find_last_of('/')+1);check(canvas.writePPM((previewDirectory+"/"+name).c_str()),"write preview");}
Button matrixButton(int index){render();for(const auto& b:buttons)if(b.label=="cell"&&index--==0)return b;check(false,"matrix cell hit box exists");return {};}
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
    // Actual matrix grid: no serialized row delimiters in the form or result.
    showForm(Tool::Inverse);check(fields.empty()&&matrixInputs[0].rows==2&&matrixInputs[0].cols==2,"matrix form is a cell model");
    screenshot("deck-matrix.ppm");
    auto cell=matrixButton(1);click(cell.x+cell.w/2,cell.y+cell.h/2);
    check(screen==Screen::Editor&&editTarget==&matrixCell(0,0,1),"touch edits the addressed matrix cell");
    soft(4);type("sqrt(9)");soft(5);check(screen==Screen::Form&&matrixCell(0,0,1)=="sqrt(9)","math editor returns to grid");
    soft(1);check(matrixRow==1&&matrixCol==0,"NEXT traverses row-major cells");
    type("12");check(matrixCell(0,1,0)=="12","first keyboard digit replaces selected cell, later digits append");
    keyboard(8);check(matrixCell(0,1,0)=="1","cell backspace");
    soft(0);check(matrixRow==0&&matrixCol==1,"PREV cell");
    soft(3);check(matrixCell(0,0,1).empty(),"CLR only clears selected cell");type("2");
    selectMatrixCell(0,0);soft(0);check(matrixRow==1&&matrixCol==1,"PREV wraps to final cell");soft(1);check(matrixRow==0&&matrixCol==0,"NEXT wraps to first cell");
    // Drive the real loop with a trackball movement and click.
    cell=matrixButton(2);pointerX=cell.x+cell.w/2-board::PointerStep;pointerY=cell.y+cell.h/2;
    hardware::ballX=1;hardware::ballClick=true;mockMillis+=20;loop();
    check(screen==Screen::Editor&&editTarget==&matrixCell(0,1,0),"trackball selects and edits a grid cell");soft(5);
    // Resize controls, independent operands, and retained hidden entries.
    showForm(Tool::Multiply);click(290,40);check(matrixInputs[0].cols==3,"C+ resizes A");
    matrixCell(0,0,2)="7";click(235,40);click(290,40);check(matrixCell(0,0,2)=="7","shrink and expand preserves cells");
    click(77,40);check(matrixOperand==1&&matrixInputs[1].cols==2,"B has independent dimensions");
    click(181,40);check(matrixInputs[1].rows==3,"R+ resizes B");
    matrixCell(1,2,0)="2";matrixCell(1,2,1)="3";
    execute();check(resultTitle!="Calculation error"&&resultMatrix==calc::Matrix({{15,23},{3,4}}),"rectangular multiplication uses grid operands");
    screenshot("deck-matrix-result.ppm");
    check(resultLines.empty(),"matrix result is not row-separated text");
    cell=matrixButton(3);click(cell.x+2,cell.y+2);check(resultMatrixCell==3,"result cells can be inspected");soft(1);check(resultMatrixCell==0,"result NEXT wraps");soft(0);check(resultMatrixCell==3,"result PREV wraps");
    soft(4);check(screen==Screen::Form&&matrixInputs[0].cols==3&&matrixCell(0,0,2)=="7","BACK preserves matrix operands");
    showForm(Tool::Transpose);resizeMatrix(0,1);matrixCell(0,0,2)="5";matrixCell(0,1,2)="6";
    execute();check(resultMatrix==calc::Matrix({{1,3},{2,4},{5,6}}),"rectangular transpose");
    showForm(Tool::Add);execute();check(resultMatrix==calc::Matrix({{2,2},{3,5}}),"grid matrix addition");
    showForm(Tool::Inverse);execute();check(resultMatrix.size()==2&&std::abs(resultMatrix[0][0]+2)<1e-12,"grid inverse");
    showForm(Tool::Det);execute();check(resultMatrix.empty()&&resultLines[0]=="det(A) = -2","determinant remains scalar, clears old grid result");
    showForm(Tool::Det);resizeMatrix(0,1);execute();check(resultTitle=="Calculation error","non-square determinant rejected");
    showForm(Tool::Inverse);matrixCell(0,1,0)="2";execute();check(resultTitle=="Calculation error","singular matrix rejected");
    showForm(Tool::Add);matrixOperand=1;resizeMatrix(1,0);execute();check(resultTitle=="Calculation error","addition dimension mismatch");
    showForm(Tool::Multiply);resizeMatrix(0,1);execute();check(resultTitle=="Calculation error","multiplication dimension mismatch");
    for(const string& invalid:{"", "1/0", "sqrt(-1)", "1,2", "1;2"}){
        showForm(Tool::Add);matrixCell(1,1,0)=invalid;execute();
        check(resultTitle=="Calculation error"&&resultLines[0].find("B[2,1]")!=string::npos,"invalid cell identifies operand and coordinates");
        soft(4);check(matrixOperand==1&&matrixRow==1&&matrixCol==0,"error BACK focuses failing cell");
    }
    showForm(Tool::Transpose);matrixCell(0,0,0)="max(1,3)";execute();check(resultMatrix[0][0]==3,"function commas stay inside one cell");
    showForm(Tool::Add);matrixCell(0,0,0)="1e308";matrixCell(1,0,0)="1e308";execute();check(resultTitle=="Calculation error","matrix arithmetic overflow rejected");
    showForm(Tool::Transpose);for(int j=0;j<10;++j)resizeMatrix(1,1);
    check(matrixInputs[0].rows==6&&matrixInputs[0].cols==6,"matrix size bounded at 6x6");
    matrixCell(0,5,5)="123456789012";screenshot("deck-matrix-6x6.ppm");
    render();int cellCount=0;for(const auto& b:buttons)if(b.label=="cell"){++cellCount;check(b.x>=0&&b.y>=72&&b.x+b.w<320&&b.y+b.h<=186,"grid hit boxes fit display");}
    check(cellCount==36,"all 36 cells available");execute();check(resultMatrix.size()==6&&resultMatrix[5][5]==123456789012.0,"6x6 evaluation and transpose");screenshot("deck-matrix-result-6x6.ppm");
    soft(4);selectMatrixCell(5,5);for(int j=0;j<10;++j)resizeMatrix(-1,-1);
    check(matrixInputs[0].rows==1&&matrixInputs[0].cols==1&&matrixRow==0&&matrixCol==0,"resize minimum and selection clamping");
    execute();check(resultMatrix.size()==1&&resultMatrix[0].size()==1,"1x1 matrix");
    showForm(Tool::Linear);resizeMatrix(1,0);check(matrixInputs[0].cols==4&&matrixCell(0,0,3)=="5"&&matrixCell(0,0,2)=="0","system resize keeps RHS in final column");
    resizeMatrix(-1,0);execute();check(resultTitle!="Calculation error"&&resultLines[0]=="x1 = 2"&&resultLines[1]=="x2 = 1","augmented grid solves system");
    soft(4);screenshot("deck-matrix-system.ppm");for(int j=0;j<10;++j)resizeMatrix(1,0);
    check(matrixInputs[0].rows==6&&matrixInputs[0].cols==7,"six-variable augmented matrix fits");render();
    for(int r=0;r<6;++r){for(int c=0;c<6;++c)matrixCell(0,r,c)=r==c?"1":"0";matrixCell(0,r,6)=std::to_string(r+1);}
    execute();check(resultLines.size()==6&&resultLines[5]=="x6 = 6","six-variable grid solution");
    save();double savedX=context.x;context.x=0;load();check(context.x==savedX,"persist solved X");
    std::cout<<"PASS: "<<checks<<" UI checks; previews in "<<previewDirectory<<"\n";
}
