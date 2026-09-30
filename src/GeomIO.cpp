// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2026 GZYL

#include "GeomIO.h"
#include "Log.h"
#include "Protocol.h"

#include <BinTools.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <TopoDS_Shape.hxx>

namespace gfemesh {

int loadGeometry(const std::string& path, const std::string& format, TopoDS_Shape& shape)
{
    shape.Nullify();

    if (format == "bintools") {
        if (!BinTools::Read(shape, path.c_str()) || shape.IsNull()) {
            log().error("BinTools::Read failed: " + path);
            return kExitIoError;
        }
        return kExitSuccess;
    }

    if (format == "step") {
        STEPControl_Reader reader;
        const IFSelect_ReturnStatus st = reader.ReadFile(path.c_str());
        if (st != IFSelect_RetDone) {
            log().error("STEP ReadFile failed: " + path);
            return kExitIoError;
        }
        reader.TransferRoots();
        shape = reader.OneShape();
        if (shape.IsNull()) {
            log().error("STEP produced null shape: " + path);
            return kExitIoError;
        }
        return kExitSuccess;
    }

    log().error("unsupported GeomFormat: " + format);
    return kExitJobParseError;
}

} // namespace gfemesh
