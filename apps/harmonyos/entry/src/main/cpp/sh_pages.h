#pragma once
#include "page_atlas.h"
#include <array>
namespace splat {
constexpr uint32_t ShPageTexels=16384; // 64 KiB RGBA8UI; four 4096-wide rows.
constexpr uint32_t ShSourcePages=64;
constexpr uint32_t ShAtlasPages=1024; // 64 MiB, shared across resident source codebooks.
inline uint32_t SogReference(uint32_t book,uint32_t label,uint32_t degree){
 if(book>=2048||label>=65536||degree>3)throw std::runtime_error("Invalid SOG SH reference");
 return book|(label<<11)|(degree<<27);
}
inline uint32_t ShLinear(uint32_t label,uint32_t degree,uint32_t coefficient){
 const uint32_t n=degree==1?3:degree==2?8:degree==3?15:0;
 if(!n||coefficient>=n||label>=65536)throw std::runtime_error("Invalid SH coefficient address");
 return label*n+coefficient;
}
}
