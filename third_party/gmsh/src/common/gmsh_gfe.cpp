#include "gmsh.h"
#include "GModelIO_OCC.h"
#include <BRep_Tool.hxx>
#include <TopoDS.hxx>
#include "OCCAttributes.h"
#include "ExtrudeParams.h"
#include <TopExp_Explorer.hxx>

#include <array>
#include <cmath>
#include <set>
#include <vector>

//! 前置声明
bool _checkInit();
void _createOcc();
ExtrudeParams *_getExtrudeParams(const std::vector<int> &numElements,
                                 const std::vector<double> &heights,
                                 const bool recombine);

int gmsh::model::mesh::gfe::sweep(const std::pair<int, int> &source,
                                  const std::pair<int, int> volume,
                                  const std::pair<int, int> target, double dx,
                                  double dy, double dz,
                                  const std::vector<int> &numElements,
                                  const std::vector<double> &heights,
                                  bool recombine)
{
  if(!_checkInit()) return -1;
  _createOcc();
  auto ee = _getExtrudeParams(numElements, heights, recombine);
  if(!ee) return -2;

  //! 1. source和target须属于同一个volume
  //! 2. source和target不能有共同的边
  //! 3. lateral中不能包含非four-sides的面

  // 找Shape
  auto occ = GModel::current()->getOCCInternals();
  auto srcSh = occ->find(source.first, source.second);
  auto volSh = occ->find(volume.first, volume.second);
  auto tgtSh = occ->find(target.first, target.second);

  // 找侧面
  std::vector<TopoDS_Shape> lateral;
  TopExp_Explorer exp;
  bool foundSrc = false, foundTgt = false;
  for(exp.Init(volSh, TopAbs_FACE); exp.More(); exp.Next()) {
    if(exp.Current().IsSame(srcSh))
      foundSrc = true;
    else if(exp.Current().IsSame(tgtSh))
      foundTgt = true;
    else
      lateral.push_back(exp.Current());
  }

  if(!foundSrc || !foundTgt) return 1;

  // 自定义shape比较器
  auto ShCmp = [](const TopoDS_Shape &a, const TopoDS_Shape &b) {
    return a.HashCode(0x7fffffff) < b.HashCode(0x7fffffff);
  };

  // 找source和target的边
  std::set<TopoDS_Shape, decltype(ShCmp)> srcEdge(ShCmp), tgtEdge(ShCmp);
  for(exp.Init(srcSh, TopAbs_EDGE); exp.More(); exp.Next())
    srcEdge.insert(exp.Current());
  for(exp.Init(tgtSh, TopAbs_EDGE); exp.More(); exp.Next())
    tgtEdge.insert(exp.Current());

  for(const auto &sh1 : srcEdge)
    if(tgtEdge.find(sh1) != tgtEdge.end()) return 2;

  // 找source的点
  std::set<TopoDS_Shape, decltype(ShCmp)> srcVert(ShCmp);
  for(exp.Init(srcSh, TopAbs_VERTEX); exp.More(); exp.Next())
    srcVert.insert(exp.Current());

  // 找source和target的边的对应关系, 构造attr2D
  std::vector<std::array<TopoDS_Shape, 3>> attr2D;
  std::set<TopoDS_Shape, decltype(ShCmp)> lateralEdge(ShCmp);
  for(const auto &face : lateral) {
    int nEdge = 0;
    std::array<TopoDS_Shape, 2> tmp;
    for(exp.Init(face, TopAbs_EDGE); exp.More(); exp.Next(), nEdge++) {
      if(nEdge == 4) { return 3; }
      if(srcEdge.find(exp.Current()) != srcEdge.end())
        tmp[0] = exp.Current();
      else if(tgtEdge.find(exp.Current()) != tgtEdge.end())
        tmp[1] = exp.Current();
      else
        lateralEdge.insert(exp.Current());
    }
    attr2D.push_back({face, tmp[0], tmp[1]});
  }

  // Attr3D
  {
    auto e1 = new ExtrudeParams(COPIED_ENTITY);
    e1->fill(TRANSLATE, dx, dy, dz, 0, 0, 0, 0, 0, 0, 0);
    e1->mesh = ee->mesh;
    occ->addAttribute(new OCCAttributes(2, tgtSh, e1, 2, srcSh));
    auto e2 = new ExtrudeParams(EXTRUDED_ENTITY);
    e2->fill(TRANSLATE, dx, dy, dz, 0, 0, 0, 0, 0, 0, 0);
    e2->mesh = ee->mesh;
    occ->addAttribute(new OCCAttributes(3, volSh, e2, 2, srcSh));
  }
  // Attr2D
  {
    for(const auto &sh3 : attr2D) {
      auto e1 = new ExtrudeParams(COPIED_ENTITY);
      e1->fill(TRANSLATE, dx, dy, dz, 0, 0, 0, 0, 0, 0, 0);
      e1->mesh = ee->mesh;
      auto e2 = new ExtrudeParams(EXTRUDED_ENTITY);
      e2->fill(TRANSLATE, dx, dy, dz, 0, 0, 0, 0, 0, 0, 0);
      e2->mesh = ee->mesh;
      occ->addAttribute(new OCCAttributes(1, sh3[2], e1, 1, sh3[1]));
      occ->addAttribute(new OCCAttributes(2, sh3[0], e2, 1, sh3[1]));
    }
  }
  // Attr1D
  {
    for(const auto &edge : lateralEdge) {
      TopoDS_Shape bot, top;
      int nVert = 0;
      for(exp.Init(edge, TopAbs_VERTEX); exp.More(); exp.Next(), nVert++) {
        if(nVert == 2) return 4;
        if(srcVert.find(exp.Current()) != srcVert.end())
          bot = exp.Current();
        else
          top = exp.Current();
      }
      auto e1 = new ExtrudeParams(EXTRUDED_ENTITY);
      e1->fill(TRANSLATE, dx, dy, dz, 0, 0, 0, 0, 0, 0, 0);
      e1->mesh = ee->mesh;
      occ->addAttribute(new OCCAttributes(1, edge, e1, 0, bot));
      double lc = occ->getAttribute()->getMeshSize(0, bot);
      if(lc <= 0 || lc > MAX_LC) return 5;
      occ->addAttribute(new OCCAttributes(0, top, lc));
    }
  }

  delete ee;
  return 0;
}

