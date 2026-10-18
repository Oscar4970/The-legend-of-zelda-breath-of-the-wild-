// Read-only bounded parser of public scalar metadata in *.cnmt.xml.
// XML entities, external files, and unsafe values are NEVER evaluated or printed.
#pragma once
#include <cctype>
#include <cstdint>
#include <string>
namespace switchcnmt {
struct Data {
 bool valid=false,hasFirmware=false;
 std::string id,type;
 uint32_t version=0,firmware=0;
};
inline std::string value(const std::string& xml,const std::string& key,size_t boundary) {
 const std::string a="<"+key+">",b="</"+key+">";
 size_t start=xml.find(a);
 if(start==std::string::npos || start>=boundary)return {};
 start+=a.size();
 const size_t end=xml.find(b,start);
 if(end==std::string::npos||end>boundary||end-start>64)return {};
 std::string s=xml.substr(start,end-start);
 const auto l=s.find_first_not_of(" \t\r\n"),r=s.find_last_not_of(" \t\r\n");
 return l==std::string::npos?std::string{}:s.substr(l,r-l+1);
}
inline bool uint32(const std::string& x,uint32_t& result) {
 if(x.empty()||x.size()>10)return false;
 uint64_t v=0;
 for(char c:x){if(c<'0'||c>'9')return false;v=v*10+(c-'0');if(v>UINT32_MAX)return false;}
 result=static_cast<uint32_t>(v);
 return true;
}
inline Data parse(const std::string& xml) {
 Data d;
 if(xml.size()>65536 || xml.find("<!DOCTYPE")!=std::string::npos ||
    xml.find("<!ENTITY")!=std::string::npos ||
    xml.find("<ContentMeta")==std::string::npos)return d;
 auto boundary=xml.find("<Content>");
 if(boundary==std::string::npos)boundary=xml.size();
 d.id=value(xml,"Id",boundary);
 if(d.id.size()==18&&d.id[0]=='0'&&(d.id[1]=='x'||d.id[1]=='X'))d.id.erase(0,2);
 if(d.id.size()!=16)return d;
 for(char& c:d.id) {
   if(!std::isxdigit(static_cast<unsigned char>(c)))return d;
   c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
 }
 d.type=value(xml,"Type",boundary);
 if(d.type!="Application"&&d.type!="Patch"&&d.type!="AddOnContent"&&
    d.type!="DataPatch"&&d.type!="SystemUpdate")return d;
 if(!uint32(value(xml,"Version",boundary),d.version))return d;
 d.hasFirmware=uint32(value(xml,"RequiredSystemVersion",xml.size()),d.firmware);
 d.valid=true;
 return d;
}
}
