#pragma once
#include "Types.h"

// 显示"接口"(抽象类):状态机只依赖这份说明书,不关心背后是墨水屏还是串口。
// 名字前缀 I 表示 Interface(接口)。= 0 表示"纯虚函数",意思是"这里不实现,
// 谁继承我谁负责实现"。屏幕到货后只要写一个 EinkDisplay 继承它,状态机一行不改。
class IDisplay {
public:
  virtual ~IDisplay() {}
  virtual void showFront(const Card& c, int remaining) = 0;
  virtual void showBack(const Card& c) = 0;
  virtual void showMessage(const char* msg) = 0;
};
