#pragma once

#include "context.hpp"

namespace is {

// An Interceptor presents a way of customizing the behaviour of the Service Provider class allowing
// functions to be called before or/and after the service implementation.
// An Interceptor needs to be move copyable
struct Interceptor {
  virtual ~Interceptor() {}

  // Function that will be called right before the service implementation
  virtual void before_call(Context* context) = 0;

  // Function that will be called right after the service implementation
  virtual void after_call(Context* context) = 0;
};

}
