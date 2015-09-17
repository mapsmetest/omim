#pragma once

#include "base/base.hpp"

#include "geometry/point2d.hpp"

class Index;
class FeatureType;
class FileWriter;

namespace routing
{
/// Speed camera road record. Contains the identifier of the camera feature, and the road feature
/// identifier with geomety offset.
struct SpeedCamera
{
  uint32_t camFID;
  uint32_t roadFID;
  uint16_t pointOffset;

  SpeedCamera(uint32_t cFID, uint32_t rFID, uint16_t offset) : camFID(cFID), roadFID(rFID), pointOffset(offset) {}
};

class SpeedCameraIndexBuilder
{
  struct SpeedCameraFeature
  {
    uint32_t fID;
    m2::PointD point;
    SpeedCameraFeature(uint32_t fd, m2::PointD const & pnt) : fID(fd), point(pnt) {}
  };

  vector<SpeedCameraFeature> m_cameras;
  vector<SpeedCamera> m_result;
  set<uint32_t> m_checkedFIDs;

public:
  SpeedCameraIndexBuilder(Index & index);

  void AddVehicleFeature(FeatureType const & ft);
  void Serialize(FileWriter & writer);
};

}  // namespace routing
