// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2026 GZYL

#include "JobFile.h"
#include "Log.h"
#include "Protocol.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <future>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace gfemesh {
namespace {

using Sv = std::string_view;

Sv trimView(Sv s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

Sv stripCommentView(Sv line)
{
    const auto pos = line.find('#');
    if (pos != Sv::npos)
        line.remove_suffix(line.size() - pos);
    return line;
}

bool equalsIgnoreCaseAscii(Sv a, Sv b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto ca = static_cast<unsigned char>(a[i]);
        const auto cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb))
            return false;
    }
    return true;
}

bool parseBool(Sv v, bool& out)
{
    if (equalsIgnoreCaseAscii(v, "true") || v == "1") {
        out = true;
        return true;
    }
    if (equalsIgnoreCaseAscii(v, "false") || v == "0") {
        out = false;
        return true;
    }
    return false;
}

bool parseInt(Sv v, int& out)
{
    v = trimView(v);
    if (v.empty())
        return false;
    int value = 0;
    const auto* first = v.data();
    const auto* last = v.data() + v.size();
    const auto r = std::from_chars(first, last, value);
    if (r.ec != std::errc{} || r.ptr != last)
        return false;
    out = value;
    return true;
}

bool parseDouble(Sv v, double& out)
{
    v = trimView(v);
    if (v.empty())
        return false;
    // MSVC <charconv> float support varies; use temporary for portability.
    try {
        std::size_t idx = 0;
        const std::string tmp(v);
        out = std::stod(tmp, &idx);
        return idx == tmp.size();
    } catch (...) {
        return false;
    }
}

bool parseDimTag(Sv v, DimTag& out)
{
    const auto comma = v.find(',');
    if (comma == Sv::npos)
        return false;
    return parseInt(trimView(v.substr(0, comma)), out.dim)
        && parseInt(trimView(v.substr(comma + 1)), out.tag);
}

template <typename T>
bool parseCsv(Sv v, std::vector<T>& out)
{
    out.clear();
    while (!v.empty()) {
        const auto comma = v.find(',');
        const Sv item = trimView(comma == Sv::npos ? v : v.substr(0, comma));
        if (!item.empty()) {
            if constexpr (std::is_same_v<T, int>) {
                int n = 0;
                if (!parseInt(item, n))
                    return false;
                out.push_back(n);
            } else {
                double d = 0;
                if (!parseDouble(item, d))
                    return false;
                out.push_back(d);
            }
        }
        if (comma == Sv::npos)
            break;
        v.remove_prefix(comma + 1);
    }
    return !out.empty();
}

