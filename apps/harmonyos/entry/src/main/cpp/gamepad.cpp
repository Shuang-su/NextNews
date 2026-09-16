#include "gamepad.h"
#include <GameControllerKit/game_pad.h>
#include <GameControllerKit/game_device.h>
#include <mutex>
#include <cmath>
#include <algorithm>
namespace splat {
namespace {
std::mutex mutex;
std::array<double,4> axes{};
bool listening=false;
void Axis(const GamePad_AxisEvent* event,bool right){
 double x=0,y=0;
 const auto a=right?OH_GamePad_AxisEvent_GetZAxisValue(event,&x):OH_GamePad_AxisEvent_GetXAxisValue(event,&x);
 const auto b=right?OH_GamePad_AxisEvent_GetRZAxisValue(event,&y):OH_GamePad_AxisEvent_GetYAxisValue(event,&y);
 if(a!=GAME_CONTROLLER_SUCCESS||b!=GAME_CONTROLLER_SUCCESS||!std::isfinite(x)||!std::isfinite(y))return;
 std::lock_guard<std::mutex> lock(mutex);if(!listening)return;
 axes[right?2:0]=std::clamp(x,-1.0,1.0);axes[right?3:1]=std::clamp(y,-1.0,1.0);
}
void Left(const GamePad_AxisEvent* event){Axis(event,false);}
void Right(const GamePad_AxisEvent* event){Axis(event,true);}
void Device(const GameDevice_DeviceEvent*){std::lock_guard<std::mutex> lock(mutex);axes={};}
void Unregister(){OH_GamePad_LeftThumbstick_UnregisterAxisInputMonitor();OH_GamePad_RightThumbstick_UnregisterAxisInputMonitor();OH_GameDevice_UnregisterDeviceMonitor();}
}
bool EnableGamepad(bool enabled){
 {std::lock_guard<std::mutex> lock(mutex);if(enabled&&listening)return true;listening=false;axes={};}
 Unregister();if(!enabled)return true;
 const auto a=OH_GamePad_LeftThumbstick_RegisterAxisInputMonitor(Left);
 const auto b=OH_GamePad_RightThumbstick_RegisterAxisInputMonitor(Right);
 const auto c=OH_GameDevice_RegisterDeviceMonitor(Device);
 if(a!=GAME_CONTROLLER_SUCCESS||b!=GAME_CONTROLLER_SUCCESS||c!=GAME_CONTROLLER_SUCCESS){Unregister();return false;}
 {std::lock_guard<std::mutex> lock(mutex);listening=true;}return true;
}
std::array<double,4> ReadGamepad(){std::lock_guard<std::mutex> lock(mutex);return axes;}
}
