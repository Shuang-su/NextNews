#pragma once
#include <array>
#include <map>
#include <string>
#include <cmath>
#include <algorithm>
namespace splat {
// Caller owns synchronization. Sum standard axes like PlayCanvas GamepadSource.
class GamepadState {
 std::map<std::string,std::array<double,4>> devices_;
public:
 void Axis(const std::string& id,bool right,double x,double y){
  if(id.empty()||!std::isfinite(x)||!std::isfinite(y))return;
  if(!devices_.count(id)&&devices_.size()>=32)return;
  auto &v=devices_[id];v[right?2:0]=std::clamp(x,-1.0,1.0);v[right?3:1]=std::clamp(y,-1.0,1.0);
 }
 void Remove(const std::string& id){devices_.erase(id);}
 void Clear(){devices_.clear();}
 std::array<double,4> Read()const{std::array<double,4> result{};for(const auto &device:devices_)for(int i=0;i<4;i++)result[i]+=device.second[i];return result;}
};
}