std::string toLowerAscii(Sv s)
{
    std::string out(s);
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string parentPath(const std::string& path)
{
    const auto p = path.find_last_of("/\\");
    if (p == std::string::npos)
        return ".";
    if (p == 0)
        return path.substr(0, 1);
    return path.substr(0, p);
}

std::string joinPath(const std::string& dir, const std::string& rel)
{
    if (rel.size() >= 2 && std::isalpha(static_cast<unsigned char>(rel[0])) && rel[1] == ':')
        return rel;
    if (!rel.empty() && (rel[0] == '/' || rel[0] == '\\'))
        return rel;
    if (dir.empty() || dir == ".")
        return rel;
    const char sep = (dir.find('\\') != std::string::npos) ? '\\' : '/';
    if (dir.back() == '/' || dir.back() == '\\')
        return dir + rel;
    return dir + sep + rel;
}

struct CleanLine {
    int lineNo = 0;
    std::string text;
};

enum class BlockKind {
    Header,
    CurveSize,
    Sweep,
    Revolve,
    TransfiniteSurface,
    TransfiniteVolume,
    Recombine,
    IgnoredSection
};

struct BlockIndex {
    BlockKind kind = BlockKind::Header;
    std::size_t begin = 0; // inclusive into cleanLines
    std::size_t end = 0;   // exclusive
    int sectionLineNo = 0; // [Section] line number; 0 for header
    std::string sectionName; // for diagnostics
};

struct KeyVal {
    int lineNo = 0;
    Sv key;
    Sv val;
};

bool splitKeyVal(const CleanLine& line, KeyVal& out)
{
    const Sv text(line.text);
    const auto eq = text.find('=');
    if (eq == Sv::npos)
        return false;
    out.lineNo = line.lineNo;
    out.key = trimView(text.substr(0, eq));
    out.val = trimView(text.substr(eq + 1));
    return !out.key.empty();
}

struct BlockParseStatus {
    int exitCode = kExitSuccess;
    std::string error;
    std::vector<std::string> warnings;
};

void fail(BlockParseStatus& st, std::string msg)
{
    st.exitCode = kExitJobParseError;
    st.error = std::move(msg);
}

BlockParseStatus parseHeaderBlock(const std::vector<CleanLine>& lines,
                                  std::size_t begin,
                                  std::size_t end,
                                  JobFile& out)
{
    BlockParseStatus st;
    for (std::size_t i = begin; i < end; ++i) {
        KeyVal kv;
        if (!splitKeyVal(lines[i], kv)) {
            fail(st, "expected key=value at line " + std::to_string(lines[i].lineNo));
            return st;
        }
        const Sv key = kv.key;
        const Sv val = kv.val;

        if (key == "JobFormatVersion") {
            if (!parseInt(val, out.formatVersion)) {
                fail(st, "invalid JobFormatVersion");
                return st;
            }
        } else if (key == "GeomPath") {
            out.geomPath.assign(val.begin(), val.end());
        } else if (key == "GeomFormat") {
            out.geomFormat = toLowerAscii(val);
        } else if (key == "GenerateDim") {
            if (!parseInt(val, out.generateDim)) {
                fail(st, "invalid GenerateDim");
                return st;
            }
        } else if (key == "GFE.DefaultSize") {
            if (!parseDouble(val, out.defaultSize)) {
                fail(st, "invalid GFE.DefaultSize");
                return st;
            }
        } else if (key == "GFE.Optimize") {
            if (!parseBool(val, out.optimize)) {
                fail(st, "invalid GFE.Optimize");
                return st;
            }
        } else if (key == "GFE.Recombine2D") {
            if (!parseBool(val, out.recombine2d)) {
                fail(st, "invalid GFE.Recombine2D");
                return st;
            }
        } else if (key == "GFE.StrictSize") {
            // ignored
        } else if (key.starts_with("Auto")) {
            st.warnings.push_back("ignoring unsupported key: " + std::string(key));
        } else if (key.starts_with("Mesh.") || key.starts_with("General.")) {
            const std::string keyStr(key);
            double num = 0;
            std::size_t idx = 0;
            try {
                const std::string tmp(val);
                num = std::stod(tmp, &idx);
                if (idx == tmp.size())
                    out.numberOptions[keyStr] = num;
                else
                    out.stringOptions[keyStr] = tmp;
            } catch (...) {
                out.stringOptions[keyStr].assign(val.begin(), val.end());
            }
        } else {
            st.warnings.push_back("ignoring unknown header key: " + std::string(key));
        }
    }
    return st;
}

BlockParseStatus parseCurveSizeBlock(const std::vector<CleanLine>& lines,
                                     std::size_t begin,
                                     std::size_t end,
                                     CurveSizeBlock& out)
{
    BlockParseStatus st;
    out = {};
    for (std::size_t i = begin; i < end; ++i) {
        KeyVal kv;
        if (!splitKeyVal(lines[i], kv)) {
            fail(st, "expected key=value at line " + std::to_string(lines[i].lineNo));
            return st;
        }
        if (kv.key == "tags") {
            if (!parseCsv(kv.val, out.tags)) {
                fail(st, "invalid [CurveSize] tags at line " + std::to_string(kv.lineNo));
                return st;
            }
        } else if (kv.key == "cnt") {
            if (!parseInt(kv.val, out.cnt)) {
                fail(st, "invalid [CurveSize] cnt");
                return st;
            }
        } else if (kv.key == "density") {
            if (!parseDouble(kv.val, out.density)) {
                fail(st, "invalid [CurveSize] density");
                return st;
            }
        } else if (kv.key == "name") {
            out.name.assign(kv.val.begin(), kv.val.end());
        } else {
            fail(st, "unknown [CurveSize] key: " + std::string(kv.key));
            return st;
        }
    }
    if (out.tags.empty()) {
        fail(st, "[CurveSize] missing tags=");
        return st;
    }
    if (out.density < 0 && out.cnt < 2) {
        fail(st, "[CurveSize] need cnt>=2 or density>=0");
        return st;
    }
    return st;
}

BlockParseStatus parseSweepBlock(const std::vector<CleanLine>& lines,
                                 std::size_t begin,
                                 std::size_t end,
                                 SweepBlock& out)
{
    BlockParseStatus st;
    out = {};
    for (std::size_t i = begin; i < end; ++i) {
        KeyVal kv;
        if (!splitKeyVal(lines[i], kv)) {
            fail(st, "expected key=value at line " + std::to_string(lines[i].lineNo));
            return st;
        }
        if (kv.key == "source") {
            if (!parseDimTag(kv.val, out.source)) {
                fail(st, "invalid [Sweep] source");
                return st;
            }
        } else if (kv.key == "target") {
            if (!parseDimTag(kv.val, out.target)) {
                fail(st, "invalid [Sweep] target");
                return st;
            }
        } else if (kv.key == "body") {
            if (!parseDimTag(kv.val, out.body)) {
                fail(st, "invalid [Sweep] body");
                return st;
            }
        } else if (kv.key == "dx") {
            if (!parseDouble(kv.val, out.dx)) {
                fail(st, "invalid [Sweep] dx");
                return st;
            }
        } else if (kv.key == "dy") {
            if (!parseDouble(kv.val, out.dy)) {
                fail(st, "invalid [Sweep] dy");
                return st;
            }
        } else if (kv.key == "dz") {
            if (!parseDouble(kv.val, out.dz)) {
                fail(st, "invalid [Sweep] dz");
                return st;
            }
        } else if (kv.key == "layers") {
            if (!parseCsv(kv.val, out.layers)) {
                fail(st, "invalid [Sweep] layers");
                return st;
            }
        } else if (kv.key == "ratio") {
            if (!parseCsv(kv.val, out.ratio)) {
                fail(st, "invalid [Sweep] ratio");
                return st;
            }
        } else if (kv.key == "recombine") {
            if (!parseBool(kv.val, out.recombine)) {
                fail(st, "invalid [Sweep] recombine");
                return st;
            }
        } else {
            fail(st, "unknown [Sweep] key: " + std::string(kv.key));
            return st;
        }
    }
    if (out.source.dim < 0 || out.target.dim < 0 || out.body.dim < 0
        || out.layers.empty() || out.ratio.empty()) {
        fail(st, "[Sweep] incomplete block");
        return st;
    }
    return st;
}

BlockParseStatus parseRevolveBlock(const std::vector<CleanLine>& lines,
                                   std::size_t begin,
                                   std::size_t end,
                                   RevolveBlock& out)
{
    BlockParseStatus st;
    out = {};
    for (std::size_t i = begin; i < end; ++i) {
        KeyVal kv;
        if (!splitKeyVal(lines[i], kv)) {
            fail(st, "expected key=value at line " + std::to_string(lines[i].lineNo));
            return st;
        }
        if (kv.key == "source") {
            if (!parseDimTag(kv.val, out.source)) {
                fail(st, "invalid [Revolve] source");
                return st;
            }
        } else if (kv.key == "target") {
            if (!parseDimTag(kv.val, out.target)) {
                fail(st, "invalid [Revolve] target");
                return st;
            }
        } else if (kv.key == "body") {
            if (!parseDimTag(kv.val, out.body)) {
                fail(st, "invalid [Revolve] body");
                return st;
            }
        } else if (kv.key == "x") {
            if (!parseDouble(kv.val, out.x)) {
                fail(st, "invalid [Revolve] x");
                return st;
            }
        } else if (kv.key == "y") {
            if (!parseDouble(kv.val, out.y)) {
                fail(st, "invalid [Revolve] y");
                return st;
            }
        } else if (kv.key == "z") {
            if (!parseDouble(kv.val, out.z)) {
                fail(st, "invalid [Revolve] z");
                return st;
            }
        } else if (kv.key == "ax") {
            if (!parseDouble(kv.val, out.ax)) {
                fail(st, "invalid [Revolve] ax");
                return st;
            }
        } else if (kv.key == "ay") {
            if (!parseDouble(kv.val, out.ay)) {
                fail(st, "invalid [Revolve] ay");
                return st;
            }
        } else if (kv.key == "az") {
            if (!parseDouble(kv.val, out.az)) {
                fail(st, "invalid [Revolve] az");
                return st;
            }
        } else if (kv.key == "angle") {
            if (!parseDouble(kv.val, out.angle)) {
                fail(st, "invalid [Revolve] angle");
                return st;
            }
        } else if (kv.key == "layers") {
            if (!parseCsv(kv.val, out.layers)) {
                fail(st, "invalid [Revolve] layers");
                return st;
            }
        } else if (kv.key == "ratio") {
            if (!parseCsv(kv.val, out.ratio)) {
                fail(st, "invalid [Revolve] ratio");
                return st;
            }
        } else if (kv.key == "recombine") {
            if (!parseBool(kv.val, out.recombine)) {
                fail(st, "invalid [Revolve] recombine");
                return st;
            }
        } else if (kv.key == "copyTarget") {
            if (!parseBool(kv.val, out.copyTarget)) {
                fail(st, "invalid [Revolve] copyTarget");
                return st;
            }
        } else {
            fail(st, "unknown [Revolve] key: " + std::string(kv.key));
            return st;
        }
    }
    if (out.source.dim < 0 || out.target.dim < 0 || out.body.dim < 0
        || out.layers.empty() || out.ratio.empty()) {
        fail(st, "[Revolve] incomplete block");
        return st;
    }
    return st;
}

BlockParseStatus parseTransfiniteSurfaceBlock(const std::vector<CleanLine>& lines,
                                              std::size_t begin,
                                              std::size_t end,
                                              TransfiniteSurfaceBlock& out)
{
    BlockParseStatus st;
    out = {};
    for (std::size_t i = begin; i < end; ++i) {
        KeyVal kv;
        if (!splitKeyVal(lines[i], kv)) {
            fail(st, "expected key=value at line " + std::to_string(lines[i].lineNo));
            return st;
        }
        if (kv.key == "tag") {
            if (!parseInt(kv.val, out.tag)) {
                fail(st, "invalid [TransfiniteSurface] tag");
                return st;
            }
        } else if (kv.key == "arrangement") {
            out.arrangement.assign(kv.val.begin(), kv.val.end());
        } else if (kv.key == "corners") {
            out.corners.clear();
            if (!trimView(kv.val).empty() && !parseCsv(kv.val, out.corners)) {
                fail(st, "invalid [TransfiniteSurface] corners at line "
                     + std::to_string(kv.lineNo));
                return st;
            }
        } else if (kv.key == "curveTags") {
            if (!parseCsv(kv.val, out.curveTags)) {
                fail(st, "invalid [TransfiniteSurface] curveTags");
                return st;
            }
        } else if (kv.key == "curveCnt") {
            if (!parseCsv(kv.val, out.curveCnt)) {
                fail(st, "invalid [TransfiniteSurface] curveCnt");
                return st;
            }
        } else {
            fail(st, "unknown [TransfiniteSurface] key: " + std::string(kv.key));
            return st;
        }
    }
    if (out.tag < 0) {
        fail(st, "[TransfiniteSurface] missing tag=");
        return st;
    }
    if (out.curveTags.size() != out.curveCnt.size()) {
        fail(st, "[TransfiniteSurface] curveTags/curveCnt size mismatch");
        return st;
    }
    return st;
}

BlockParseStatus parseTransfiniteVolumeBlock(const std::vector<CleanLine>& lines,
                                             std::size_t begin,
                                             std::size_t end,
                                             TransfiniteVolumeBlock& out)
{
    BlockParseStatus st;
    out = {};
    for (std::size_t i = begin; i < end; ++i) {
        KeyVal kv;
        if (!splitKeyVal(lines[i], kv)) {
            fail(st, "expected key=value at line " + std::to_string(lines[i].lineNo));
            return st;
        }
        if (kv.key == "tag") {
            if (!parseInt(kv.val, out.tag)) {
                fail(st, "invalid [TransfiniteVolume] tag");
                return st;
            }
        } else if (kv.key == "corners") {
            out.corners.clear();
            if (!trimView(kv.val).empty() && !parseCsv(kv.val, out.corners)) {
                fail(st, "invalid [TransfiniteVolume] corners at line "
                     + std::to_string(kv.lineNo));
                return st;
            }
        } else if (kv.key == "curveTags") {
            if (!parseCsv(kv.val, out.curveTags)) {
                fail(st, "invalid [TransfiniteVolume] curveTags");
                return st;
            }
        } else if (kv.key == "curveCnt") {
            if (!parseCsv(kv.val, out.curveCnt)) {
                fail(st, "invalid [TransfiniteVolume] curveCnt");
                return st;
            }
        } else {
            fail(st, "unknown [TransfiniteVolume] key: " + std::string(kv.key));
            return st;
        }
    }
    if (out.tag < 0) {
        fail(st, "[TransfiniteVolume] missing tag=");
        return st;
    }
    if (out.curveTags.size() != out.curveCnt.size()) {
        fail(st, "[TransfiniteVolume] curveTags/curveCnt size mismatch");
        return st;
    }
    return st;
}

BlockParseStatus parseRecombineBlock(const std::vector<CleanLine>& lines,
                                     std::size_t begin,
                                     std::size_t end,
                                     RecombineBlock& out)
{
    BlockParseStatus st;
    out = {};
    out.dim = 2;
    for (std::size_t i = begin; i < end; ++i) {
        KeyVal kv;
        if (!splitKeyVal(lines[i], kv)) {
            fail(st, "expected key=value at line " + std::to_string(lines[i].lineNo));
            return st;
        }
        if (kv.key == "dim") {
            if (!parseInt(kv.val, out.dim)) {
                fail(st, "invalid [Recombine] dim");
                return st;
            }
        } else if (kv.key == "tags") {
            if (!parseCsv(kv.val, out.tags)) {
                fail(st, "invalid [Recombine] tags at line " + std::to_string(kv.lineNo));
                return st;
            }
        } else {
            fail(st, "unknown [Recombine] key: " + std::string(kv.key));
            return st;
        }
    }
    if (out.tags.empty()) {
        fail(st, "[Recombine] missing tags=");
        return st;
    }
    return st;
}

bool loadRawLines(const std::string& jobPath, std::vector<std::string>& rawLines)
{
    std::ifstream in(jobPath);
    if (!in)
        return false;
    rawLines.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        rawLines.push_back(std::move(line));
    }
    return true;
}

