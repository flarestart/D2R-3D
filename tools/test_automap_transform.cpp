#include "../standalone/automap_transform.h"
#include <cassert>
#include <cmath>
#include <cstdio>
bool Near(float a, float b) { return std::fabs(a-b)<0.001f; }
int main() {
    float base[16]{}; base[0]=1; base[9]=-0.5f;
    automap::Transform t;
    assert(automap::FromViews(base,base,t));
    auto p=t.Apply({23,71},{100,100}); assert(Near(p.x,23)&&Near(p.y,71));
    float turned[16]{}; turned[8]=-1; turned[1]=-0.5f;
    assert(automap::FromViews(base,turned,t));
    p=t.Apply({110,100},{100,100}); assert(Near(p.x,100)&&Near(p.y,105));
    p=t.Apply({100,105},{100,100}); assert(Near(p.x,90)&&Near(p.y,100));
    p=t.Apply({100,100},{100,100}); assert(Near(p.x,100)&&Near(p.y,100));
    automap::Vertex v[4]={{290,400,1,0,0},{300,400,1,1,0},{290,405,1,0,1},{300,405,1,1,1}};
    automap::TransformQuad(v,t,{100,100},800,600);
    assert(Near(v[0].x,300)&&Near(v[0].y,405));
    for(int i=0;i<4;++i) { assert(v[i].z==1); assert(v[i].u==float(i%2)); assert(v[i].v==float(i/2)); }
    float singular[16]{}; assert(!automap::FromViews(singular,base,t));
    puts("automap transform tests passed");
}
