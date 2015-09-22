#include "speed_camera.hpp"

#include "indexer/classificator.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/index.hpp"
#include "indexer/scales.hpp"

#include "coding/reader.hpp"
#include "coding/writer.hpp"
#include "coding/read_write_utils.hpp"

#include "base/string_utils.hpp"

namespace
{
auto RoadCameraComparator = [](routing::SpeedCamera const & lhs, routing::SpeedCamera const & rhs)
      {
        return lhs.roadFID < rhs.roadFID;
      };
}  // namespace

namespace routing
{
uint8_t ReadCamRestriction(FeatureType & ft)
{
  using feature::Metadata;
  ft.ParseMetadata();
  feature::Metadata const & mt = ft.GetMetadata();
  string const & speed = mt.Get(Metadata::FMD_MAXSPEED);
  if (!speed.length())
    return 0;
  int result;
  strings::to_int(speed, result);
  return result;
}

SpeedCameraIndexBuilder::SpeedCameraIndexBuilder(Index & index)
{
  Classificator & c = classif();
  uint32_t const req = c.GetTypeByPath({"highway", "speed_camera"});

  auto const f = [&req, &c, this](FeatureType & ft)
  {
    if (ft.GetFeatureType() != feature::GEOM_POINT)
      return;

    feature::TypesHolder hl = ft;

    for (uint32_t t : hl)
    {
      uint32_t const type = ftypes::BaseChecker::PrepareToMatch(t, 2);
      if (type == req)
      {
        ft.ParseGeometry(FeatureType::BEST_GEOMETRY);
        m_camerasTree.Add(SpeedCameraFeature(ft.GetID().m_index, ft.GetCenter()));
        break;
      }
    }
  };

  index.ForEachInScale(f, scales::GetUpperScale());
}

void SpeedCameraIndexBuilder::AddVehicleFeature(FeatureType const & ft)
{
  // Check if we already have to process this feature.
  auto uniq = m_checkedFIDs.insert(ft.GetID().m_index);
  if (!uniq.second)
    return;

  for (size_t i = 0; i < ft.GetPointsCount(); ++i)
  {
    m2::PointD const & pt = ft.GetPoint(i);
    // KDTree works only if we will have minimal delta.
    m_camerasTree.ForEachInRect(MercatorBounds::RectByCenterXYAndSizeInMeters(pt, 0.1), [&](SpeedCameraFeature const & cam)
      {
        // Speed cam point must be one point of the feature geometry. So we can use strong equality.
        if (cam.point != pt)
          return;
        m_result.emplace_back(cam.fID, ft.GetID().m_index, i);
      });
  }
}

void SpeedCameraIndexBuilder::Serialize(Writer & writer)
{
  sort(m_result.begin(), m_result.end(), RoadCameraComparator);
  uint32_t const size = static_cast<uint32_t>(m_result.size());
  WriteVarUint(writer, size);
  writer.Write(&m_result[0], size * sizeof(SpeedCamera));
  LOG(LINFO, ("Write ", size, "speed camera records."));
}

void SpeedCameraIndex::GetCamerasByFID(uint32_t fid, map<uint32_t, SpeedCamera> & cameras) const
{
  cameras.clear();
  SpeedCamera cam(0 /* camFID */, fid /* roadFID */, 0 /* offset */);
  auto it = lower_bound(m_cameras.begin(), m_cameras.end(), cam, RoadCameraComparator);
  while (it != m_cameras.end() && it->roadFID == fid)
    cameras.insert(make_pair(it->roadFID, *it++));
}
}  // namespace routing
