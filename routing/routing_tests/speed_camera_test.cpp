#include "testing/testing.hpp"

#include "routing/speed_camera.hpp"

#include "indexer/index.hpp"

using namespace routing;

namespace routing
{
UNIT_TEST(SpeedCameraSerializationTest)
{
  Index index;
  SpeedCameraIndexBuilder builder(index);
  builder.m_result.emplace_back(1 /* camFID */, 2 /* roadFID */, 3 /* offset */);
  builder.m_result.emplace_back(5 /* camFID */, 6 /* roadFID */, 7 /* offset */);

  vector<uint8_t> rawData;
  MemWriter<vector<uint8_t>> writer(rawData);
  builder.Serialize(writer);

  ModelReaderPtr::ReaderPtr<MemReader> model(new MemReader(&rawData[0], rawData.size()));
  SpeedCameraIndex camIndex;
  camIndex.Load(model);

  auto result = camIndex.GetCameras();

  TEST_EQUAL(result.size(), 2, ());
  TEST_EQUAL(result.front().roadFID, 2, ());
}

UNIT_TEST(SpeedCameraSortTest)
{
  Index index;
  SpeedCameraIndexBuilder builder(index);
  builder.m_result.emplace_back(5 /* camFID */, 6 /* roadFID */, 7 /* offset */);
  builder.m_result.emplace_back(1 /* camFID */, 2 /* roadFID */, 3 /* offset */);

  vector<uint8_t> rawData;
  MemWriter<vector<uint8_t>> writer(rawData);
  builder.Serialize(writer);

  ModelReaderPtr::ReaderPtr<MemReader> model(new MemReader(&rawData[0], rawData.size()));
  SpeedCameraIndex camIndex;
  camIndex.Load(model);

  auto result = camIndex.GetCameras();

  TEST_EQUAL(result.size(), 2, ());
  TEST_EQUAL(result.front().roadFID, 2, ());
  TEST_EQUAL(result.front().camFID, 1, ());
  TEST_EQUAL(result.back().roadFID, 6, ());
  TEST_EQUAL(result.back().camFID, 5, ());
}

UNIT_TEST(SpeedCameraGetCameraByFIDTest)
{
  Index index;
  SpeedCameraIndexBuilder builder(index);
  builder.m_result.emplace_back(5 /* camFID */, 6 /* roadFID */, 7 /* offset */);
  builder.m_result.emplace_back(1 /* camFID */, 2 /* roadFID */, 3 /* offset */);
  builder.m_result.emplace_back(8 /* camFID */, 6 /* roadFID */, 3 /* offset */);

  vector<uint8_t> rawData;
  MemWriter<vector<uint8_t>> writer(rawData);
  builder.Serialize(writer);

  ModelReaderPtr::ReaderPtr<MemReader> model(new MemReader(&rawData[0], rawData.size()));
  SpeedCameraIndex camIndex;
  camIndex.Load(model);

  vector<SpeedCamera> candidates;
  camIndex.GetCamerasByFID(6, candidates);
  TEST_EQUAL(candidates.size(), 2, ());
  camIndex.GetCamerasByFID(2, candidates);
  TEST_EQUAL(candidates.size(), 1, ());
  camIndex.GetCamerasByFID(1, candidates);
  TEST_EQUAL(candidates.size(), 0, ());
}
}  // namespace
