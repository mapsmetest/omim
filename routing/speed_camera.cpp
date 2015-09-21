#include "speed_camera.hpp"

#include "indexer/classificator.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/index.hpp"
#include "indexer/scales.hpp"

#include "coding/reader.hpp"
#include "coding/writer.hpp"
#include "coding/read_write_utils.hpp"

#include "base/string_utils.hpp"

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

    feature::TypesHolder hl = ft;  // feature::TypesHolder(ft.GetFeatureType());

    for (uint32_t t : hl)
    {
      uint32_t const type = ftypes::BaseChecker::PrepareToMatch(t, 2);
      if (type == req)
      {
        ft.ParseGeometry(FeatureType::BEST_GEOMETRY);
        m_cameras.emplace_back(ft.GetID().m_index, ft.GetCenter());
        break;
      }
    }
  };

  index.ForEachInScale(f, scales::GetUpperScale());
  LOG(LINFO, ("Found ", m_cameras.size(), " cameras."));
}

void SpeedCameraIndexBuilder::AddVehicleFeature(FeatureType const & ft)
{
  // Check if we already have to process this feature.
  auto uniq = m_checkedFIDs.insert(ft.GetID().m_index);
  if (!uniq.second)
    return;

  for (size_t i = 0; i < ft.GetPointsCount(); ++i)
  {
    //TODO(ldragunov) Rewrite this with rtree index usage. ASAP.
    for (auto const & cam : m_cameras)
    {
      if (cam.point == ft.GetPoint(i))
      {
        LOG(LINFO, ("Camera snapped!"));
        m_result.emplace_back(cam.fID, ft.GetID().m_index, i);
        break;
      }
    }
  }
}

void SpeedCameraIndexBuilder::Serialize(Writer & writer)
{
  uint32_t const size = static_cast<uint32_t>(m_result.size());
  WriteVarUint(writer, size);
  writer.Write(&m_result[0], size * sizeof(SpeedCamera));
  LOG(LINFO, ("Write ", size, "speed camera records."));
}

void SpeedCameraIndex::GetCamerasByFID(uint32_t fid, vector<SpeedCamera> & cameras) const
{
  //TODO (ldragunov) Rewrite to half division find
  for (auto const & cam : m_cameras)
    if (cam.roadFID == fid)
      cameras.push_back(cam);
}
}  // namesoace routing
