#ifndef STATE_H
#define STATE_H
enum class State:int{
  Invalid = 0,
  Shared = 1,
  Exclusive = 2,
  Modified = 3
};

#endif