#pragma once

#include "defines.hpp"

#include "geometry/point2d.hpp"

#include "coding/reader.hpp"
#include "coding/read_write_utils.hpp"

#include "base/base.hpp"

#include "std/vector.hpp"

class Writer;
class Index;
class FeatureType;

namespace routing
{

uint8_t ReadCamRestriction(FeatureType & ft);

/// Speed camera road record. Contains the identifier of the camera feature, and the road feature
/// identifier with geomety offset.
struct SpeedCamera
{
  uint32_t camFID;
  uint32_t roadFID;
  uint16_t pointOffset;

  SpeedCamera(uint32_t cFID, uint32_t rFID, uint16_t offset) : camFID(cFID), roadFID(rFID), pointOffset(offset) {}
  SpeedCamera() = default;
};

class SpeedCameraIndex
{
  vector<SpeedCamera> m_cameras;
public:
  SpeedCameraIndex() = default;

  template<class TSink>
  void Load(TSink model)
  {
    ReaderSource<TSink> src(model);
    uint32_t count = ReadVarUint<uint32_t>(src);
    if (!count)
      return;
    m_cameras.resize(count);
    src.Read(&m_cameras[0], sizeof(SpeedCamera)*count);
  }

  vector<SpeedCamera> const & GetCameras() const { return m_cameras; }

  void GetCamerasByFID(uint32_t fid, vector<SpeedCamera> & cameras) const;
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

  friend void UnitTest_SpeedCameraSerializationTest();

public:
  SpeedCameraIndexBuilder(Index & index);

  void AddVehicleFeature(FeatureType const & ft);
  void Serialize(Writer & writer);
};
}
