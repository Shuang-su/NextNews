#include "sky_image.h"
#include <iostream>
#include <cmath>
int main(int argc,char**argv){
 try{auto image=splat::ReadSkyImage(argv[1]);
  if(argc>2)return 2;
  if(image->width!=4||image->height!=2)return 3;
  if(std::abs(image->pixels.get()[0]-1)>0.001||image->pixels.get()[1]>0.001)return 4;
  std::cout<<"decoded 4x2 linear RGBA\n";
 }catch(const std::exception&e){if(argc>2){std::cout<<"rejected: "<<e.what()<<"\n";return 0;}std::cerr<<e.what();return 1;}
}
