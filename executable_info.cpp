#include "executable_info.hpp"

#include <stdexcept>

#if defined(__linux__)
#include <unistd.h>
#include <linux/limits.h>
#else
#error "OS not supported yet"
#endif

std::filesystem::path get_executable_path()
{
  char buffer[PATH_MAX];
  const auto nbytes = readlink("/proc/self/exe", buffer, PATH_MAX);
  if (nbytes == -1)
    throw std::runtime_error("failed to get executable path");

  // readlink does _not_ null terminate strings, that's why we
  // implicitly include the number of bytes returned by readlink
  return std::filesystem::path{buffer, buffer+nbytes};
}