int gmsh::model::mesh::gfe::revolve(
  const std::pair<int, int> &source, const std::pair<int, int> volume,
  const std::pair<int, int> target, double x, double y, double z, double ax,
  double ay, double az, double angle, const std::vector<int> &numElements,
  const std::vector<double> &heights, bool recombine, bool copyTarget)
{
  if(!_checkInit()) return -1;
  _createOcc();

  // �� OCC �ٷ�������������һ�£���ת���Ҽ�����������
  const double twoPi = std::acos(-1.0) * 2.0;
  if(std::abs(angle) >= twoPi - 1e-9) return -3;

  auto ee = _getExtrudeParams(numElements, heights, recombine);
  if(!ee) return -2;

  //! 1. source��target������ͬһ��volume
  //! 2. ���� source/target ���ߣ�����/��״ʵ�����������泣�������߱ߣ�
  //! 3. lateral �в��ܰ�������>4 ����

  auto occ = GModel::current()->getOCCInternals();
  auto srcSh = occ->find(source.first, source.second);
  auto volSh = occ->find(volume.first, volume.second);
  auto tgtSh = occ->find(target.first, target.second);

  std::vector<TopoDS_Shape> lateral;
  TopExp_Explorer exp;
  bool foundSrc = false, foundTgt = false;
  for(exp.Init(volSh, TopAbs_FACE); exp.More(); exp.Next()) {
    if(exp.Current().IsSame(srcSh))
      foundSrc = true;
    else if(exp.Current().IsSame(tgtSh))
      foundTgt = true;
    else
      lateral.push_back(exp.Current());
  }

  if(!foundSrc || !foundTgt) {
    delete ee;
    return 1;
  }

  auto ShCmp = [](const TopoDS_Shape &a, const TopoDS_Shape &b) {
    return a.HashCode(0x7fffffff) < b.HashCode(0x7fffffff);
  };

  std::set<TopoDS_Shape, decltype(ShCmp)> srcEdge(ShCmp), tgtEdge(ShCmp);
  for(exp.Init(srcSh, TopAbs_EDGE); exp.More(); exp.Next())
    srcEdge.insert(exp.Current());
  for(exp.Init(tgtSh, TopAbs_EDGE); exp.More(); exp.Next())
    tgtEdge.insert(exp.Current());

  // ���ߣ�����Ϊ����ߡ������������߶�Ӧ��Ҳ����Ϊ lateralEdge ȥ�� 1D ����
  std::set<TopoDS_Shape, decltype(ShCmp)> sharedEdge(ShCmp);
  for(const auto &e : srcEdge)
    if(tgtEdge.find(e) != tgtEdge.end()) sharedEdge.insert(e);

  std::set<TopoDS_Shape, decltype(ShCmp)> srcVert(ShCmp);
  for(exp.Init(srcSh, TopAbs_VERTEX); exp.More(); exp.Next())
    srcVert.insert(exp.Current());

  std::vector<std::array<TopoDS_Shape, 3>> attr2D;
  std::set<TopoDS_Shape, decltype(ShCmp)> lateralEdge(ShCmp);
  for(const auto &face : lateral) {
    int nEdge = 0;
    std::array<TopoDS_Shape, 2> tmp;
    for(exp.Init(face, TopAbs_EDGE); exp.More(); exp.Next(), nEdge++) {
      if(nEdge == 4) {
        delete ee;
        return 3;
      }
      const TopoDS_Shape &ed = exp.Current();
      if(sharedEdge.find(ed) != sharedEdge.end())
        continue; // ���Ϲ��ߣ�����
      if(srcEdge.find(ed) != srcEdge.end())
        tmp[0] = ed;
      else if(tgtEdge.find(ed) != tgtEdge.end())
        tmp[1] = ed;
      else
        lateralEdge.insert(ed);
    }
    attr2D.push_back({face, tmp[0], tmp[1]});
  }

  auto fillRotate = [&](ExtrudeParams *e) {
    e->fill(ROTATE, 0, 0, 0, ax, ay, az, x, y, z, angle);
  };

  // Attr3D
  {
    if(copyTarget) {
      auto e1 = new ExtrudeParams(COPIED_ENTITY);
      fillRotate(e1);
      e1->mesh = ee->mesh;
      occ->addAttribute(new OCCAttributes(2, tgtSh, e1, 2, srcSh));
    }
    auto e2 = new ExtrudeParams(EXTRUDED_ENTITY);
    fillRotate(e2);
    e2->mesh = ee->mesh;
    occ->addAttribute(new OCCAttributes(3, volSh, e2, 2, srcSh));
  }
  // Attr2D
  {
    for(const auto &sh3 : attr2D) {
      // no valid source/target edge
      if(sh3[1].IsNull() || sh3[2].IsNull()) continue;
      if(copyTarget) {
        auto e1 = new ExtrudeParams(COPIED_ENTITY);
        fillRotate(e1);
        e1->mesh = ee->mesh;
        occ->addAttribute(new OCCAttributes(1, sh3[2], e1, 1, sh3[1]));
      }
      auto e2 = new ExtrudeParams(EXTRUDED_ENTITY);
      fillRotate(e2);
      e2->mesh = ee->mesh;
      occ->addAttribute(new OCCAttributes(2, sh3[0], e2, 1, sh3[1]));
    }
  }
  // Attr1D
  {
    for(const auto &edge : lateralEdge) {
      TopoDS_Shape bot, top;
      int nVert = 0;
      for(exp.Init(edge, TopAbs_VERTEX); exp.More(); exp.Next(), nVert++) {
        if(nVert == 2) {
          delete ee;
          return 4;
        }
        if(srcVert.find(exp.Current()) != srcVert.end())
          bot = exp.Current();
        else
          top = exp.Current();
      }
      if(bot.IsNull() || top.IsNull()) continue;
      auto e1 = new ExtrudeParams(EXTRUDED_ENTITY);
      fillRotate(e1);
      e1->mesh = ee->mesh;
      occ->addAttribute(new OCCAttributes(1, edge, e1, 0, bot));
      double lc = occ->getAttribute()->getMeshSize(0, bot);
      if(lc <= 0 || lc > MAX_LC) {
        delete ee;
        return 5;
      }
      occ->addAttribute(new OCCAttributes(0, top, lc));
    }
  }

  delete ee;
  return 0;
}
