#ifndef GFEMESH_LOG_H
#define GFEMESH_LOG_H

// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2026 GZYL

#include <fstream>
#include <iostream>
#include <string>

namespace gfemesh {

class Logger {
public:
    void setLogPath(const std::string& path)
    {
        path_ = path;
        if (path_.empty())
            return;
        file_.open(path_, std::ios::out | std::ios::trunc);
    }

    void info(const std::string& msg)
    {
        write(std::cout, "INFO", msg);
    }

    void warn(const std::string& msg)
    {
        write(std::cerr, "WARN", msg);
    }

    void error(const std::string& msg)
    {
        write(std::cerr, "ERROR", msg);
    }

private:
    void write(std::ostream& console, const char* level, const std::string& msg)
    {
        console << level << ": " << msg << '\n';
        if (file_)
            file_ << level << ": " << msg << '\n';
    }

    std::string path_;
    std::ofstream file_;
};

inline Logger& log()
{
    static Logger instance;
    return instance;
}

} // namespace gfemesh

#endif // GFEMESH_LOG_H
