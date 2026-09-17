#include "splat.h"
#include <iostream>
#include <cmath>
#include <chrono>
#include <cassert>
int main(int argc,char**argv){if(argc<2)return 2;
 try {auto start=std::chrono::steady_clock::now();auto a=splat::ReadSog(argv[1]);
 std::cout<<"SOG count="<<a.points.size()<<" decodeMs="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<"\n";
 auto bounds=splat::InspectModel(argv[1]);
 for(int k=0;k<3;k++)if(std::abs(bounds[k]-(k<2?-a.center[k]:a.center[k]))>1e-6)throw std::runtime_error("Inspection frame mismatch");
 if(bounds[3]!=a.radius)throw std::runtime_error("Inspection radius mismatch");
 std::cout<<"PASS imported model bounds in Viewer world coordinates\n";
 auto encoded=splat::ReadSog(argv[1],nullptr,true);
 if(encoded.Count()!=a.Count()||!encoded.points.empty()||encoded.codes.size()!=a.Count())throw std::runtime_error("encoded shape mismatch");
 if(a.shDegree){
 if(a.shDegree!=encoded.shDegree||a.shLabels!=encoded.shLabels||a.sogHarmonics->centroids!=encoded.sogHarmonics->centroids)throw std::runtime_error("SH encoded/float mismatch");
 if(argc>2&&std::string(argv[2])=="--sh-fixture"){
   if(a.Count()!=2||a.shLabels[0][0]!=0||a.shLabels[1][0]!=65)throw std::runtime_error("SH fixture labels");
   splat::Scene merged;
   splat::AppendRange(merged,a,1,1);splat::AppendRange(merged,a,0,1);
   if(merged.harmonics[0]!=splat::HarmonicsAt(a,1)||merged.harmonics[1]!=splat::HarmonicsAt(a,0))throw std::runtime_error("SH reordered merge mismatch");
   splat::Scene dc;dc.points.push_back(a.points[0]);
   splat::AppendRange(merged,dc,0,1);if(merged.harmonics[2][3]!=0)throw std::runtime_error("SH0 mixed merge mismatch");
   splat::Scene dcFirst;splat::AppendRange(dcFirst,dc,0,1);splat::AppendRange(dcFirst,a,1,1);
   if(dcFirst.harmonics.size()!=2||dcFirst.harmonics[0][3]!=0||dcFirst.harmonics[1]!=splat::HarmonicsAt(a,1))throw std::runtime_error("DC-first merge mismatch");
   const auto &h=*a.sogHarmonics;int n=a.shDegree==1?3:a.shDegree==2?8:15;
   for(int i=0;i<2;i++)for(int k=0;k<n;k++)for(int c=0;c<3;c++){
     const int label=a.shLabels[i][0],x=(label%64)*n+k,y=label/64;
     const auto code=h.centroids[(y*h.width+x)*4+c];
     if(h.books[code][1]!=float(i*50+k*3+c)/10)throw std::runtime_error("SH independent coefficient mismatch");
   }
   std::cout<<"PASS compressed SH fixture degree="<<a.shDegree<<"\n";return 0;
 }
}
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
 if(argc>3&&std::string(argv[2])=="--sog") {
   const auto b=splat::ReadSog(argv[3]);if(a.Count()!=b.Count())throw std::runtime_error("ZIP/unbundled count mismatch");
   for(size_t i=0;i<a.Count();++i){const auto &x=a.points[i],&y=b.points[i];
     for(int k=0;k<3;++k)if(x.position[k]!=y.position[k])throw std::runtime_error("ZIP/unbundled position mismatch");
     for(int k=0;k<4;++k)if(x.color[k]!=y.color[k])throw std::runtime_error("ZIP/unbundled color mismatch");
     for(int k=0;k<6;++k)if(x.covariance[k]!=y.covariance[k])throw std::runtime_error("ZIP/unbundled covariance mismatch");
   }
   std::cout<<"PASS ZIP/unbundled exact position, color, covariance equality\n";return 0;
 }
 if(argc>2){auto b=splat::ReadPly(argv[2]);if(a.points.size()!=b.points.size())throw std::runtime_error("count mismatch");
 double position=0,color=0,cov=0;
 for(size_t i=0;i<a.points.size();++i){for(int k=0;k<3;++k)position=std::max(position,double(std::abs(a.points[i].position[k]-b.points[i].position[k])));for(int k=0;k<4;++k)color=std::max(color,double(std::abs(a.points[i].color[k]-b.points[i].color[k])));for(int k=0;k<6;++k)cov=std::max(cov,double(std::abs(a.points[i].covariance[k]-b.points[i].covariance[k]))/std::max(1.,double(std::abs(b.points[i].covariance[k]))));}
 std::cout<<"maxError position="<<position<<" color="<<color<<" relativeCov="<<cov<<"\n";
 if(position>.001||color>.0001||cov>.001)throw std::runtime_error("decoder mismatch");
 }
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
}
