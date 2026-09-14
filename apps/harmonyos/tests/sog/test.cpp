#include "splat.h"
#include <iostream>
#include <cmath>
#include <chrono>
#include <cassert>
int main(int argc,char**argv){if(argc<2)return 2;
 try {auto start=std::chrono::steady_clock::now();auto a=splat::ReadSog(argv[1]);
 std::cout<<"SOG count="<<a.points.size()<<" decodeMs="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<"\n";
 auto encoded=splat::ReadSog(argv[1],nullptr,true);
 if(encoded.Count()!=a.Count()||!encoded.points.empty()||encoded.codes.size()!=a.Count())throw std::runtime_error("encoded shape mismatch");
 for(int transform=0;transform<2;++transform){
   if(transform){splat::ApplyViewerTransform(a);splat::ApplyViewerTransform(encoded);}
   for(size_t i=0;i<a.Count();++i){const auto g=encoded.At(i),expected=a.At(i);
     for(int k=0;k<3;++k)if(g.position[k]!=expected.position[k])throw std::runtime_error("encoded position mismatch");
     for(int k=0;k<4;++k)if(g.color[k]!=expected.color[k])throw std::runtime_error("encoded color mismatch");
     for(int k=0;k<6;++k)if(g.covariance[k]!=expected.covariance[k])throw std::runtime_error("encoded covariance mismatch");
   }
 }
 splat::ApplyViewerTransform(a); // Restore the source frame for the independent PLY comparison.
 std::cout<<"PASS encoded byte codes, SH0, covariance and Rz180 for every Gaussian\n";
 if(argc>2){auto b=splat::ReadPly(argv[2]);if(a.points.size()!=b.points.size())throw std::runtime_error("count mismatch");
 double position=0,color=0,cov=0;
 for(size_t i=0;i<a.points.size();++i){for(int k=0;k<3;++k)position=std::max(position,double(std::abs(a.points[i].position[k]-b.points[i].position[k])));for(int k=0;k<4;++k)color=std::max(color,double(std::abs(a.points[i].color[k]-b.points[i].color[k])));for(int k=0;k<6;++k)cov=std::max(cov,double(std::abs(a.points[i].covariance[k]-b.points[i].covariance[k]))/std::max(1.,double(std::abs(b.points[i].covariance[k]))));}
 std::cout<<"maxError position="<<position<<" color="<<color<<" relativeCov="<<cov<<"\n";
 if(position>.001||color>.0001||cov>.001)throw std::runtime_error("decoder mismatch");
 }
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
}
