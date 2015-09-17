#include "speedcam_generator.hpp"

#include "coding/file_writer.hpp"

#include "indexer/classificator.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/index.hpp"
#include "indexer/scales.hpp"

namespace routing
{
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

void SpeedCameraIndexBuilder::Serialize(FileWriter & writer)
{
  WriteVarUint(writer, m_result.size());
  for (auto const & cam : m_result)
    writer.Write(&cam, sizeof(cam));
  writer.WritePaddingByEnd(4);
  LOG(LINFO, ("Write ", m_result.size(), "speed camera records."));
}
}  // namespace routing
