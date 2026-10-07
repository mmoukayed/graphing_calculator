#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
class Preferences {
    std::map<std::string,std::string> strings;
    std::map<std::string,double> numbers;
    std::map<std::string,std::vector<unsigned char>> bytes;
public:
    bool begin(const char*,bool){return true;} void end(){}
    void putInt(const char* k,int v){numbers[k]=v;}void putUInt(const char* k,unsigned v){numbers[k]=v;}
    void putDouble(const char* k,double v){numbers[k]=v;}void putBool(const char* k,bool v){numbers[k]=v;}
    int getInt(const char* k,int d){return numbers.count(k)?numbers[k]:d;}
    unsigned getUInt(const char* k,unsigned d){return numbers.count(k)?numbers[k]:d;}
    bool getBool(const char* k,bool d){return numbers.count(k)?numbers[k]:d;}
    double getDouble(const char* k,double d){return numbers.count(k)?numbers[k]:d;}
    void putString(const char* k,const char* v){strings[k]=v;}
    std::string getString(const char* k,const char* d){return strings.count(k)?strings[k]:d;}
    void putBytes(const char* k,const void* v,size_t n){auto p=static_cast<const unsigned char*>(v);bytes[k]={p,p+n};}
    size_t getBytesLength(const char* k){return bytes.count(k)?bytes[k].size():0;}
    void getBytes(const char* k,void* v,size_t n){if(bytes[k].size()==n)std::memcpy(v,bytes[k].data(),n);}
};
