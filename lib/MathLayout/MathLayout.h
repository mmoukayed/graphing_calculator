#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

// Platform-independent textbook layout. All positions are pixels, and every
// insertion boundary is mapped back to the original editable ASCII expression.
namespace mathlayout {
enum class Kind { Text, Line };
struct Stroke {
    Kind kind=Kind::Text;
    int x=0,y=0,x2=0,y2=0,font=2;
    std::string text;
};
struct Caret {int x=0,y=0,height=16;};
struct Layout {
    int width=0,height=0,baseline=0;
    bool formatted=false;
    std::vector<Stroke> strokes;
    std::vector<Caret> carets;
    size_t hitTest(int x,int y) const;
};
using Measure=std::function<int(const std::string&,int)>;
Layout typeset(const std::string& source,const Measure& measure,int font=2);
}
