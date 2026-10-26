#include <Arduino.h>
#include <Preferences.h>
#include "Hardware.h"
#include "BoardConfig.h"
#include <CalcCore.h>
#include <Equation.h>
#include <MathLayout.h>
#include <algorithm>
#include <functional>
#include <cerrno>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <utility>

using std::string;
using std::vector;
using calc::Value;
SET_LOOP_TASK_STACK_SIZE(32768);
namespace {
constexpr uint16_t Bg=0xEF5D,Ink=0x1928,Muted=0x6B70,Blue=0x2256,Teal=0x0493,White=0xFFFF,Red=0xD986,Line=0xCE79;
constexpr uint16_t Colors[]={0x21DB,0xE1CA,0x058D,0xA9DB,0xEBC0,0x059C};
enum class Screen {Menu,Run,Functions,Graph,Table,Tools,Form,Results,Settings,Help,Editor,Catalog,UnitCategories,UnitConvert,UnitSelect,SystemInfo,QRCode};
enum class Tool {Root,Minimum,Maximum,Intersection,Derivative,Integral,Quadratic,Cubic,Linear,Stats,Regression,Det,Inverse,Transpose,Multiply,Add,Base,Payment,Future,Present,TableSetup,Window,Substitute,ShiftSolve};
struct Button {int x,y,w,h;string label;std::function<void()> action;};
struct Field {string label,value;};
struct History {string input,output;bool error;Value value;};
struct MatrixInput {
    int rows=2,cols=2;
    std::array<std::array<string,7>,6> cells;
    MatrixInput(){for(auto& row:cells)row.fill("0");}
};
struct UnitDef {string name,symbol;double scale,offset=0,reciprocal=0;};
struct UnitCategory {string name;vector<UnitDef> units;};
Screen screen=Screen::Menu,editorReturn=Screen::Run,catalogReturn=Screen::Run,resultReturn=Screen::Form;
Screen formReturn=Screen::Tools;
Tool activeTool=Tool::Root;
TFT_eSprite canvas(&hardware::display);
Preferences prefs;
calc::Context context;
vector<Button> buttons;
vector<Field> fields;
vector<History> history;
vector<string> resultLines;
std::array<MatrixInput,2> matrixInputs;
calc::Matrix resultMatrix;
int matrixOperand=0,matrixRow=0,matrixCol=0,resultMatrixCell=0;
bool matrixReplace=true;
string unitValue="1";
int unitCategory=0,unitFrom=0,unitTo=1,unitCategoryPage=0,unitListPage=0;
bool selectingSource=true;
string qrEquation="";
Screen qrReturn=Screen::Run;
bool qrReady=false;
string title="",runEntry="",notice="",resultTitle="";
string resultEquation="";
string* editTarget=nullptr;
size_t cursor=0;
int fieldIndex=0,toolGroup=0,catalogPage=0,resultOffset=0,historyOffset=0;
int pointerX=160,pointerY=100,selectedFn=0,digits=10,light=12,graphType=0;
bool pointerVisible=false,dirty=true,trace=false,savePending=false,fractionMode=false;
bool shifted=false;
mathlayout::Layout editorLayout;
int editorMathX=0,editorMathY=0;
uint32_t lastChange=0,noticeUntil=0;
std::array<string,6> functions={"sin(x)","cos(x)","","","",""};
std::array<bool,6> enabled={true,true,false,false,false,false};
std::array<calc::Expression,6> compiled;
std::array<string,6> graphErrors;
double xmin=-10,xmax=10,ymin=-6,ymax=6,traceX=0,tableStart=0,tableStep=1;
double parameterMin=0,parameterMax=2*calc::Pi;
int tableRow=0;
constexpr int GX=24,GY=42,GW=284,GH=135;

void render();void open(Screen s);void execute();void soft(int n);void showTools(int group);
void showForm(Tool tool);void showEditor(string& target,Screen back);void compileGraphs();
void launchCalc(bool solve);
void showQr(const string& source);
string fmt(Value v);
void message(const string& s);
void showResults(const string& heading,const vector<string>& lines,Screen back);
const vector<UnitCategory>& unitCategories(){
    static const vector<UnitCategory> categories={
        {"Length",{{"meter","m",1},{"kilometer","km",1000},{"centimeter","cm",.01},{"millimeter","mm",.001},{"micrometer","um",1e-6},{"nanometer","nm",1e-9},{"inch","in",.0254},{"foot","ft",.3048},{"yard","yd",.9144},{"mile","mi",1609.344},{"nautical mile","nmi",1852},{"fathom","fathom",1.8288},{"astronomical unit","AU",149597870700.0},{"light-year","ly",9.4607304725808e15},{"parsec","pc",3.085677581491367e16}}},
        {"Area",{{"square meter","m2",1},{"square kilometer","km2",1e6},{"square centimeter","cm2",1e-4},{"square foot","ft2",.09290304},{"square inch","in2",.00064516},{"square yard","yd2",.83612736},{"hectare","ha",10000},{"acre","ac",4046.8564224},{"square mile","mi2",2589988.110336}}},
        {"Volume",{{"cubic meter","m3",1},{"liter","L",.001},{"milliliter","mL",1e-6},{"cubic centimeter","cm3",1e-6},{"US gallon","US gal",.003785411784},{"US quart","US qt",.000946352946},{"US pint","US pt",.000473176473},{"US cup","US cup",.0002365882365},{"US fluid ounce","US fl oz",2.95735295625e-5},{"US tablespoon","US tbsp",1.478676478125e-5},{"US teaspoon","US tsp",4.92892159375e-6},{"Imperial gallon","Imp gal",.00454609},{"Imperial quart","Imp qt",.0011365225},{"Imperial pint","Imp pt",.00056826125},{"Imperial fluid ounce","Imp fl oz",2.84130625e-5},{"cubic foot","ft3",.028316846592},{"cubic inch","in3",1.6387064e-5}}},
        {"Mass",{{"kilogram","kg",1},{"gram","g",.001},{"milligram","mg",1e-6},{"microgram","ug",1e-9},{"metric tonne","t",1000},{"ounce","oz",.028349523125},{"pound","lb",.45359237},{"stone","st",6.35029318},{"US short ton","short ton",907.18474},{"UK long ton","long ton",1016.0469088}}},
        {"Temperature",{{"Celsius","deg C",1,273.15},{"Fahrenheit","deg F",5.0/9.0,255.3722222222222},{"Kelvin","K",1},{"Rankine","deg R",5.0/9.0},{"Reaumur","deg Re",1.25,273.15}}},
        {"Speed",{{"meter per second","m/s",1},{"kilometer per hour","km/h",1.0/3.6},{"mile per hour","mph",.44704},{"foot per second","ft/s",.3048},{"knot","kn",.5144444444444445},{"centimeter per second","cm/s",.01}}},
        {"Time",{{"second","s",1},{"millisecond","ms",.001},{"microsecond","us",1e-6},{"minute","min",60},{"hour","h",3600},{"day","d",86400},{"week","wk",604800},{"year (365.25 d)","yr",31557600}}},
        {"Frequency",{{"hertz","Hz",1},{"kilohertz","kHz",1e3},{"megahertz","MHz",1e6},{"gigahertz","GHz",1e9},{"revolution/minute","rpm",1.0/60}}},
        {"Pressure",{{"pascal","Pa",1},{"kilopascal","kPa",1e3},{"megapascal","MPa",1e6},{"bar","bar",1e5},{"millibar","mbar",100},{"standard atmosphere","atm",101325},{"torr","Torr",101325.0/760.0},{"millimeter mercury","mmHg",133.322387415},{"pound/in2","psi",6894.757293168}}},
        {"Energy",{{"joule","J",1},{"kilojoule","kJ",1e3},{"calorie (th)","cal",4.184},{"kilocalorie","kcal",4184},{"watt-hour","Wh",3600},{"kilowatt-hour","kWh",3.6e6},{"electronvolt","eV",1.602176634e-19},{"BTU (IT)","BTU",1055.05585262},{"foot-pound force","ft lbf",1.3558179483314}}},
        {"Power",{{"watt","W",1},{"kilowatt","kW",1e3},{"megawatt","MW",1e6},{"milliwatt","mW",.001},{"mechanical horsepower","hp",745.699871582},{"metric horsepower","PS",735.49875},{"BTU/hour","BTU/h",.293071070172}}},
        {"Force",{{"newton","N",1},{"kilonewton","kN",1e3},{"dyne","dyn",1e-5},{"kilogram-force","kgf",9.80665},{"pound-force","lbf",4.4482216152605}}},
        {"Angle",{{"radian","rad",1},{"degree","deg",calc::Pi/180},{"gradian","grad",calc::Pi/200},{"arcminute","arcmin",calc::Pi/10800},{"arcsecond","arcsec",calc::Pi/648000},{"turn","turn",2*calc::Pi}}},
        {"Data size",{{"bit","bit",1},{"byte","B",8},{"kilobit (SI)","kbit",1e3},{"kilobyte (SI)","kB",8e3},{"megabyte (SI)","MB",8e6},{"gigabyte (SI)","GB",8e9},{"kibibyte","KiB",8192},{"mebibyte","MiB",8388608},{"gibibyte","GiB",8589934592.0}}},
        {"Data rate",{{"bit/second","bit/s",1},{"kilobit/second","kbit/s",1e3},{"megabit/second","Mbit/s",1e6},{"gigabit/second","Gbit/s",1e9},{"byte/second","B/s",8},{"kilobyte/second","kB/s",8e3},{"megabyte/second","MB/s",8e6},{"kibibyte/second","KiB/s",8192},{"mebibyte/second","MiB/s",8388608}}},
        {"Fuel economy",{{"kilometer/liter","km/L",1},{"mile/US gallon","mpg US",.425143707430272},{"mile/Imperial gallon","mpg Imp",.3540061899346476},{"liter/100 kilometer","L/100km",1,0,100}}},
        {"Density",{{"kilogram/m3","kg/m3",1},{"gram/cm3","g/cm3",1000},{"gram/liter","g/L",1},{"kilogram/liter","kg/L",1000},{"pound/ft3","lb/ft3",16.01846337396},{"pound/US gallon","lb/US gal",119.826427316}}},
        {"Torque",{{"newton meter","N m",1},{"newton centimeter","N cm",.01},{"kilonewton meter","kN m",1e3},{"pound-force foot","lbf ft",1.3558179483314},{"pound-force inch","lbf in",.1129848290276},{"kilogram-force meter","kgf m",9.80665}}},
        {"Current",{{"ampere","A",1},{"milliampere","mA",.001},{"microampere","uA",1e-6},{"kiloampere","kA",1000}}},
        {"Voltage",{{"volt","V",1},{"millivolt","mV",.001},{"microvolt","uV",1e-6},{"kilovolt","kV",1000},{"megavolt","MV",1e6}}},
        {"Resistance",{{"ohm","ohm",1},{"milliohm","mohm",.001},{"kiloohm","kohm",1e3},{"megaohm","Mohm",1e6},{"gigaohm","Gohm",1e9}}},
        {"Electric charge",{{"coulomb","C",1},{"ampere-hour","Ah",3600},{"milliampere-hour","mAh",3.6},{"millicoulomb","mC",.001},{"microcoulomb","uC",1e-6},{"elementary charge","e",1.602176634e-19}}},
        {"Capacitance",{{"farad","F",1},{"millifarad","mF",.001},{"microfarad","uF",1e-6},{"nanofarad","nF",1e-9},{"picofarad","pF",1e-12},{"kilofarad","kF",1000}}}
    };
    return categories;
}
const UnitCategory& currentUnitCategory(){return unitCategories()[unitCategory];}
void openUnitPicker(bool source){selectingSource=source;unitListPage=0;open(Screen::UnitSelect);}
void convertUnits(){
    const auto& category=currentUnitCategory();const auto& from=category.units[unitFrom];const auto& to=category.units[unitTo];
    calc::Expression expression;calc::Result parsed;
    if(!expression.compile(unitValue))parsed.error=expression.error();else parsed=expression.evaluate(context,4096);
    if(!parsed||!calc::real(parsed.value)||!calc::finite(parsed.value)){
        string error=!parsed?parsed.error:"Enter a finite real value";
        showResults("Conversion error",{error,"Check the value and try again."},Screen::UnitConvert);return;
    }
    double input=parsed.value.real(),base=from.reciprocal!=0?from.reciprocal/input:input*from.scale+from.offset;
    if(category.name=="Fuel economy"&&input<=0){
        showResults("Conversion error",{"Fuel economy must be above zero.","Check the value and try again."},Screen::UnitConvert);return;
    }
    double output=to.reciprocal!=0?to.reciprocal/base:(base-to.offset)/to.scale;
    if(!std::isfinite(base)||!std::isfinite(output)){
        showResults("Conversion error",{"Result is undefined or out of range.","Check the value and selected units."},Screen::UnitConvert);return;
    }
    showResults(category.name,{fmt(Value(input))+" "+from.symbol,"=",fmt(Value(output))+" "+to.symbol},Screen::UnitConvert);
}
int estimatedBatteryPercent(const hardware::BatteryInfo& info){
    if(!info.valid)return -1;
    static const std::pair<int,int> curve[]={{3300,0},{3500,4},{3600,10},{3700,20},{3750,30},{3800,42},{3850,55},{3900,68},{3950,80},{4000,90},{4100,97},{4200,100}};
    int mv=info.millivolts;
    if(mv<=curve[0].first)return 0;
    for(size_t j=1;j<sizeof(curve)/sizeof(curve[0]);++j)if(mv<=curve[j].first){
        int dv=curve[j].first-curve[j-1].first,dp=curve[j].second-curve[j-1].second;
        return curve[j-1].second+(mv-curve[j-1].first)*dp/dv;
    }
    return 100;
}
string percentEncode(const string& source){
    static const char hex[]="0123456789ABCDEF";string output;output.reserve(source.size()*3);
    for(unsigned char c:source){
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')output.push_back(char(c));
        else{output.push_back('%');output.push_back(hex[c>>4]);output.push_back(hex[c&15]);}
    }
    return output;
}
void showQr(const string& source){
    string latex;
    if(!mathlayout::toLatex(source,latex)){message("Complete a valid expression before QR");return;}
    qrEquation=source;qrReturn=screen;
    string url="https://latex.codecogs.com/svg.image?"+percentEncode(latex);
    qrReady=hardware::generateQr(url.c_str());open(Screen::QRCode);
}
bool matrixTool(){return activeTool==Tool::Linear||activeTool==Tool::Det||activeTool==Tool::Inverse||activeTool==Tool::Transpose||activeTool==Tool::Multiply||activeTool==Tool::Add;}
bool twoMatrices(){return activeTool==Tool::Multiply||activeTool==Tool::Add;}
string& matrixCell(int operand,int row,int col){
    // Keep an augmented system's RHS separate when its dimensions change.
    return matrixInputs[operand].cells[row][activeTool==Tool::Linear&&col==matrixInputs[operand].cols-1?6:col];
}
void selectMatrixCell(int row,int col){
    matrixRow=row;matrixCol=col;cursor=matrixCell(matrixOperand,row,col).size();matrixReplace=true;dirty=true;
}
void stepMatrixCell(int direction){
    auto& m=matrixInputs[matrixOperand];int count=m.rows*m.cols;
    int selected=(matrixRow*m.cols+matrixCol+direction+count)%count;
    selectMatrixCell(selected/m.cols,selected%m.cols);
}
void resizeMatrix(int dr,int dc){
    auto& m=matrixInputs[matrixOperand];
    m.rows=std::max(1,std::min(6,m.rows+dr));
    m.cols=activeTool==Tool::Linear?m.rows+1:std::max(1,std::min(6,m.cols+dc));
    selectMatrixCell(std::min(matrixRow,m.rows-1),std::min(matrixCol,m.cols-1));
}
bool evaluateMatrix(int operand,calc::Matrix& out,string& error){
    auto& m=matrixInputs[operand];out.assign(m.rows,vector<double>(m.cols));
    for(int r=0;r<m.rows;++r)for(int c=0;c<m.cols;++c){
        calc::Expression expression;const string& source=matrixCell(operand,r,c);
        string problem;
        if(!expression.compile(source))problem=expression.error();
        else {auto value=expression.evaluate(context,4096);
            if(!value)problem=value.error;
            else if(!calc::real(value.value)||!calc::finite(value.value))problem="A finite real number is required";
            else out[r][c]=value.value.real();
        }
        if(!problem.empty()){
            matrixOperand=operand;selectMatrixCell(r,c);
            error=string(operand?"B":"A")+"["+std::to_string(r+1)+","+std::to_string(c+1)+"]: "+problem;return false;
        }
    }
    return true;
}
void markSave(){savePending=true;lastChange=millis();}
string fmt(Value v){return calc::format(v,digits);}
void message(const string& s){notice=s;noticeUntil=millis()+3500;dirty=true;}
void text(const string& s,int x,int y,uint16_t color=Ink,int font=2){canvas.setTextColor(color);canvas.drawString(s.c_str(),x,y,font);}
string clipped(const string& s,size_t n){return s.size()>n?s.substr(0,n-2)+"..":s;}
// Render in a clipped viewport. Large editable expressions scroll to the caret;
// read-only previews use smaller glyphs where needed and show overflow markers.
void mathText(const string& source,int x,int y,int w,int h,uint16_t color=Ink,
              bool editing=false,bool remember=false,int preferredFont=2){
    auto measure=[](const string& s,int f){return canvas.textWidth(s.c_str(),f);};
    auto layout=mathlayout::typeset(source,measure,preferredFont);
    if(!editing&&(layout.height>h||layout.width>w))layout=mathlayout::typeset(source,measure,1);
    int ox=x+2,oy=y+std::max(0,(h-layout.height)/2);
    const auto caret=layout.carets[std::min(cursor,source.size())];
    if(editing){
        if(caret.x>w-10)ox-=caret.x-w+10;
        if(caret.y+caret.height>h-2)oy=y+h-2-caret.y-caret.height;
        if(caret.y+oy<y+2)oy=y+2-caret.y;
    }
    canvas.setViewport(x,y,w,h,false);
    canvas.setTextColor(color);
    for(const auto& stroke:layout.strokes){
        if(stroke.kind==mathlayout::Kind::Text)canvas.drawString(stroke.text.c_str(),ox+stroke.x,oy+stroke.y,stroke.font);
        else canvas.drawLine(ox+stroke.x,oy+stroke.y,ox+stroke.x2,oy+stroke.y2,color);
    }
    if(editing)canvas.drawFastVLine(ox+caret.x,oy+caret.y,caret.height,Teal);
    if(ox<x)canvas.fillTriangle(x,y+h/2,x+3,y+h/2-3,x+3,y+h/2+3,Muted);
    if(ox+layout.width>x+w)canvas.fillTriangle(x+w-1,y+h/2,x+w-4,y+h/2-3,x+w-4,y+h/2+3,Muted);
    if(layout.height>h)canvas.drawFastHLine(x+w-8,y+h-2,6,Muted);
    canvas.resetViewport();
    if(remember){editorLayout=std::move(layout);editorMathX=ox;editorMathY=oy;}
}
void button(int x,int y,int w,int h,const string& label,std::function<void()> fn,uint16_t fill=White,uint16_t color=Ink){
    canvas.fillRoundRect(x,y,w,h,4,fill);canvas.drawRoundRect(x,y,w,h,4,Line);
    string fitted=label;
    if(canvas.textWidth(fitted.c_str(),2)>w-4){while(!fitted.empty()&&canvas.textWidth((fitted+"..").c_str(),2)>w-4)fitted.pop_back();fitted+="..";}
    canvas.setTextColor(color);canvas.drawCentreString(fitted.c_str(),x+w/2,y+(h-16)/2,2);
    buttons.push_back({x,y,w,h,label,std::move(fn)});
}
void bar(const std::array<string,6>& labels){
    for(int j=0;j<6;++j)if(!labels[j].empty())button(j*53+1,213,j==5?54:51,26,labels[j],[j]{soft(j);},Blue,White);
}
void header(const string& name){
    canvas.fillRect(0,0,320,24,Ink);button(2,1,44,21,"MENU",[]{open(Screen::Menu);},Ink,White);
    text(clipped(name,20),52,4,White,2);
    auto battery=hardware::batteryInfo();int charge=estimatedBatteryPercent(battery);
    canvas.drawRect(232,6,17,12,White);canvas.fillRect(249,9,2,6,White);
    if(charge>=0)canvas.fillRect(234,8,std::max(1,13*charge/100),8,charge<=15?Red:Teal);
    text(charge<0?"--%":std::to_string(charge)+"%",253,8,charge>=0&&charge<=15?Red:White,1);
    string angle=context.angle==calc::Angle::Radians?"RAD":context.angle==calc::Angle::Degrees?"DEG":"GRA";
    button(277,1,41,21,angle,[]{context.angle=calc::Angle((int(context.angle)+1)%3);compileGraphs();markSave();dirty=true;},Ink,White);
}
void calcKeys(){
    button(146,1,59,21,shifted?"SHIFT*":"SHIFT",[]{shifted=!shifted;dirty=true;},shifted?0xFCE0:Ink,shifted?Ink:White);
    button(208,1,65,21,shifted?"SOLVE":"CALC",[]{launchCalc(shifted);},Ink,White);
}
void open(Screen s){if(screen==Screen::Editor&&editorReturn==Screen::Functions)compileGraphs();screen=s;dirty=true;pointerVisible=false;shifted=false;notice.clear();}
void showEditor(string& target,Screen back){editTarget=&target;editorReturn=back;cursor=target.size();open(Screen::Editor);}
string* currentInput(){if(screen==Screen::Run)return &runEntry;if(screen==Screen::Editor)return editTarget;if(screen==Screen::Form){if(matrixTool())return &matrixCell(matrixOperand,matrixRow,matrixCol);if(!fields.empty())return &fields[fieldIndex].value;}return nullptr;}
void insert(const string& s){auto target=currentInput();if(!target)return;if(target->size()+s.size()>384){message("Maximum 384 characters");return;}cursor=std::min(cursor,target->size());target->insert(cursor,s);cursor+=s.size();dirty=true;markSave();}
void erase(){auto target=currentInput();if(target&&cursor){cursor=std::min(cursor,target->size());target->erase(cursor-1,1);--cursor;dirty=true;markSave();}}
void clearInput(){auto target=currentInput();if(target){target->clear();cursor=0;dirty=true;markSave();}}
void insertTemplate(const string& source,size_t caretOffset){
    auto target=currentInput();if(!target)return;
    size_t before=target->size(),start=std::min(cursor,before);insert(source);
    if(target->size()==before+source.size())cursor=start+caretOffset;
}
void closeEditor(){if(editorReturn==Screen::Functions){compileGraphs();markSave();}open(editorReturn);}
void catalog(){catalogReturn=screen;catalogPage=0;open(Screen::Catalog);}
void showResults(const string& heading,const vector<string>& lines,Screen back=Screen::Form){
    resultTitle=heading;resultLines.clear();resultEquation.clear();resultMatrix.clear();resultMatrixCell=0;
    for(const auto& line:lines){if(line.empty())resultLines.push_back("");else for(size_t j=0;j<line.size();j+=49)resultLines.push_back(line.substr(j,49));}
    resultOffset=0;resultReturn=back;open(Screen::Results);
}
void fail(const string& error){showResults("Calculation error",{error,"Check inputs, then tap BACK."});}
void run(){
    if(runEntry.empty()){if(!history.empty()){runEntry=history.back().input;cursor=runEntry.size();dirty=true;}return;}
    if(runEntry.find('=')!=string::npos){
        string lhs=runEntry.substr(0,runEntry.find('='));
        lhs.erase(std::remove_if(lhs.begin(),lhs.end(),[](unsigned char c){return std::isspace(c);}),lhs.end());
        bool assignment=lhs.size()==1&&std::isalpha(static_cast<unsigned char>(lhs[0]))&&std::tolower(lhs[0])!='x'&&std::tolower(lhs[0])!='t';
        calc::Equation equation;
        if(!assignment&&equation.compile(runEntry)&&equation.usesParameter()){launchCalc(true);return;}
    }
    auto r=calc::calculate(runEntry,context);history.push_back({runEntry,r?fmt(r.value):r.error,!r,r.value});
    if(history.size()>40)history.erase(history.begin());historyOffset=0;
    if(r){runEntry.clear();cursor=0;markSave();}dirty=true;
}
void compileGraphs(){for(int j=0;j<6;++j){if(functions[j].empty()){graphErrors[j].clear();continue;}compiled[j].compile(functions[j]);graphErrors[j]=compiled[j].error();}}
string functionLabel(int j){return graphType==0?"Y"+std::to_string(j+1):graphType==1?"R"+std::to_string(j+1):(j%2?"Y":"X")+std::to_string(j/2+1);}
bool graphPoint(int j,double t,double& x,double& y){
    calc::Context c=context;c.x=t;auto r=compiled[j].evaluate(c,4096);if(!r||!calc::real(r.value))return false;
    x=t;y=r.value.real();
    if(graphType==1){double a=t*(context.angle==calc::Angle::Degrees?calc::Pi/180:context.angle==calc::Angle::Gradians?calc::Pi/200:1);x=y*std::cos(a);y*=std::sin(a);}
    else if(graphType==2){if(j%2||j+1>=6)return false;auto s=compiled[j+1].evaluate(c,4096);if(!s||!calc::real(s.value))return false;x=y;y=s.value.real();}
    return std::isfinite(x)&&std::isfinite(y);
}
bool activeGraph(int j){return enabled[j]&&!functions[j].empty()&&graphErrors[j].empty()&&(graphType!=2||(j%2==0&&!functions[j+1].empty()&&graphErrors[j+1].empty()));}
void selectGraph(int direction){for(int n=0;n<6;++n){selectedFn=(selectedFn+direction+6)%6;if(activeGraph(selectedFn))break;}dirty=true;}
void zoom(double factor){double cx=(xmin+xmax)/2,cy=(ymin+ymax)/2,rx=(xmax-xmin)*factor/2,ry=(ymax-ymin)*factor/2;if(rx<1e-9||ry<1e-9||rx>1e9||ry>1e9)return;xmin=cx-rx;xmax=cx+rx;ymin=cy-ry;ymax=cy+ry;dirty=true;markSave();}
void save(){
    if(!prefs.begin("deckcalc",false))return;
    prefs.putUInt("schema",1);prefs.putInt("angle",int(context.angle));prefs.putInt("digits",digits);prefs.putInt("light",light);prefs.putInt("gtype",graphType);
    for(int j=0;j<6;++j){prefs.putString(("f"+std::to_string(j)).c_str(),functions[j].c_str());prefs.putBool(("on"+std::to_string(j)).c_str(),enabled[j]);}
    prefs.putDouble("xmin",xmin);prefs.putDouble("xmax",xmax);prefs.putDouble("ymin",ymin);prefs.putDouble("ymax",ymax);
    prefs.putDouble("pmin",parameterMin);prefs.putDouble("pmax",parameterMax);
    prefs.putDouble("tstart",tableStart);prefs.putDouble("tstep",tableStep);
    prefs.putDouble("xvalue",context.x);
    double vars[54];for(int j=0;j<26;++j){vars[j*2]=context.vars[j].real();vars[j*2+1]=context.vars[j].imag();}vars[52]=context.ans.real();vars[53]=context.ans.imag();prefs.putBytes("vars",vars,sizeof(vars));
    prefs.end();savePending=false;
}
void load(){
    if(!prefs.begin("deckcalc",true))return;
    if(prefs.getUInt("schema",0)!=1){prefs.end();return;}
    context.angle=calc::Angle(std::max(0,std::min(2,prefs.getInt("angle",0))));digits=std::max(4,std::min(12,prefs.getInt("digits",10)));light=std::max(1,std::min(16,prefs.getInt("light",12)));graphType=std::max(0,std::min(2,prefs.getInt("gtype",0)));
    for(int j=0;j<6;++j){functions[j]=prefs.getString(("f"+std::to_string(j)).c_str(),functions[j].c_str()).c_str();if(functions[j].size()>384)functions[j].clear();enabled[j]=prefs.getBool(("on"+std::to_string(j)).c_str(),enabled[j]);}
    xmin=prefs.getDouble("xmin",-10);xmax=prefs.getDouble("xmax",10);ymin=prefs.getDouble("ymin",-6);ymax=prefs.getDouble("ymax",6);
    if(!std::isfinite(xmin)||!std::isfinite(xmax)||xmax<=xmin){xmin=-10;xmax=10;}
    if(!std::isfinite(ymin)||!std::isfinite(ymax)||ymax<=ymin){ymin=-6;ymax=6;}
    parameterMin=prefs.getDouble("pmin",0);parameterMax=prefs.getDouble("pmax",2*calc::Pi);
    if(!std::isfinite(parameterMin)||!std::isfinite(parameterMax)||parameterMin>=parameterMax){parameterMin=0;parameterMax=2*calc::Pi;}
    tableStart=prefs.getDouble("tstart",0);tableStep=prefs.getDouble("tstep",1);
    context.x=prefs.getDouble("xvalue",0);if(!std::isfinite(context.x)||std::abs(context.x)>1e12)context.x=0;
    if(!std::isfinite(tableStart))tableStart=0;if(!std::isfinite(tableStep)||tableStep==0)tableStep=1;
    double vars[54];if(prefs.getBytesLength("vars")==sizeof(vars)){prefs.getBytes("vars",vars,sizeof(vars));for(int j=0;j<26;++j)if(calc::finite({vars[j*2],vars[j*2+1]}))context.vars[j]={vars[j*2],vars[j*2+1]};if(calc::finite({vars[52],vars[53]}))context.ans={vars[52],vars[53]};}
    prefs.end();
}
void showTools(int group){toolGroup=group;open(Screen::Tools);}
void showForm(Tool tool){
    activeTool=tool;fields.clear();fieldIndex=0;
    formReturn=tool==Tool::Window?Screen::Graph:tool==Tool::TableSetup?Screen::Table:Screen::Tools;
    auto field=[](const string& name,const string& value){fields.push_back({name,value});};
    string f=functions[selectedFn].empty()?"x^2-2":functions[selectedFn];
    switch(tool){
        case Tool::Root:case Tool::Minimum:case Tool::Maximum:case Tool::Integral:
            title=tool==Tool::Root?"SOLVE f(x)=0":tool==Tool::Minimum?"MINIMUM":tool==Tool::Maximum?"MAXIMUM":"INTEGRAL";
            field("f(x)",f);field("Lower bound",tool==Tool::Integral?"0":"-1");field("Upper bound","3");break;
        case Tool::Intersection:title="INTERSECTION";field("f(x)",f);field("g(x)",functions[(selectedFn+1)%6].empty()?"x":functions[(selectedFn+1)%6]);field("Lower bound","0");field("Upper bound","3");break;
        case Tool::Derivative:title="DERIVATIVE";field("f(x)",f);field("At x",fmt(traceX));break;
        case Tool::Quadratic:title="ax^2 + bx + c = 0";field("a","1");field("b","-3");field("c","2");break;
        case Tool::Cubic:title="CUBIC EQUATION";field("a (x^3)","1");field("b (x^2)","-6");field("c (x)","11");field("d","-6");break;
        case Tool::Linear:title="SIMULTANEOUS";break;
        case Tool::Stats:title="1-VARIABLE STATS";field("List (comma separated)","1,2,3,4,5");break;
        case Tool::Regression:title="LINEAR REGRESSION";field("X list","1,2,3,4,5");field("Y list","2,4,5,4,5");break;
        case Tool::Det:case Tool::Inverse:case Tool::Transpose:case Tool::Multiply:case Tool::Add:
            title=tool==Tool::Det?"DETERMINANT":tool==Tool::Inverse?"MATRIX INVERSE":tool==Tool::Transpose?"TRANSPOSE":tool==Tool::Multiply?"MATRIX A x B":"MATRIX A + B";
            break;
        case Tool::Base:title="BASE CONVERSION";field("Integer digits","FF");field("Source base (2..36)","16");break;
        case Tool::Payment:title="LOAN PAYMENT";field("Principal","10000");field("Annual rate (%)","5");field("Number of months","36");break;
        case Tool::Future:case Tool::Present:title=tool==Tool::Future?"FUTURE VALUE":"PRESENT VALUE";field(tool==Tool::Future?"Present amount":"Future amount","1000");field("Rate per period (%)","5");field("Periods","10");break;
        case Tool::TableSetup:title="TABLE SETUP";field("Start",fmt(tableStart));field("Step (nonzero)",fmt(tableStep));break;
        case Tool::Window:title="VIEW WINDOW";field("X min",fmt(xmin));field("X max",fmt(xmax));field("Y min",fmt(ymin));field("Y max",fmt(ymax));field("T/theta min",fmt(parameterMin));field("T/theta max",fmt(parameterMax));break;
        case Tool::ShiftSolve:title="SHIFT CALC / SOLVE";field("Equation in X (or expression = 0)","x^2=2");field("Initial X",calc::format(context.x,12));break;
        case Tool::Substitute:title="CALC / SUBSTITUTE";field("Expression (stored variables used)","x^2");field("X value",calc::format(context.x,12));break;
    }
    if(matrixTool()){
        matrixInputs={MatrixInput{},MatrixInput{}};matrixOperand=0;
        if(tool==Tool::Linear){matrixInputs[0].cols=3;
            matrixInputs[0].cells[0][0]="2";matrixInputs[0].cells[0][1]="1";matrixInputs[0].cells[0][6]="5";
            matrixInputs[0].cells[1][0]="1";matrixInputs[0].cells[1][1]="-1";matrixInputs[0].cells[1][6]="1";
        }else{
            matrixInputs[0].cells[0][0]="1";matrixInputs[0].cells[0][1]="2";
            matrixInputs[0].cells[1][0]="3";matrixInputs[0].cells[1][1]="4";
            matrixInputs[1].cells[0][0]="1";matrixInputs[1].cells[1][1]="1";
        }
        selectMatrixCell(0,0);
    }else cursor=fields[0].value.size();
    open(Screen::Form);
}
void launchCalc(bool solve){
    string source=runEntry;
    if(source.empty()&&!history.empty())source=history.back().input;
    if(source.empty()){message("Enter an expression or equation first");shifted=false;return;}
    showForm(solve?Tool::ShiftSolve:Tool::Substitute);
    fields[0].value=source;fieldIndex=1;cursor=fields[1].value.size();formReturn=Screen::Run;
}
bool scalar(int index,double& value){calc::Expression e;e.compile(fields[index].value);auto r=e.evaluate(context);if(!r||!calc::real(r.value)){fail(r?"Real value required":r.error);return false;}value=r.value.real();return true;}
void execute(){
    vector<string> lines;double a=0,b=0,c=0,d=0;string err;
    auto line=[&](const string& name,Value v){lines.push_back(name+" = "+fmt(v));};
    if(activeTool==Tool::ShiftSolve||activeTool==Tool::Substitute){
        calc::Equation equation;
        if(!equation.compile(fields[0].value)){fail(equation.error());return;}
        if(!scalar(1,a))return;
        if(std::abs(a)>1e12){fail("X must be within +/-1e12");return;}
        if(activeTool==Tool::ShiftSolve){
            uint32_t started=millis();
            auto solved=calc::solveGuess(equation,context,a,[started]{return millis()-started>1500;});
            if(!solved){fail(solved.error);return;}
            context.x=solved.x;context.ans=solved.x;line("X",solved.x);line("L-R",solved.residual);
            lines.push_back("One real solution near the initial X.");
            lines.push_back("Change Initial X for another root.");
        }else{
            calc::Context cc=context;cc.x=a;auto r=equation.evaluate(cc);
            if(!r){fail(r.error);return;}
            context.x=a;context.ans=equation.hasEquals()?r.residual():r.left;
            line("X",a);
            if(equation.hasEquals()){line("Left",r.left);line("Right",r.right);line("L-R",r.residual());}
            else line("Result",r.left);
        }
        markSave();showResults(title,lines);resultEquation=fields[0].value;return;
    }
    if(activeTool==Tool::Window){
        double lo,hi;if(!scalar(0,a)||!scalar(1,b)||!scalar(2,c)||!scalar(3,d)||!scalar(4,lo)||!scalar(5,hi))return;
        if(a>=b||c>=d||lo>=hi||b-a<1e-9||d-c<1e-9||std::max({std::abs(a),std::abs(b),std::abs(c),std::abs(d),std::abs(lo),std::abs(hi)})>1e9){fail("Require increasing bounds within +/-1e9");return;}
        xmin=a;xmax=b;ymin=c;ymax=d;parameterMin=lo;parameterMax=hi;markSave();open(Screen::Graph);return;
    }
    if(activeTool==Tool::TableSetup){if(!scalar(0,a)||!scalar(1,b))return;if(b==0){fail("Step cannot be zero");return;}tableStart=a;tableStep=b;tableRow=0;markSave();open(Screen::Table);return;}
    if(activeTool<=Tool::Integral){
        string source=fields[0].value;int offset=1;if(activeTool==Tool::Intersection){source="("+source+")-("+fields[1].value+")";offset=2;}
        calc::Expression e;if(!e.compile(source)){fail(e.error());return;}
        if(!scalar(offset,a))return;if(activeTool!=Tool::Derivative&&!scalar(offset+1,b))return;
        calc::Result r;
        if(activeTool==Tool::Derivative)r=calc::derivative(e,context,a);
        else if(activeTool==Tool::Integral)r=calc::integrate(e,context,a,b);
        else if(activeTool==Tool::Minimum||activeTool==Tool::Maximum)r=calc::extremum(e,context,a,b,activeTool==Tool::Maximum);
        else r=calc::root(e,context,a,b);
        if(!r){fail(r.error);return;}
        line(activeTool==Tool::Integral?"Integral":activeTool==Tool::Derivative?"dy/dx":"x",r.value);context.ans=r.value;markSave();
        if(activeTool!=Tool::Integral&&activeTool!=Tool::Derivative){calc::Context cc=context;cc.x=r.value.real();calc::Expression original;original.compile(fields[0].value);auto y=original.evaluate(cc);if(y)line("y",y.value);traceX=cc.x;}
        if(activeTool==Tool::Minimum||activeTool==Tool::Maximum){lines.push_back("Numerical interval extremum.");lines.push_back("Use a unimodal interval.");}
    }else if(activeTool==Tool::Quadratic||activeTool==Tool::Cubic){
        if(!scalar(0,a)||!scalar(1,b)||!scalar(2,c))return;
        vector<Value> roots;
        if(activeTool==Tool::Quadratic)roots=calc::quadratic(a,b,c);else{if(!scalar(3,d))return;roots=calc::cubic(a,b,c,d);}
        if(roots.empty()){fail("Degenerate equation: no unique roots");return;}
        for(size_t j=0;j<roots.size();++j)line("x"+std::to_string(j+1),roots[j]);
    }else if(activeTool==Tool::Stats||activeTool==Tool::Regression){
        vector<double> x,y;if(!calc::parseList(fields[0].value,context,x,err)){fail(err);return;}
        if(activeTool==Tool::Stats){auto s=calc::statistics(x);line("n",double(s.n));line("Mean",s.mean);line("Sum",s.sum);line("Sum x^2",s.sumSq);line("Population SD",s.populationSD);line("Sample SD",s.sampleSD);line("Min",s.minimum);line("Q1",s.q1);line("Median",s.median);line("Q3",s.q3);line("Max",s.maximum);}
        else{if(!calc::parseList(fields[1].value,context,y,err)){fail(err);return;}auto r=calc::regression(x,y);if(!r.error.empty()){fail(r.error);return;}lines.push_back("Model: y = a + b*x");line("a",r.intercept);line("b",r.slope);line("r",r.r);line("r^2",r.r*r.r);}
    }else if(activeTool==Tool::Linear||activeTool==Tool::Det||activeTool==Tool::Inverse||activeTool==Tool::Transpose||activeTool==Tool::Multiply||activeTool==Tool::Add){
        calc::Matrix m,n,out;if(!evaluateMatrix(0,m,err)){fail(err);return;}
        if(activeTool==Tool::Linear){vector<double> values;if(!calc::linearSolve(m,values,err)){fail(err);return;}for(size_t j=0;j<values.size();++j)line("x"+std::to_string(j+1),values[j]);}
        if(activeTool==Tool::Det){if(m.size()!=m[0].size()){fail("Square matrix required");return;}double value=calc::determinant(m);if(!std::isfinite(value)){fail("Matrix result overflow");return;}line("det(A)",value);}
        if(activeTool==Tool::Inverse){if(!calc::inverse(m,out,err)){fail(err);return;}}
        if(activeTool==Tool::Transpose)out=calc::transpose(m);
        if(activeTool==Tool::Multiply||activeTool==Tool::Add){if(!evaluateMatrix(1,n,err)){fail(err);return;}
            if(activeTool==Tool::Multiply){if(!calc::multiply(m,n,out)){fail("A columns must equal B rows");return;}}
            else{if(m.size()!=n.size()||m[0].size()!=n[0].size()){fail("Matrix sizes must match");return;}out=m;for(size_t j=0;j<m.size();++j)for(size_t k=0;k<m[0].size();++k)out[j][k]+=n[j][k];}
        }
        if(!out.empty()){
            for(const auto& row:out)for(double value:row)if(!std::isfinite(value)){fail("Matrix result overflow");return;}
            showResults(title,{});resultMatrix=std::move(out);return;
        }
    }else if(activeTool==Tool::Base){
        if(!scalar(1,a))return;if(a<2||a>36||std::floor(a)!=a){fail("Source base must be 2..36");return;}
        errno=0;char* end=nullptr;const char* start=fields[0].value.c_str();long long value=std::strtoll(start,&end,int(a));
        if(end==start||*end||errno==ERANGE){fail("Invalid digits or signed 64-bit overflow");return;}
        for(int base:{2,8,10,16}){unsigned long long u=value<0?0ULL-static_cast<unsigned long long>(value):static_cast<unsigned long long>(value);string s;do{s="0123456789ABCDEF"[u%base]+s;u/=base;}while(u);if(value<0)s="-"+s;lines.push_back("BASE "+std::to_string(base));for(size_t j=0;j<s.size();j+=38)lines.push_back(s.substr(j,38));}
    }else{
        if(!scalar(0,a)||!scalar(1,b)||!scalar(2,c))return;
        if(a<0||c<=0||b<=-100){fail("Amount>=0, periods>0, rate>-100%");return;}
        double value;
        if(activeTool==Tool::Payment){if(b<0||std::floor(c)!=c){fail("Loan rate>=0; whole months required");return;}double r=b/1200;value=r==0?a/c:a*r/(-std::expm1(-c*std::log1p(r)));line("Monthly payment",value);line("Total paid",value*c);line("Total interest",value*c-a);}
        else{value=a*std::pow(1+b/100,activeTool==Tool::Future?c:-c);line(activeTool==Tool::Future?"Future value":"Present value",value);}
        if(!std::isfinite(value)){fail("Overflow: reduce inputs");return;}lines.push_back("End-of-period; no fees or taxes.");
    }
    showResults(title,lines);
}

void drawMenu(){
    header("DECK / CALC");
    const char* names[]={"RUN-MAT","GRAPH","TABLE","EQUATION","STATISTICS","MATRIX","BASE / FIN","SETUP","HELP","CONVERT"};
    const char* subtitles[]={"Scientific & complex","Plot / trace / solve","Function values","Roots & systems","Lists & regression","Up to 6 x 6","Integer & TVM","Angle / display","Keys & syntax","Length, temp + more"};
    for(int j=0;j<10;++j){int x=5+(j%3)*105,y=29+(j/3)*42;
        button(x,y,100,37,"",[j]{switch(j){case 0:open(Screen::Run);cursor=runEntry.size();break;case 1:open(Screen::Functions);break;case 2:open(Screen::Table);break;case 3:showTools(0);break;case 4:showTools(1);break;case 5:showTools(2);break;case 6:showTools(3);break;case 7:open(Screen::Settings);break;case 8:resultOffset=0;open(Screen::Help);break;case 9:unitCategoryPage=0;open(Screen::UnitCategories);break;}});
        text(names[j],x+5,y+3,Ink,2);text(std::to_string(j==9?0:j+1),x+87,y+4,Colors[j%6],1);text(clipped(subtitles[j],16),x+5,y+23,Muted,1);
    }
    text("Touch / trackball  /  keys 1-9, 0",14,204,Muted,1);
    text("Hold trackball or press $ for MENU",14,221,Muted,1);
}
void drawRun(){
    header("RUN-MAT");calcKeys();
    if(history.empty()){text("Ready when you are.",12,40,Ink,4);text("Type an expression, then Enter.",12,76,Muted);text("Try: (2+3i)/(1-i)   or   sin(pi/6)",12,101,Muted,1);text("OPTN opens the function catalog.",12,123,Muted,1);text("Tap the input for a virtual keypad.",12,139,Muted,1);}
    int end=int(history.size())-historyOffset,start=std::max(0,end-2),y=27;
    for(int j=start;j<end;++j){
        mathText(history[j].input,7,y,306,29,Muted);
        if(history[j].error)text(clipped(history[j].output,48),10,y+34,Red,1);
        else mathText(history[j].output,14,y+29,299,29,Blue);
        buttons.push_back({0,y,320,58,"history",[j]{runEntry=history[j].input;cursor=runEntry.size();dirty=true;}});y+=60;
    }
    if(history.size()>2){button(248,149,32,18,"UP",[]{historyOffset=std::min(int(history.size())-2,historyOffset+2);dirty=true;});button(282,149,35,18,"DN",[]{historyOffset=std::max(0,historyOffset-2);dirty=true;});}
    button(212,149,33,18,"QR",[]{if(!runEntry.empty())showQr(runEntry);else if(!history.empty())showQr(history.back().input);else message("Enter an expression first");});
    canvas.fillRect(3,168,314,40,White);canvas.drawRect(3,168,314,40,Line);
    if(runEntry.empty())text("Type here...",8,180,Muted,1);
    else mathText(runEntry,6,170,308,36,Ink,true);
    buttons.push_back({3,168,314,40,"input",[]{showEditor(runEntry,Screen::Run);}});
    bar({"OPTN","Ans","S<>D","DEL","AC","EXE"});
}
void drawFunctions(){
    header(graphType==0?"GRAPH / Y=":graphType==1?"GRAPH / POLAR":"GRAPH / PARAM");
    for(int j=0;j<6;++j){int y=28+j*28;
        button(3,y,41,25,functionLabel(j),[j]{enabled[j]=!enabled[j];selectedFn=j;dirty=true;markSave();},enabled[j]?Colors[j]:Muted,White);
        button(48,y,268,25,functions[j].empty()?"tap to enter":"",[j]{selectedFn=j;showEditor(functions[j],Screen::Functions);});
        if(!functions[j].empty())mathText(functions[j],51,y+1,260,23,graphErrors[j].empty()?Ink:Red);
    }
    text("Tap colored label to enable / disable",7,200,Muted,1);
    bar({"DRAW","TYPE","TABLE","V-WIN","G-SLV","MENU"});
}
double gridStep(double span){double raw=span/6,p=std::pow(10,std::floor(std::log10(raw)));double q=raw/p;return (q<2?1:q<5?2:5)*p;}
int px(double x){return GX+int((x-xmin)/(xmax-xmin)*(GW-1));}
int py(double y){return GY+GH-1-int((y-ymin)/(ymax-ymin)*(GH-1));}
void drawGraph(){
    header(trace?"GRAPH / TRACE":"GRAPH");
    text("x:"+calc::format(xmin,4)+".."+calc::format(xmax,4)+"  y:"+calc::format(ymin,4)+".."+calc::format(ymax,4),5,28,Muted,1);
    canvas.fillRect(GX,GY,GW,GH,White);
    double sx=gridStep(xmax-xmin),sy=gridStep(ymax-ymin);
    for(double x=std::ceil(xmin/sx)*sx;x<=xmax;x+=sx)canvas.drawFastVLine(px(x),GY,GH,Line);
    for(double y=std::ceil(ymin/sy)*sy;y<=ymax;y+=sy)canvas.drawFastHLine(GX,py(y),GW,Line);
    if(xmin<=0&&xmax>=0)canvas.drawFastVLine(px(0),GY,GH,Muted);
    if(ymin<=0&&ymax>=0)canvas.drawFastHLine(GX,py(0),GW,Muted);
    bool any=false,timedOut=false;uint32_t plotStarted=millis();
    for(int j=0;j<6&&!timedOut;++j){if(!activeGraph(j))continue;any=true;bool previous=false;int prevX=0,prevY=0;double prevValue=0;
        int samples=graphType==0?GW:560;double lo=graphType==0?xmin:parameterMin,hi=graphType==0?xmax:parameterMax;
        for(int k=0;k<samples;++k){
            if(millis()-plotStarted>350){timedOut=true;break;}
            double t=lo+(hi-lo)*k/(samples-1),x,y;
            if(!graphPoint(j,t,x,y)||x<xmin||x>xmax||y<ymin||y>ymax){previous=false;continue;}
            int ix=px(x),iy=py(y);
            if(previous&&std::abs(iy-prevY)<GH/2&&std::abs(ix-prevX)<GW/2){
                // Midpoint check avoids drawing a false bridge across most poles.
                double mx,my;bool ok=graphPoint(j,t-(hi-lo)/(samples-1)/2,mx,my);
                if(ok&&my>=ymin&&my<=ymax&&std::abs(my-(y+prevValue)/2)<(ymax-ymin)/8)canvas.drawLine(prevX,prevY,ix,iy,Colors[j]);
            }else canvas.drawPixel(ix,iy,Colors[j]);
            previous=true;prevX=ix;prevY=iy;prevValue=y;
        }
    }
    canvas.drawRect(GX,GY,GW,GH,Muted);
    if(!any)text("Enable a valid function in Y=",32,96,Red,2);
    if(trace){double x,y;if(graphPoint(selectedFn,traceX,x,y)){if(x>=xmin&&x<=xmax&&y>=ymin&&y<=ymax){canvas.drawCircle(px(x),py(y),4,Colors[selectedFn]);canvas.drawFastVLine(px(x),GY,GH,Colors[selectedFn]);}text(functionLabel(selectedFn)+" x="+calc::format(x,7)+" y="+calc::format(y,7),4,184,Colors[selectedFn],1);}else text("Undefined / budget at "+fmt(traceX),4,184,Red,1);text("Roll: trace / choose curve. Tap: x",4,199,Muted,1);}
    else{text("Tap plot to trace. Roll to move cursor.",4,185,Muted,1);text("Zoom centers on the view window.",4,200,Muted,1);}
    if(timedOut){canvas.fillRect(0,181,320,29,Bg);text("Partial plot: time limit reached.",4,184,Red,1);text("Simplify expressions to draw all curves.",4,199,Red,1);}
    bar({trace?"CURSOR":"TRACE","ZOOM+","ZOOM-","V-WIN","G-SLV","Y="});
}
void drawTable(){
    header("TABLE / "+functionLabel(selectedFn));
    text("x / t",10,31,Muted);text(functionLabel(selectedFn),112,31,Colors[selectedFn]);text(functionLabel((selectedFn+1)%6),215,31,Colors[(selectedFn+1)%6]);
    for(int j=0;j<8;++j){double x=tableStart+(tableRow+j)*tableStep;int y=53+j*19;if(j%2==0)canvas.fillRect(4,y-1,312,19,White);text(calc::format(x,6),10,y,Ink,1);
        for(int k=0;k<2;++k){int fn=(selectedFn+k)%6;calc::Context cc=context;cc.x=x;auto r=compiled[fn].evaluate(cc,4096);text(functions[fn].empty()?"--":r?calc::format(r.value,7):"undefined",112+k*103,y,Colors[fn],1);}}
    bar({"UP","DOWN","FUNC","SET","GRAPH","MENU"});
}
const vector<std::pair<string,Tool>>& toolsForGroup(){
    static const vector<std::pair<string,Tool>> equation={{"SHIFT CALC",Tool::ShiftSolve},{"Bracketed root",Tool::Root},{"Quadratic",Tool::Quadratic},{"Cubic",Tool::Cubic},{"Simultaneous",Tool::Linear},{"Derivative",Tool::Derivative},{"Integral",Tool::Integral}};
    static const vector<std::pair<string,Tool>> stats={{"1-variable stats",Tool::Stats},{"Linear regression",Tool::Regression}};
    static const vector<std::pair<string,Tool>> matrix={{"Determinant",Tool::Det},{"Inverse",Tool::Inverse},{"Transpose",Tool::Transpose},{"Multiply A x B",Tool::Multiply},{"Add A + B",Tool::Add},{"Linear system",Tool::Linear}};
    static const vector<std::pair<string,Tool>> finance={{"Base conversion",Tool::Base},{"Loan payment",Tool::Payment},{"Future value",Tool::Future},{"Present value",Tool::Present}};
    static const vector<std::pair<string,Tool>> graph={{"Root",Tool::Root},{"Minimum",Tool::Minimum},{"Maximum",Tool::Maximum},{"Intersection",Tool::Intersection},{"Derivative",Tool::Derivative},{"Integral",Tool::Integral}};
    return toolGroup==0?equation:toolGroup==1?stats:toolGroup==2?matrix:toolGroup==3?finance:graph;
}
void drawTools(){
    header(toolGroup==0?"EQUATION":toolGroup==1?"STATISTICS":toolGroup==2?"MATRIX":toolGroup==3?"BASE / FINANCE":"GRAPH SOLVE");
    auto& items=toolsForGroup();int rowHeight=items.size()>6?43:53;
    for(size_t j=0;j<items.size();++j)button(7+(j%2)*157,30+(j/2)*rowHeight,149,rowHeight-6,items[j].first,[j]{showForm(toolsForGroup()[j].second);});
    if(toolGroup==1){text("Distributions are in RUN > OPTN.",8,150,Muted,1);text("Normal, binomial and Poisson PDF/CDF",8,169,Muted,1);}
    bar({"","","","","",toolGroup==4?"GRAPH":"MENU"});
}
// Values stay in aligned cells inside one pair of brackets. Long expressions
// are abbreviated here; EDIT opens their full, scrollable math layout.
void matrixValue(const string& value,int x,int y,int w,int h,uint16_t color){
    auto measure=[](const string& s,int f){return canvas.textWidth(s.c_str(),f);};
    auto layout=mathlayout::typeset(value,measure,2);int font=2;
    if(layout.width>w-4||layout.height>h){font=1;layout=mathlayout::typeset(value,measure,1);}
    if(layout.width<=w-4&&layout.height<=h){
        int width=layout.width+4;mathText(value,x+(w-width)/2,y,width,h,color,false,false,font);
    }else{
        string fitted=value;
        if(canvas.textWidth(fitted.c_str(),1)>w-4){
            while(!fitted.empty()&&canvas.textWidth((fitted+"..").c_str(),1)>w-4)fitted.pop_back();
            fitted+="..";
        }
        canvas.setTextColor(color);canvas.drawCentreString(fitted.c_str(),x+w/2,y+(h-8)/2,1);
    }
}
void matrixGrid(int rows,int cols,int selected,bool augmented,
                const std::function<string(int,int)>& value,
                const std::function<void(int,int)>& select){
    constexpr int left=29,width=264;
    int cw=width/cols,ch=std::min(48,114/rows),top=72+(114-ch*rows)/2;
    for(int c=0;c<cols;++c){
        string label=augmented?(c==cols-1?"RHS":"x"+std::to_string(c+1)):std::to_string(c+1);
        canvas.setTextColor(Muted);canvas.drawCentreString(label.c_str(),left+c*cw+cw/2,62,1);
    }
    for(int r=0;r<rows;++r){
        text(std::to_string(r+1),6,top+r*ch+(ch-8)/2,Muted,1);
        for(int c=0;c<cols;++c){
            int x=left+c*cw,y=top+r*ch;bool active=r*cols+c==selected;
            canvas.fillRect(x+1,y+1,cw-2,ch-2,active?0xD71E:White);
            if(active)canvas.drawRect(x+1,y+1,cw-2,ch-2,Teal);
            matrixValue(value(r,c),x+3,y+2,cw-6,ch-4,Ink);
            buttons.push_back({x,y,cw,ch,"cell",[select,r,c]{select(r,c);}});
        }
    }
    int right=left+cols*cw+7,bottom=top+rows*ch;
    for(int x:{left-8,right}){
        canvas.drawFastVLine(x,top,bottom-top,Ink);
        canvas.drawFastHLine(x==right?x-6:x,top,7,Ink);
        canvas.drawFastHLine(x==right?x-6:x,bottom-1,7,Ink);
    }
    if(augmented)canvas.drawFastVLine(left+(cols-1)*cw,top,rows*ch,Muted);
}
void drawMatrixForm(){
    header(title);auto& m=matrixInputs[matrixOperand];
    button(3,28,48,24,activeTool==Tool::Linear?"SYS":"A",[]{matrixOperand=0;selectMatrixCell(0,0);},matrixOperand==0?Teal:White,matrixOperand==0?White:Ink);
    if(twoMatrices())button(55,28,48,24,"B",[]{matrixOperand=1;selectMatrixCell(0,0);},matrixOperand==1?Teal:White,matrixOperand==1?White:Ink);
    button(107,28,48,24,activeTool==Tool::Linear?"N-":"R-",[]{resizeMatrix(-1,0);});
    button(159,28,48,24,activeTool==Tool::Linear?"N+":"R+",[]{resizeMatrix(1,0);});
    if(activeTool!=Tool::Linear){
        button(211,28,48,24,"C-",[]{resizeMatrix(0,-1);});
        button(263,28,54,24,"C+",[]{resizeMatrix(0,1);});
    }
    text(std::to_string(m.rows)+"x"+std::to_string(m.cols),5,54,Muted,1);
    matrixGrid(m.rows,m.cols,matrixRow*m.cols+matrixCol,activeTool==Tool::Linear,
        [](int r,int c){return matrixCell(matrixOperand,r,c);},
        [](int r,int c){selectMatrixCell(r,c);showEditor(matrixCell(matrixOperand,r,c),Screen::Form);});
    string label=string(matrixOperand?"B":"A")+"["+std::to_string(matrixRow+1)+","+std::to_string(matrixCol+1)+"]";
    button(3,189,314,21,"",[]{showEditor(matrixCell(matrixOperand,matrixRow,matrixCol),Screen::Form);});
    text(label,8,196,Teal,1);matrixValue(matrixCell(matrixOperand,matrixRow,matrixCol),65,191,246,17,Ink);
    bar({"PREV","NEXT","EDIT","CLR","BACK","EXE"});
}
void drawUnitCategories(){
    header("UNIT CONVERTER / CATEGORY");const auto& categories=unitCategories();
    constexpr int perPage=12;int first=unitCategoryPage*perPage;
    for(int j=first;j<std::min(first+perPage,int(categories.size()));++j){
        int local=j-first,x=5+(local%2)*157,y=30+(local/2)*28;
        button(x,y,150,25,categories[j].name,[j]{unitCategory=j;unitFrom=0;unitTo=1;open(Screen::UnitConvert);});
    }
    text("Choose a quantity to convert.",8,202,Muted,1);
    bar({"PREV","NEXT","","","","BACK"});
}
void drawUnitConvert(){
    header("UNIT CONVERTER");const auto& category=currentUnitCategory();
    button(4,28,312,25,category.name+"  /  CHANGE",[]{unitCategoryPage=unitCategory/12;open(Screen::UnitCategories);},0xD71E,Ink);
    text("Value",8,57,Muted,1);
    button(4,69,312,39,"",[]{showEditor(unitValue,Screen::UnitConvert);});
    if(unitValue.empty())text("Tap VALUE to enter a number",9,83,Muted,1);
    else mathText(unitValue,8,71,304,35,Ink);
    text("From",8,112,Muted,1);
    button(4,123,312,31,category.units[unitFrom].name+"  ("+category.units[unitFrom].symbol+")",[]{openUnitPicker(true);});
    text("To",8,158,Muted,1);
    button(4,169,312,31,category.units[unitTo].name+"  ("+category.units[unitTo].symbol+")",[]{openUnitPicker(false);});
    text("Expressions allowed, e.g. pi/2",8,199,Muted,1);
    bar({"VALUE","FROM","TO","SWAP","BACK","EXE"});
}
void drawUnitSelect(){
    const auto& category=currentUnitCategory();header(string(selectingSource?"FROM / ":"TO / ")+category.name);
    const auto& units=category.units;constexpr int perPage=12;int first=unitListPage*perPage;
    text("Choose a unit",7,28,Muted,1);
    for(int j=first;j<std::min(first+perPage,int(units.size()));++j){
        int local=j-first,x=5+(local%2)*157,y=42+(local/2)*27;
        button(x,y,150,24,units[j].name+" ("+units[j].symbol+")",[j]{if(selectingSource)unitFrom=j;else unitTo=j;open(Screen::UnitConvert);});
    }
    bar({"PREV","NEXT","","","","BACK"});
}
void drawForm(){
    if(matrixTool()){drawMatrixForm();return;}
    header(clipped(title,24));int page=fieldIndex/4,first=page*4;
    if(activeTool==Tool::ShiftSolve||activeTool==Tool::Substitute){
        text(fields[0].label,8,29,Muted,1);
        button(5,41,310,65,"",[]{fieldIndex=0;showEditor(fields[0].value,Screen::Form);});
        mathText(fields[0].value,9,44,302,58);
        text(fields[1].label,8,114,Muted,1);
        button(5,127,310,35,"",[]{fieldIndex=1;showEditor(fields[1].value,Screen::Form);},fieldIndex==1?0xD71E:White);
        mathText(fields[1].value,10,131,298,27,Ink,fieldIndex==1);
        text(activeTool==Tool::ShiftSolve?"Enter an initial guess, then EXE.":"Enter X, then EXE to substitute.",8,176,Muted,1);
        text("Other letters use stored variable values.",8,194,Muted,1);
        bar({"PREV","NEXT","EDIT","OPTN","BACK","EXE"});return;
    }
    for(int j=first;j<std::min(first+4,int(fields.size()));++j){int y=29+(j-first)*42;
        text(fields[j].label,8,y,Muted,1);button(5,y+12,310,27,"",[j]{fieldIndex=j;showEditor(fields[j].value,Screen::Form);},j==fieldIndex?0xD71E:White);
        mathText(fields[j].value,9,y+14,302,23);
    }
    text("Tap field to edit. Enter = calculate.",8,201,Muted,1);
    bar({"PREV","NEXT","EDIT","OPTN","BACK","EXE"});
}
void drawResults(){
    header(clipped(resultTitle,24));int y=32;
    if(!resultMatrix.empty()){
        int rows=int(resultMatrix.size()),cols=int(resultMatrix[0].size());
        text("Result: "+std::to_string(rows)+" x "+std::to_string(cols),8,31,Blue);
        text("Select a cell to read its full value.",8,51,Muted,1);
        matrixGrid(rows,cols,resultMatrixCell,false,
            [](int r,int c){return calc::format(resultMatrix[r][c],digits);},
            [cols](int r,int c){resultMatrixCell=r*cols+c;dirty=true;});
        int r=resultMatrixCell/cols,c=resultMatrixCell%cols;
        text("["+std::to_string(r+1)+","+std::to_string(c+1)+"] =",8,196,Teal,1);
        matrixValue(calc::format(resultMatrix[r][c],digits),65,189,249,21,Ink);
        bar({"PREV","NEXT","","","BACK","MENU"});return;
    }
    if(!resultEquation.empty()){
        mathText(resultEquation,6,28,308,60);
        canvas.drawFastHLine(6,92,308,Line);y=101;
        for(int j=resultOffset;j<std::min(resultOffset+5,int(resultLines.size()));++j){
            if(resultLines[j].find('=')!=string::npos)mathText(resultLines[j],7,y,306,20,Blue);
            else text(resultLines[j],7,y+5,Muted,1);
            y+=21;
        }
        bar({"UP","DOWN","","QR","BACK","MENU"});return;
    }
    for(int j=resultOffset;j<std::min(resultOffset+9,int(resultLines.size()));++j){text(clipped(resultLines[j],50),7,y,Ink,1);y+=19;}
    bar({"UP","DOWN","","","BACK","MENU"});
}
void drawSettings(){
    header("SETUP");
    button(8,32,304,30,"Angle: "+string(context.angle==calc::Angle::Radians?"Radians":context.angle==calc::Angle::Degrees?"Degrees":"Gradians"),[]{context.angle=calc::Angle((int(context.angle)+1)%3);markSave();dirty=true;});
    button(8,68,304,30,"Significant digits: "+std::to_string(digits),[]{digits=digits==12?4:digits+1;markSave();dirty=true;});
    button(8,104,304,30,"Brightness: "+std::to_string(light)+" / 16",[]{light=light==16?1:light+1;hardware::brightness(light);markSave();dirty=true;});
    button(8,140,304,30,"Graph type: "+string(graphType==0?"Cartesian":graphType==1?"Polar":"Parametric"),[]{graphType=(graphType+1)%3;trace=false;markSave();dirty=true;});
    auto battery=hardware::batteryInfo();int charge=estimatedBatteryPercent(battery);
    string batteryLine=battery.valid?"BAT "+calc::format(battery.millivolts/1000.0,4)+"V  ~"+std::to_string(charge)+"%":"BATTERY ADC UNAVAILABLE";
    text(batteryLine,8,179,charge>=0&&charge<=15?Red:Ink,1);
    text("Voltage based estimate; no fuel gauge",8,195,Muted,1);
    button(226,177,86,29,"SYSTEM",[]{open(Screen::SystemInfo);});
    bar({"","","","","SAVE","MENU"});
}
void drawSystemInfo(){
    header("SYSTEM INFO");auto battery=hardware::batteryInfo();int charge=estimatedBatteryPercent(battery);
    text("BATTERY",7,28,Teal,1);
    text(battery.valid?"Voltage: "+calc::format(battery.millivolts/1000.0,4)+" V   estimate "+std::to_string(charge)+"%":"Voltage: unavailable (GPIO 4 ADC)",7,40,Ink,1);
    text("ADC pin: "+std::to_string(battery.adcMillivolts)+" mV; raw "+std::to_string(battery.raw),7,53,Muted,1);
    text("No fuel gauge; charge state not MCU-readable.",7,66,Muted,1);
    text("ESP32",7,82,Teal,1);
    text(string("Chip: ")+(ESP.getChipModel()?ESP.getChipModel():"unknown")+" rev "+std::to_string(ESP.getChipRevision()),7,94,Ink,1);
    text("CPU: "+std::to_string(ESP.getCpuFreqMHz())+" MHz; "+std::to_string(ESP.getChipCores())+" cores",7,107,Ink,1);
    text("Flash: "+std::to_string(ESP.getFlashChipSize()/1048576)+" MB",7,120,Ink,1);
    text("PSRAM: "+std::to_string(ESP.getPsramSize()/1024)+" KB total, "+std::to_string(ESP.getFreePsram()/1024)+" KB free",7,133,Ink,1);
    text("Heap: "+std::to_string(ESP.getHeapSize()/1024)+" KB total, "+std::to_string(ESP.getFreeHeap()/1024)+" KB free",7,146,Ink,1);
    text(string("SDK: ")+(ESP.getSdkVersion()?ESP.getSdkVersion():"unknown"),7,159,Ink,1);
    text("Uptime: "+std::to_string(millis()/1000)+" seconds",7,172,Ink,1);
    text(string("Touch: ")+(hardware::touchFound?"GT911 detected":"not detected")+"  Keyboard: "+(hardware::keyboardFound?"OK":"missing"),7,188,Muted,1);
    bar({"","","","","BACK","MENU"});
}
void drawQrCode(){
    header("QR / LATEX");
    if(!qrReady||hardware::qrSize()<=0){
        text("QR code too large or unavailable.",12,83,Red,2);
        text("Try a shorter expression.",12,108,Muted,1);
        mathText(qrEquation,8,132,304,54,Ink);
    }else{
        int n=hardware::qrSize(),total=n+8;
        int scale=std::min(6,std::min((320-16)/total,(240-24-26)/total));
        int side=total*scale,x0=(320-side)/2,y0=24+(189-side)/2;
        canvas.fillRect(x0,y0,side,side,White);
        for(int y=0;y<n;++y){
            int run=-1;
            for(int x=0;x<=n;++x){
                bool black=x<n&&hardware::qrModule(x,y);
                if(black&&run<0)run=x;
                else if(!black&&run>=0){canvas.fillRect(x0+(run+4)*scale,y0+(y+4)*scale,(x-run)*scale,scale,Ink);run=-1;}
            }
        }
    }
    bar({"","","","","BACK","MENU"});
}
const vector<string>& helpLines(){static const vector<string> lines={
    "NEW: SHIFT CALC / SOLVE",
    "RUN: type 2x+5=17, tap SHIFT then CALC.",
    "Enter Initial X, then EXE. X=6.",
    "L-R shows the numerical residual.",
    "CALC alone evaluates at an entered X.",
    "Keyboard: # then @ = SHIFT CALC.",
    "~ = direct SOLVE; @ = ordinary CALC.",
    "In RUN, x/t use the last saved X.",
    "",
    "MATH EDIT: fractions, powers, radicals.",
    "a/b, x^n and sqrt insert templates.",
    "Fill boxes; tap or use < > to move.",
    "Source below math shows exact input.",
    "S<>D displays a stacked fraction.",
    "Formatting does not add exact surds.",
    "",
    "DECK CALC - CG50-inspired workflows", "Touch tiles, fields and the six soft keys.","Trackball moves cursor; click selects.","Hold trackball (0.7s) or $ opens MENU.","Keyboard Enter = EXE. Backspace = DEL.","Esc = BACK. Tab cycles soft keys.","On MENU, keys 1..9 open apps; 0 opens CONVERT.","Tap input to open the virtual keypad.","Editor: < > move cursor; AC clears.","OPTN inserts scientific functions.","Numbers: use keyboard Alt or keypad.","", "Syntax: 2x, 2(x+1), x^2, 5!, 25%", "Power before unary minus: -2^2 = -4", "pi, e, i, ans; a=5 stores a variable.","Letters e,i,x,t are reserved.","x / t are the graph parameter.","Complex: sqrt(-1), (2+3i)/(1-i)","Functions: sin, asin, sinh, ln, log", "re, im, conj, abs, arg, ncr, npr", "diff(x^3,2) = derivative at x=2", "integral(sin(x),0,pi) = integral", "solve(x^2-2,0,2) = bracketed root", "Normal: normalpdf(z), normalcdf(z)","Binomial: binompdf(n,p,k), binomcdf", "Poisson: poissonpdf(lambda,k), CDF", "", "GRAPH: tap colored Y label to toggle.","Tap formula to edit, DRAW to plot.","TRACE: roll X; roll Y changes curve.","Click in TRACE returns to cursor mode.","Tap plot selects trace parameter.","V-WIN edits axes and T/theta bounds.","Polar uses current angle unit.","Parametric pairs: X1,Y1; X2,Y2; X3,Y3.","G-SLV tools use real Cartesian f(x).", "Root bounds must bracket a sign change.","Min/max require a unimodal interval.","Integrals need a continuous interval.","Graphs approximate; poles can be missed.","", "TABLE: FUNC cycles function columns.","STAT: commas separate data values.","Quartiles exclude the median (odd n).", "MATRIX: tap a grid cell to edit.", "A/B tabs choose the operand.", "R-/R+ rows; C-/C+ columns. Max 6x6.", "PREV/NEXT select; typing replaces cell.", "EDIT preserves it; CLR clears one cell.", "EXE calculates. BACK keeps your input.", "Results: select a cell for full value.", "SIMULT: coefficient grid | RHS.", "N-/N+ set the number of unknowns (1-6).","CONVERT: choose category, value, FROM, TO.", "SWAP reverses the units; EXE converts.", "Includes metric, US/Imperial and SI units.", "Temperature and fuel-economy scales included.","RUN QR encodes a LaTeX-rendered equation link.","Scanning needs Internet; the equation is shared.", "SETUP shows battery and SYSTEM INFO.","Battery percent is a voltage estimate only.", "BASE uses signed 64-bit integers.","Finance uses ordinary compounding.","Settings, graph entries, variables and", "Ans save after 1.5 seconds idle.","Calculation history is session-only.","", "Limits: numerical double precision.","No CAS, exact surds, 3D, spreadsheet,", "Python or Casio program compatibility.","Independent firmware; no Casio ROM."};return lines;}
void drawHelp(){header("HELP");auto& lines=helpLines();for(int j=0;j<10&&resultOffset+j<int(lines.size());++j)text(lines[resultOffset+j],5,31+j*17,Ink,1);bar({"UP","DOWN","","","","MENU"});}
void drawEditor(){
    header("MATH EDIT");if(editorReturn==Screen::Run)calcKeys();
    if(!editTarget)return;
    mathText(*editTarget,5,27,310,54,Ink,true,true);
    size_t start=cursor>47?cursor-47:0;
    text(editTarget->substr(start,50),7,86,Muted,1);
    canvas.drawFastHLine(7+int(cursor-start)*6,95,5,Teal);
    const char* keys[]={"7","8","9","a/b","(",")","4","5","6","*","x^n","sqrt","1","2","3","-","x","=","0",".",",","+","ans",";"};
    for(int j=0;j<24;++j)button(3+(j%6)*53,100+(j/6)*27,49,25,keys[j],[j]{
        const char* values[]={"7","8","9","","(",")","4","5","6","*","","","1","2","3","-","x","=","0",".",",","+","ans",";"};
        if(j==3)insertTemplate("()/()",1);
        else if(j==10)insertTemplate("^()",2);
        else if(j==11)insertTemplate("sqrt()",5);
        else insert(values[j]);
    });
    bar({"OPTN","<",">","DEL","AC","DONE"});
}
const vector<string>& catalogItems(){static const vector<string> items={"sin(","cos(","tan(","asin(","acos(","atan(","sqrt(","cbrt(","ln(","log(","exp(","^2","pi","e","i","abs(","arg(","conj(","re(","im(","sinh(","cosh(","tanh(","log2(","ncr(","npr(","!","%","gcd(","lcm(","floor(","ceil(","round(","mod(","min(","max(","diff(","integral(","solve(","normalpdf(","normalcdf(","binompdf(","binomcdf(","poissonpdf(","poissoncdf(","atan2(","hypot(","sign(","a","b","c","d","=","ans","asinh(","acosh(","atanh("};return items;}
void drawCatalog(){
    header("FUNCTION CATALOG");auto& items=catalogItems();int first=catalogPage*18;
    for(int j=0;j<18&&first+j<int(items.size());++j){string value=items[first+j];button(3+(j%3)*106,30+(j/3)*29,102,26,value,[value]{screen=catalogReturn;insert(value);dirty=true;});}
    bar({"PREV","NEXT","","","","BACK"});
}
void soft(int n){
    switch(screen){
        case Screen::Run:if(n==0)catalog();if(n==1)insert("ans");if(n==2){fractionMode=!fractionMode;if(!history.empty()&&!history.back().error){Value v=history.back().value;history.back().output=fractionMode&&calc::real(v)?calc::fraction(v.real()):fmt(v);dirty=true;}}if(n==3)erase();if(n==4)clearInput();if(n==5)run();break;
        case Screen::Functions:if(n==0){compileGraphs();if(!activeGraph(selectedFn))selectGraph(1);open(Screen::Graph);}if(n==1){graphType=(graphType+1)%3;trace=false;dirty=true;markSave();}if(n==2)open(Screen::Table);if(n==3)showForm(Tool::Window);if(n==4)showTools(4);if(n==5)open(Screen::Menu);break;
        case Screen::Graph:if(n==0){trace=!trace;if(!activeGraph(selectedFn))selectGraph(1);traceX=graphType==0?(xmin+xmax)/2:(parameterMin+parameterMax)/2;dirty=true;}if(n==1)zoom(0.5);if(n==2)zoom(2);if(n==3)showForm(Tool::Window);if(n==4){if(graphType!=0)message("G-SLV uses Cartesian f(x)");else showTools(4);}if(n==5)open(Screen::Functions);break;
        case Screen::Table:if(n==0)tableRow-=8;if(n==1)tableRow+=8;if(n==2)selectedFn=(selectedFn+1)%6;if(n==3)showForm(Tool::TableSetup);if(n==4)open(Screen::Graph);if(n==5)open(Screen::Menu);dirty=true;break;
        case Screen::Tools:if(n==5)open(toolGroup==4?Screen::Graph:Screen::Menu);break;
        case Screen::UnitCategories:{int pages=(int(unitCategories().size())+11)/12;if(n==0)unitCategoryPage=std::max(0,unitCategoryPage-1);if(n==1)unitCategoryPage=std::min(pages-1,unitCategoryPage+1);if(n==5)open(Screen::Menu);dirty=true;break;}
        case Screen::UnitConvert:
            if(n==0)showEditor(unitValue,Screen::UnitConvert);
            if(n==1)openUnitPicker(true);
            if(n==2)openUnitPicker(false);
            if(n==3)std::swap(unitFrom,unitTo);
            if(n==4)open(Screen::Menu);
            if(n==5)convertUnits();dirty=true;break;
        case Screen::UnitSelect:{int pages=(int(currentUnitCategory().units.size())+11)/12;if(n==0)unitListPage=std::max(0,unitListPage-1);if(n==1)unitListPage=std::min(pages-1,unitListPage+1);if(n==5)open(Screen::UnitConvert);dirty=true;break;}
        case Screen::SystemInfo:if(n==4)open(Screen::Settings);if(n==5)open(Screen::Menu);break;
        case Screen::QRCode:if(n==4)open(qrReturn);if(n==5)open(Screen::Menu);break;
        case Screen::Form:
            if(matrixTool()){
                if(n==0||n==1)stepMatrixCell(n==0?-1:1);
                if(n==2)showEditor(matrixCell(matrixOperand,matrixRow,matrixCol),Screen::Form);
                if(n==3){clearInput();matrixReplace=false;}
            }else{
                if(n==0||n==1){fieldIndex=(fieldIndex+(n==0?-1:1)+fields.size())%fields.size();cursor=fields[fieldIndex].value.size();dirty=true;}
                if(n==2)showEditor(fields[fieldIndex].value,Screen::Form);
                if(n==3)catalog();
            }
            if(n==4)open(formReturn);if(n==5)execute();break;
        case Screen::Results:
            if(!resultMatrix.empty()){
                int count=int(resultMatrix.size()*resultMatrix[0].size());
                if(n==0||n==1)resultMatrixCell=(resultMatrixCell+(n==0?-1:1)+count)%count;
            }else{
                if(n==0)resultOffset=std::max(0,resultOffset-4);
                if(n==1)resultOffset=std::min(std::max(0,int(resultLines.size())-(resultEquation.empty()?9:5)),resultOffset+4);
            }
            if(n==3&&!resultEquation.empty())showQr(resultEquation);
            if(n==4)open(resultReturn);if(n==5)open(Screen::Menu);dirty=true;break;
        case Screen::Settings:if(n==4){save();message("Saved");}if(n==5)open(Screen::Menu);break;
        case Screen::Help:if(n==0)resultOffset=std::max(0,resultOffset-9);if(n==1)resultOffset=std::min(std::max(0,int(helpLines().size())-10),resultOffset+9);if(n==5)open(Screen::Menu);dirty=true;break;
        case Screen::Editor:if(n==0)catalog();if(n==1&&cursor)--cursor;if(n==2&&editTarget&&cursor<editTarget->size())++cursor;if(n==3)erase();if(n==4)clearInput();if(n==5)closeEditor();dirty=true;break;
        case Screen::Catalog:if(n==0)catalogPage=std::max(0,catalogPage-1);if(n==1)catalogPage=std::min(int(catalogItems().size()-1)/18,catalogPage+1);if(n==5)open(catalogReturn);dirty=true;break;
        default:break;
    }
}
void click(int x,int y){
    if(dirty)render(); // Hit boxes must belong to the current screen, even during fast input.
    // Header/footer take priority over graph gestures. Copy callback before it changes screens.
    for(auto it=buttons.rbegin();it!=buttons.rend();++it)if(x>=it->x&&x<it->x+it->w&&y>=it->y&&y<it->y+it->h){auto fn=it->action;fn();dirty=true;return;}
    if(screen==Screen::Editor&&x>=5&&x<315&&y>=27&&y<81){cursor=editorLayout.hitTest(x-editorMathX,y-editorMathY);dirty=true;return;}
    if(screen==Screen::Graph&&x>=GX&&x<GX+GW&&y>=GY&&y<GY+GH){trace=true;if(!activeGraph(selectedFn))selectGraph(1);double lo=graphType==0?xmin:parameterMin,hi=graphType==0?xmax:parameterMax;traceX=lo+(hi-lo)*(x-GX)/(GW-1);dirty=true;}
}
void back(){switch(screen){case Screen::Editor:closeEditor();break;case Screen::Catalog:open(catalogReturn);break;case Screen::Results:open(resultReturn);break;case Screen::Graph:open(Screen::Functions);break;case Screen::Form:soft(4);break;case Screen::UnitSelect:open(Screen::UnitConvert);break;case Screen::SystemInfo:open(Screen::Settings);break;case Screen::QRCode:open(qrReturn);break;case Screen::UnitConvert:case Screen::UnitCategories:open(Screen::Menu);break;default:open(Screen::Menu);break;}}
void keyboard(char key){
    if(key=='$'){open(Screen::Menu);return;}if(key==27){back();return;}
    bool calcScreen=screen==Screen::Run||(screen==Screen::Editor&&editorReturn==Screen::Run);
    if(calcScreen&&key=='#'){shifted=!shifted;dirty=true;return;}
    if(calcScreen&&(key=='@'||key=='~')){launchCalc(key=='~'||shifted);return;}
    if(key=='\t'){static int tab=0;pointerX=(tab++%6)*53+25;pointerY=226;pointerVisible=true;dirty=true;return;}
    if(key=='\r'||key=='\n'){
        if(pointerVisible){click(pointerX,pointerY);pointerVisible=false;}
        else if(screen==Screen::Run)run();else if(screen==Screen::Form)execute();else if(screen==Screen::Editor)closeEditor();else if(screen==Screen::Functions)soft(0);else if(screen==Screen::UnitConvert)convertUnits();return;
    }
    if(screen==Screen::Menu&&key>='1'&&key<='9'){int j=key-'1';click(55+(j%3)*105,45+(j/3)*42);return;}
    if(screen==Screen::Menu&&key=='0'){click(55,45+3*42);return;}
    // Typing in the grid replaces the selected entry; EDIT preserves the old
    // expression for cursor-based changes in the full math editor.
    if(screen==Screen::Form&&matrixTool()&&matrixReplace&&((key>=32&&key<127)||key==8||key==127)){
        clearInput();matrixReplace=false;
    }
    if(key==8||key==127){erase();return;}
    if(currentInput()&&key>=32&&key<127){pointerVisible=false;insert(string(1,key));}
}
void render(){
    buttons.clear();canvas.fillSprite(Bg);
    switch(screen){case Screen::Menu:drawMenu();break;case Screen::Run:drawRun();break;case Screen::Functions:drawFunctions();break;case Screen::Graph:drawGraph();break;case Screen::Table:drawTable();break;case Screen::Tools:drawTools();break;case Screen::Form:drawForm();break;case Screen::Results:drawResults();break;case Screen::Settings:drawSettings();break;case Screen::Help:drawHelp();break;case Screen::Editor:drawEditor();break;case Screen::Catalog:drawCatalog();break;case Screen::UnitCategories:drawUnitCategories();break;case Screen::UnitConvert:drawUnitConvert();break;case Screen::UnitSelect:drawUnitSelect();break;case Screen::SystemInfo:drawSystemInfo();break;case Screen::QRCode:drawQrCode();break;}
    if(!notice.empty()){canvas.fillRoundRect(4,183,312,26,4,Ink);text(clipped(notice,48),8,192,White,1);}
    if(pointerVisible){canvas.fillTriangle(pointerX,pointerY,pointerX+3,pointerY+12,pointerX+10,pointerY+8,Ink);canvas.drawTriangle(pointerX,pointerY,pointerX+3,pointerY+12,pointerX+10,pointerY+8,White);}
    canvas.pushSprite(0,0);dirty=false;
}
}
void setup(){
    Serial.begin(115200);hardware::begin();canvas.setColorDepth(16);
    if(!canvas.createSprite(320,240)){hardware::display.setTextColor(TFT_WHITE,TFT_BLACK);hardware::display.drawString("Framebuffer allocation failed",5,50,2);while(true)delay(1000);}
    canvas.setTextWrap(false);load();hardware::brightness(light);compileGraphs();render();
}
void loop(){
    static uint32_t polled=0,drawn=0,touchSeen=0;static bool wasTouch=false;uint32_t now=millis();
    static uint32_t batteryPolled=0;
    if(now-batteryPolled>=5000){batteryPolled=now;hardware::sampleBattery();dirty=true;}
    if(now-polled>=12){polled=now;char ch=hardware::key();if(ch)keyboard(ch);while(Serial.available())keyboard(char(Serial.read()));
        int x=0,y=0;bool pressed=false;if(hardware::touch(x,y,pressed)){touchSeen=now;if(pressed){pointerVisible=false;if(!wasTouch)click(x,y);else if(screen==Screen::Graph&&trace&&y>=GY&&y<GY+GH)click(x,y);}wasTouch=pressed;}
        // Recover from a lost I2C release without generating repeated held taps.
        if(wasTouch&&now-touchSeen>500)wasTouch=false;
        int dx,dy;bool clicked,held;hardware::ball(dx,dy,clicked,held);
        if(held)open(Screen::Menu);
        else if(screen==Screen::Graph&&trace){if(dx){double span=graphType==0?xmax-xmin:parameterMax-parameterMin;traceX+=dx*span/GW;dirty=true;}if(dy)selectGraph(dy>0?1:-1);if(clicked){trace=false;dirty=true;}}
        else {if(dx||dy){pointerX=std::max(0,std::min(319,pointerX+dx*board::PointerStep));pointerY=std::max(0,std::min(239,pointerY+dy*board::PointerStep));pointerVisible=true;dirty=true;}if(clicked){if(pointerVisible)click(pointerX,pointerY);else if(screen==Screen::Run)run();else if(screen==Screen::Form)execute();else if(screen==Screen::Editor)closeEditor();else{pointerVisible=true;dirty=true;}}}
    }
    if(!notice.empty()&&int32_t(now-noticeUntil)>=0){notice.clear();dirty=true;}
    if(savePending&&now-lastChange>1500)save();
    if(dirty&&now-drawn>=35){render();drawn=millis();}
    delay(2);
}
