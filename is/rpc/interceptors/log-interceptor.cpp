#include "log-interceptor.hpp"

namespace is {

LogInterceptor::LogInterceptor(char level) {
  logger = spdlog::stdout_color_mt("log-interceptor");
  logger->set_pattern("[%L][%t][%d-%m-%Y %H:%M:%S:%e] %v");
  if (level == 'i')
    logger->set_level(spdlog::level::info);
  else if (level == 'w')
    logger->set_level(spdlog::level::warn);
  else if (level == 'e')
    logger->set_level(spdlog::level::err);
}

void LogInterceptor::before_call(Context* context) {
  started_at = current_time();
}

void LogInterceptor::after_call(Context* context) {
  auto took = pb::TimeUtil::DurationToMilliseconds(current_time() - started_at);
  auto code = common::StatusCode_Name(context->status().code());
  auto service = context->topic();

  if (context->status().code() == StatusCode::OK) {
    logger->info("{};{}ms;{}", service, took, code);
  } else {
    logger->warn("{};{}ms;{};'{}'", service, took, code, context->status().why());
  }
}

}  // namespace is