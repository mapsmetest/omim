#pragma once

#include "base/base.hpp"

#include "geometry/point2d.hpp"

class Index;

namespace routing
{
/// Speed camera road record. Contains the identifier of the camera feature, and the road feature
/// identifier with geomety offset.
struct SpeedCamera
{
  uint32_t camFID;
  uint32_t roadFID;
  uint16_t pointOffset;
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

public:
  SpeedCameraIndexBuilder(Index & index);
};

}  // namespace routing