bool isSectionHeader(Sv text)
{
    return text.size() >= 2 && text.front() == '[' && text.back() == ']';
}

bool endsWithComma(Sv text)
{
    return !text.empty() && text.back() == ',';
}

//! Fold CSV values wrapped after ',': join following lines that are not
//! key=value / [Section] onto the previous logical line.
bool buildCleanLines(const std::vector<std::string>& rawLines, std::vector<CleanLine>& clean)
{
    clean.clear();
    clean.reserve(rawLines.size());
    for (std::size_t i = 0; i < rawLines.size(); ++i) {
        const Sv trimmed = trimView(stripCommentView(Sv(rawLines[i])));
        if (trimmed.empty())
            continue;

        if (!clean.empty() && endsWithComma(Sv(clean.back().text))
            && !isSectionHeader(trimmed) && trimmed.find('=') == Sv::npos) {
            clean.back().text.append(trimmed.begin(), trimmed.end());
            continue;
        }

        CleanLine cl;
        cl.lineNo = static_cast<int>(i + 1);
        cl.text.assign(trimmed.begin(), trimmed.end());
        clean.push_back(std::move(cl));
    }
    return true;
}

int buildBlockIndex(const std::vector<CleanLine>& clean, std::vector<BlockIndex>& blocks)
{
    blocks.clear();

    BlockIndex header;
    header.kind = BlockKind::Header;
    header.begin = 0;
    header.end = 0;

    auto flushOpenEnd = [&](std::size_t endIdx) {
        if (blocks.empty()) {
            header.end = endIdx;
            blocks.push_back(header);
        } else {
            blocks.back().end = endIdx;
        }
    };

    for (std::size_t i = 0; i < clean.size(); ++i) {
        const Sv text(clean[i].text);
        if (text.size() >= 2 && text.front() == '[' && text.back() == ']') {
            flushOpenEnd(i);
            const Sv name = trimView(text.substr(1, text.size() - 2));
            BlockIndex b;
            b.begin = i + 1;
            b.end = i + 1;
            b.sectionLineNo = clean[i].lineNo;
            b.sectionName.assign(name.begin(), name.end());
            if (name == "CurveSize") {
                b.kind = BlockKind::CurveSize;
            } else if (name == "Sweep") {
                b.kind = BlockKind::Sweep;
            } else if (name == "Revolve") {
                b.kind = BlockKind::Revolve;
            } else if (name == "TransfiniteSurface") {
                b.kind = BlockKind::TransfiniteSurface;
            } else if (name == "TransfiniteVolume") {
                b.kind = BlockKind::TransfiniteVolume;
            } else if (name == "Recombine") {
                b.kind = BlockKind::Recombine;
            } else if (name == "Geom2Type" || name == "ElemType") {
                b.kind = BlockKind::IgnoredSection;
            } else {
                log().error("unknown section [" + b.sectionName + "] at line "
                            + std::to_string(clean[i].lineNo));
                return kExitJobParseError;
            }
            blocks.push_back(std::move(b));
        }
    }

    if (blocks.empty()) {
        header.end = clean.size();
        blocks.push_back(header);
    } else {
        blocks.back().end = clean.size();
    }
    return kExitSuccess;
}

} // namespace

