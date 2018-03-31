#pragma once

#include "context.hpp"

namespace is {

// An Interceptor presents a way of customizing the behaviour of the Service Provider class allowing
// functions to be called before or/and after the service implementation.
// An Interceptor needs to implement the InterceptorConcept

struct InterceptorConcept {
  virtual ~InterceptorConcept() = default;
  virtual InterceptorConcept* copy() const = 0;

  // Function that will be called right before the service implementation
  virtual void before_call(Context* context) = 0;
  // Function that will be called right after the service implementation
  virtual void after_call(Context* context) = 0;
};

class Interceptor {
  std::unique_ptr<InterceptorConcept> self;

 public:
  template <typename T>
  Interceptor(T x) : self(new T(std::move(x))) {}
  Interceptor(Interceptor const& x) : self(x.self->copy()) {}
  Interceptor(Interceptor&&) noexcept = default;

  friend void before_call(Interceptor const& x, Context* context) { x.self->before_call(context); }
  friend void after_call(Interceptor const& x, Context* context) { x.self->after_call(context); }
};

}  // namespace is
