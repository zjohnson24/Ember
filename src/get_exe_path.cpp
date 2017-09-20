#include <stdio.h>
#include <stdlib.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/predef.h>

#include "get_exe_path.h"


const char *argv0;

std::string get_exe_path_()
{
    if (argv0 == NULL || argv0[0] == 0) {
        return "";
    }
    boost::system::error_code ec;
    boost::filesystem::path p(
        boost::filesystem::canonical(
            argv0, boost::filesystem::current_path(), ec));
    return p.make_preferred().string();
}


#if (BOOST_OS_CYGWIN || BOOST_OS_WINDOWS) // {
#include "compat.h"

std::string get_exe_path()
{
    char buf[1024] = {0};
    DWORD ret = GetModuleFileNameA(NULL, buf, sizeof(buf));
    if (ret == 0 || ret == sizeof(buf))
    {
        return get_exe_path_();
    }
    return buf;
}

#elif (BOOST_OS_MACOS) // } {
#include <mach-o/dyld.h>

std::string get_exe_path()
{
    char buf[1024] = {0};
    uint32_t size = sizeof(buf);
    int ret = _NSGetExecutablePath(buf, &size);
    if (0 != ret) {
        return get_exe_path_();
    }
    boost::system::error_code ec;
    boost::filesystem::path p(
        boost::filesystem::canonical(buf, boost::filesystem::current_path(), ec));
    return p.make_preferred().string();
}

#elif (BOOST_OS_SOLARIS) // } {
#include <stdlib.h>

std::string get_exe_path()
{
    std::string ret = getexecname();
    if (ret.empty()) {
        return get_exe_path_();
    }
    boost::filesystem::path p(ret);
    if (!p.has_root_directory())
    {
        boost::system::error_code ec;
        p = boost::filesystem::canonical(
            p, boost::filesystem::current_path(), ec);
        ret = p.make_preferred().string();
    }
    return ret;
}

#elif (BOOST_OS_BSD) // } {
#include <sys/sysctl.h>

std::string get_exe_path()
{
    int mib[4] = {0};
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = -1;
    char buf[1024] = {0};
    size_t size = sizeof(buf);
    sysctl(mib, 4, buf, &size, NULL, 0);
    if (size == 0 || size == sizeof(buf)) {
        return get_exe_path_();
    }
    std::string path(buf, size);
    boost::system::error_code ec;
    boost::filesystem::path p(
        boost::filesystem::canonical(
            path, boost::filesystem::current_path(), ec));
    return p.make_preferred().string();
}

#elif (BOOST_OS_LINUX) // } {
#include <unistd.h>

std::string get_exe_path()
{
    char buf[1024] = {0};
    ssize_t size = readlink("/proc/self/exe", buf, sizeof(buf));
    if (size == 0 || size == sizeof(buf)) {
        return get_exe_path_();
    }
    std::string path(buf, size);
    boost::system::error_code ec;
    boost::filesystem::path p(
        boost::filesystem::canonical(
            path, boost::filesystem::current_path(), ec));
    return p.make_preferred().string();
}

#else // } {

std::string get_exe_path()
{
    return get_exe_path_();
}

#endif // }
