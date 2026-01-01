#pragma once
#include <QString>

template<typename... args_t>
[[nodiscard]]
auto qformat(std::format_string<args_t...> fmt, args_t &&... args) -> QString
  {
  return QString::fromStdString(std::vformat(fmt.get(), std::make_format_args(args...)));
  }
