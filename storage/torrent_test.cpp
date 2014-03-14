#include "torrent_test.hpp"

#include "../base/logging.hpp"

#include "../platform/platform.hpp"

#include "../std/target_os.hpp"

#include "libtorrent/entry.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/session.hpp"

libtorrent::session g_session;

void TorrentTest::Start()
{
  libtorrent::error_code ec;
  g_session.listen_on(std::make_pair(6881, 6889), ec);
  if (ec)
  {
    LOG(LWARNING, ("TORRENT: failed to open listen socket:", ec.message()));
    return;
  }
  libtorrent::add_torrent_params p;
  p.save_path = GetPlatform().WritableDir();
  p.ti = new libtorrent::torrent_info(p.save_path + "Belarus.torrent.imported", ec);
  if (ec)
  {
    LOG(LWARNING, (ec.message()));
    return;
  }
  g_session.add_torrent(p, ec);
  if (ec)
  {
    LOG(LWARNING, (ec.message()));
    return;
  }
  LOG(LINFO, ("Torrent successfully imported"));
}
