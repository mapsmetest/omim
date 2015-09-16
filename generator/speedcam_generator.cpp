#include "speedcam_generator.hpp"

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
}  // namespace routing
