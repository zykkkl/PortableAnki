#pragma once
#include "IDisplay.h"

// IDisplay 的"串口模拟"实现:屏幕到货前,用串口当显示器来验证状态机。
// : public IDisplay 表示"我继承 IDisplay,并实现它要求的那几个方法"。
class SerialDisplay : public IDisplay {
public:
  void showFront(const Card& c, int remaining) override;
  void showBack(const Card& c) override;
  void showMessage(const char* msg) override;
};