int parseJobFile(const std::string& jobPath, JobFile& out)
{
    std::vector<std::string> rawLines;
    if (!loadRawLines(jobPath, rawLines)) {
        log().error("cannot open job file: " + jobPath);
        return kExitIoError;
    }

    std::vector<CleanLine> clean;
    buildCleanLines(rawLines, clean);

    std::vector<BlockIndex> blocks;
    if (const int ec = buildBlockIndex(clean, blocks); ec != kExitSuccess)
        return ec;

    out = JobFile{};
    out.jobDir = parentPath(jobPath);

    // Slot results by block index (order preserved when merging).
    struct Slot {
        BlockKind kind = BlockKind::Header;
        BlockParseStatus status;
        CurveSizeBlock curve;
        SweepBlock sweep;
        RevolveBlock revolve;
        TransfiniteSurfaceBlock tfSurface;
        TransfiniteVolumeBlock tfVolume;
        RecombineBlock recombine;
        std::string ignoredSectionName;
    };
    std::vector<Slot> slots(blocks.size());
    JobFile headerOut;

    std::vector<std::future<void>> tasks;
    tasks.reserve(blocks.size());

    for (std::size_t bi = 0; bi < blocks.size(); ++bi) {
        tasks.push_back(std::async(std::launch::async, [&, bi]() {
            const BlockIndex& b = blocks[bi];
            Slot& slot = slots[bi];
            slot.kind = b.kind;
            switch (b.kind) {
            case BlockKind::Header:
                // Header writes into local JobFile; merged on main thread.
                slot.status = parseHeaderBlock(clean, b.begin, b.end, headerOut);
                break;
            case BlockKind::CurveSize:
                slot.status = parseCurveSizeBlock(clean, b.begin, b.end, slot.curve);
                break;
            case BlockKind::Sweep:
                slot.status = parseSweepBlock(clean, b.begin, b.end, slot.sweep);
                break;
            case BlockKind::Revolve:
                slot.status = parseRevolveBlock(clean, b.begin, b.end, slot.revolve);
                break;
            case BlockKind::TransfiniteSurface:
                slot.status = parseTransfiniteSurfaceBlock(clean, b.begin, b.end, slot.tfSurface);
                break;
            case BlockKind::TransfiniteVolume:
                slot.status = parseTransfiniteVolumeBlock(clean, b.begin, b.end, slot.tfVolume);
                break;
            case BlockKind::Recombine:
                slot.status = parseRecombineBlock(clean, b.begin, b.end, slot.recombine);
                break;
            case BlockKind::IgnoredSection:
                slot.ignoredSectionName = b.sectionName;
                slot.status.warnings.push_back("ignoring unsupported section [" + b.sectionName + "]");
                break;
            }
        }));
    }

    for (auto& t : tasks)
        t.get();

    // Merge in file order; first error wins.
    int exitCode = kExitSuccess;
    for (const auto& slot : slots) {
        for (const auto& w : slot.status.warnings)
            log().warn(w);
        if (slot.status.exitCode != kExitSuccess && exitCode == kExitSuccess) {
            log().error(slot.status.error);
            exitCode = slot.status.exitCode;
        }
    }
    if (exitCode != kExitSuccess)
        return exitCode;

    // Exactly one header block expected (possibly empty).
    out.formatVersion = headerOut.formatVersion;
    out.geomPath = std::move(headerOut.geomPath);
    out.geomFormat = std::move(headerOut.geomFormat);
    out.generateDim = headerOut.generateDim;
    out.defaultSize = headerOut.defaultSize;
    out.optimize = headerOut.optimize;
    out.recombine2d = headerOut.recombine2d;
    out.numberOptions = std::move(headerOut.numberOptions);
    out.stringOptions = std::move(headerOut.stringOptions);

    for (auto& slot : slots) {
        switch (slot.kind) {
        case BlockKind::CurveSize:
            out.curveSizes.push_back(std::move(slot.curve));
            break;
        case BlockKind::Sweep:
            out.sweeps.push_back(std::move(slot.sweep));
            break;
        case BlockKind::Revolve:
            out.revolves.push_back(std::move(slot.revolve));
            break;
        case BlockKind::TransfiniteSurface:
            out.tfSurfaces.push_back(std::move(slot.tfSurface));
            break;
        case BlockKind::TransfiniteVolume:
            out.tfVolumes.push_back(std::move(slot.tfVolume));
            break;
        case BlockKind::Recombine:
            out.recombines.push_back(std::move(slot.recombine));
            break;
        default:
            break;
        }
    }

    if (out.formatVersion != kJobFormatVersion) {
        log().error("JobFormatVersion must be " + std::to_string(kJobFormatVersion));
        return kExitJobParseError;
    }
    if (out.geomPath.empty()) {
        log().error("GeomPath is required");
        return kExitJobParseError;
    }

    out.resolvedGeomPath = joinPath(out.jobDir, out.geomPath);
    if (out.geomFormat.empty()) {
        const auto lower = toLowerAscii(out.resolvedGeomPath);
        if ((lower.size() >= 5 && lower.compare(lower.size() - 5, 5, ".step") == 0)
            || (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".stp") == 0))
            out.geomFormat = "step";
        else
            out.geomFormat = "bintools";
    }

    return kExitSuccess;
}

} // namespace gfemesh
