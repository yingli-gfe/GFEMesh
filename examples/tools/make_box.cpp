// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2026 GZYL
// Helper: write a 10x10x10 box as STEP or BinTools for mesh_gen smoke tests.

#include <BRepPrimAPI_MakeBox.hxx>
#include <BinTools.hxx>
#include <STEPControl_Writer.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <TopoDS_Shape.hxx>

#include <cctype>
#include <cstring>
#include <iostream>
#include <string>

namespace {

bool endsWithIgnoreCase(const std::string& s, const char* suffix)
{
    const std::size_t n = std::strlen(suffix);
    if (s.size() < n)
        return false;
    for (std::size_t i = 0; i < n; ++i) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[s.size() - n + i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
        if (a != b)
            return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "box.step";
    TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();
    const std::string p(path);

    if (endsWithIgnoreCase(p, ".bin") || endsWithIgnoreCase(p, ".bintools")) {
        if (!BinTools::Write(box, path)) {
            std::cerr << "BinTools::Write failed: " << path << "\n";
            return 1;
        }
        std::cout << "wrote bintools " << path << "\n";
        return 0;
    }

    STEPControl_Writer writer;
    writer.Transfer(box, STEPControl_AsIs);
    if (writer.Write(path) != IFSelect_RetDone) {
        std::cerr << "failed to write STEP " << path << "\n";
        return 1;
    }
    std::cout << "wrote STEP " << path << "\n";
    return 0;
}
