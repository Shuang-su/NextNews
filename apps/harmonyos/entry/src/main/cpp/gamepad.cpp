#include "gamepad.h"
#include "gamepad_state.h"
#include <cstdlib>
#include <GameControllerKit/game_pad.h>
#include <GameControllerKit/game_device.h>
#include <mutex>
#include <cmath>
#include <algorithm>
namespace splat {
namespace {
std::mutex mutex;
GamepadState state;
bool listening=false;
void Axis(const GamePad_AxisEvent* event,bool right){
 double x=0,y=0;
 const auto a=right?OH_GamePad_AxisEvent_GetZAxisValue(event,&x):OH_GamePad_AxisEvent_GetXAxisValue(event,&x);
 const auto b=right?OH_GamePad_AxisEvent_GetRZAxisValue(event,&y):OH_GamePad_AxisEvent_GetYAxisValue(event,&y);
 if(a!=GAME_CONTROLLER_SUCCESS||b!=GAME_CONTROLLER_SUCCESS||!std::isfinite(x)||!std::isfinite(y))return;
 char *id=nullptr;const auto result=OH_GamePad_AxisEvent_GetDeviceId(event,&id);
 std::string key=result==GAME_CONTROLLER_SUCCESS&&id?id:"";std::free(id);
 std::lock_guard<std::mutex> lock(mutex);if(listening)state.Axis(key,right,x,y);
}
void Left(const GamePad_AxisEvent* event){Axis(event,false);}
void Right(const GamePad_AxisEvent* event){Axis(event,true);}
void Device(const GameDevice_DeviceEvent* event){
 GameDevice_DeviceInfo *info=nullptr;char *id=nullptr;std::string key;
 if(OH_GameDevice_DeviceEvent_GetDeviceInfo(event,&info)==GAME_CONTROLLER_SUCCESS&&info){
  if(OH_GameDevice_DeviceInfo_GetDeviceId(info,&id)==GAME_CONTROLLER_SUCCESS&&id)key=id;
 }
 std::free(id);if(info)OH_GameDevice_DestroyDeviceInfo(&info);
 std::lock_guard<std::mutex> lock(mutex);if(listening)state.Remove(key);
}
void Unregister(){OH_GamePad_LeftThumbstick_UnregisterAxisInputMonitor();OH_GamePad_RightThumbstick_UnregisterAxisInputMonitor();OH_GameDevice_UnregisterDeviceMonitor();}
}
bool EnableGamepad(bool enabled){
 {std::lock_guard<std::mutex> lock(mutex);if(enabled&&listening)return true;listening=false;state.Clear();}
 Unregister();if(!enabled)return true;
 const auto c=OH_GameDevice_RegisterDeviceMonitor(Device);
 if(c!=GAME_CONTROLLER_SUCCESS){Unregister();return false;}
 const auto a=OH_GamePad_LeftThumbstick_RegisterAxisInputMonitor(Left);
 const auto b=OH_GamePad_RightThumbstick_RegisterAxisInputMonitor(Right);
 if(a!=GAME_CONTROLLER_SUCCESS||b!=GAME_CONTROLLER_SUCCESS||c!=GAME_CONTROLLER_SUCCESS){Unregister();return false;}
 {std::lock_guard<std::mutex> lock(mutex);listening=true;}return true;
}
std::array<double,4> ReadGamepad(){std::lock_guard<std::mutex> lock(mutex);return state.Read();}
}
