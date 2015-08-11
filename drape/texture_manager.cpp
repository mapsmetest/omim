#include "drape/texture_manager.hpp"
#include "drape/symbols_texture.hpp"
#include "drape/font_texture.hpp"
#include "drape/stipple_pen_resource.hpp"
#include "drape/texture_of_colors.hpp"
#include "drape/glfunctions.hpp"
#include "drape/utils/glyph_usage_tracker.hpp"

#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/reader.hpp"

#include "base/logging.hpp"
#include "base/stl_add.hpp"
#include "base/string_utils.hpp"

#include "std/vector.hpp"
#include "std/bind.hpp"

namespace dp
{

uint32_t const STIPPLE_TEXTURE_WIDTH = 512;
uint32_t const MIN_STIPPLE_TEXTURE_HEIGHT = 64;
uint32_t const MIN_COLOR_TEXTURE_SIZE = 64;
size_t const INVALID_GLYPH_GROUP = numeric_limits<size_t>::max();

size_t const PIXELS_PER_COLOR = 2;

// number of glyphs (since 0) which will be in each texture
size_t const DUPLICATED_GLYPHS_COUNT = 128;

size_t const RESERVED_PATTERNS = 10;
size_t const RESERVED_COLORS = 20;

namespace
{

void MultilineTextToUniString(TextureManager::TMultilineText const & text, strings::UniString & outString)
{
  size_t cnt = 0;
  for (strings::UniString const & str : text)
    cnt += str.size();

  outString.clear();
  outString.reserve(cnt);
  for (strings::UniString const & str : text)
    outString.append(str.begin(), str.end());
}

template <typename ToDo>
void ParseColorsList(string const & colorsFile, ToDo toDo)
{
  string colorsList;
  try
  {
    ReaderPtr<Reader>(GetPlatform().GetReader(colorsFile)).ReadAsString(colorsList);
  }
  catch(RootException const & e)
  {
    LOG(LWARNING, ("Error reading colors list ", colorsFile, " : ", e.what()));
    return;
  }

  istringstream fin(colorsList);
  while (true)
  {
    uint32_t color;
    fin >> color;
    if (!fin)
      break;

    toDo(dp::Extract(color));
  }
}

template <typename ToDo>
void ParsePatternsList(string const & patternsFile, ToDo toDo)
{
  string patternsList;
  try
  {
    ReaderPtr<Reader>(GetPlatform().GetReader(patternsFile)).ReadAsString(patternsList);
  }
  catch(RootException const & e)
  {
    LOG(LWARNING, ("Error reading patterns list ", patternsFile, " : ", e.what()));
    return;
  }

  strings::Tokenize(patternsList, "\n", [&](string const & patternStr)
  {
    if (patternStr.empty())
      return;

    buffer_vector<double, 8> pattern;
    strings::Tokenize(patternStr, " ", [&](string const & token)
    {
      double d = 0.0;
      strings::to_double(token, d);
      pattern.push_back(d);
    });

    bool isValid = true;
    for (size_t i = 0; i < pattern.size(); i++)
    {
      if (fabs(pattern[i]) < 1e-5)
      {
        isValid = false;
        break;
      }
    }

    if (isValid)
      toDo(pattern);
  });
}

} // namespace

TextureManager::TextureManager()
  : m_maxTextureSize(0)
  , m_maxGlypsCount(0)
{
  m_nothingToUpload.test_and_set();
}

TextureManager::BaseRegion::BaseRegion()
  : m_info(nullptr)
  , m_texture(nullptr)
{}

bool TextureManager::BaseRegion::IsValid() const
{
  return m_info != nullptr && m_texture != nullptr;
}

void TextureManager::BaseRegion::SetResourceInfo(ref_ptr<Texture::ResourceInfo> info)
{
  m_info = info;
}

void TextureManager::BaseRegion::SetTexture(ref_ptr<Texture> texture)
{
  m_texture = texture;
}

m2::PointU TextureManager::BaseRegion::GetPixelSize() const
{
  ASSERT(IsValid(), ());
  m2::RectF const & texRect = m_info->GetTexRect();
  return  m2::PointU(ceil(texRect.SizeX() * m_texture->GetWidth()),
                     ceil(texRect.SizeY() * m_texture->GetHeight()));
}

uint32_t TextureManager::BaseRegion::GetPixelHeight() const
{
  return ceil(m_info->GetTexRect().SizeY() * m_texture->GetHeight());
}

m2::RectF const & TextureManager::BaseRegion::GetTexRect() const
{
  return m_info->GetTexRect();
}

TextureManager::GlyphRegion::GlyphRegion()
  : BaseRegion()
{
}

float TextureManager::GlyphRegion::GetOffsetX() const
{
  ASSERT(m_info->GetType() == Texture::Glyph, ());
  return ref_ptr<GlyphInfo>(m_info)->GetMetrics().m_xOffset;
}

float TextureManager::GlyphRegion::GetOffsetY() const
{
  ASSERT(m_info->GetType() == Texture::Glyph, ());
  return ref_ptr<GlyphInfo>(m_info)->GetMetrics().m_yOffset;
}

float TextureManager::GlyphRegion::GetAdvanceX() const
{
  ASSERT(m_info->GetType() == Texture::Glyph, ());
  return ref_ptr<GlyphInfo>(m_info)->GetMetrics().m_xAdvance;
}

float TextureManager::GlyphRegion::GetAdvanceY() const
{
  ASSERT(m_info->GetType() == Texture::Glyph, ());
  return ref_ptr<GlyphInfo>(m_info)->GetMetrics().m_yAdvance;
}

uint32_t TextureManager::StippleRegion::GetMaskPixelLength() const
{
  ASSERT(m_info->GetType() == Texture::StipplePen, ());
  return ref_ptr<StipplePenResourceInfo>(m_info)->GetMaskPixelLength();
}

uint32_t TextureManager::StippleRegion::GetPatternPixelLength() const
{
  ASSERT(m_info->GetType() == Texture::StipplePen, ());
  return ref_ptr<StipplePenResourceInfo>(m_info)->GetPatternPixelLength();
}

void TextureManager::Release()
{
  m_glyphGroups.clear();
  m_hybridGlyphGroups.clear();

  m_symbolTexture.reset();
  m_stipplePenTexture.reset();
  m_colorTexture.reset();

  m_glyphTextures.clear();

  m_glyphManager.reset();
}

bool TextureManager::UpdateDynamicTextures()
{
  bool const asyncRoutines = HasAsyncRoutines(m_glyphGroups) || HasAsyncRoutines(m_hybridGlyphGroups);

  if (!asyncRoutines && m_nothingToUpload.test_and_set())
    return false;

  m_colorTexture->UpdateState();
  m_stipplePenTexture->UpdateState();

  UpdateGlyphTextures(m_glyphGroups);
  UpdateGlyphTextures(m_hybridGlyphGroups);

  return true;
}

ref_ptr<Texture> TextureManager::AllocateGlyphTexture()
{
  m2::PointU size(m_maxTextureSize, m_maxTextureSize);
  m_glyphTextures.push_back(make_unique_dp<FontTexture>(size, make_ref(m_glyphManager)));
  return make_ref(m_glyphTextures.back());
}

void TextureManager::GetRegionBase(ref_ptr<Texture> tex, TextureManager::BaseRegion & region, Texture::Key const & key)
{
  bool isNew = false;
  region.SetResourceInfo(tex != nullptr ? tex->FindResource(key, isNew) : nullptr);
  region.SetTexture(tex);
  ASSERT(region.IsValid(), ());
  if (isNew)
    m_nothingToUpload.clear();
}

size_t TextureManager::FindGlyphsGroup(strings::UniChar const & c) const
{
  auto const iter = lower_bound(m_glyphGroups.begin(), m_glyphGroups.end(), c, [](GlyphGroup const & g, strings::UniChar const & c)
  {
    return g.m_endChar < c;
  });

  if (iter == m_glyphGroups.end())
    return INVALID_GLYPH_GROUP;

  return distance(m_glyphGroups.begin(), iter);
}

size_t TextureManager::FindGlyphsGroup(strings::UniString const & text) const
{
  size_t groupIndex = INVALID_GLYPH_GROUP;
  for (strings::UniChar const & c : text)
  {
    // skip glyphs which can be duplicated
    if (c < DUPLICATED_GLYPHS_COUNT)
      continue;

    size_t currentIndex = FindGlyphsGroup(c);

    // an invalid glyph found
    if (currentIndex == INVALID_GLYPH_GROUP)
    {
#if defined(TRACK_GLYPH_USAGE)
      GlyphUsageTracker::Instance().AddInvalidGlyph(text, c);
#endif
      return INVALID_GLYPH_GROUP;
    }

    // check if each glyph in text id in one group
    if (groupIndex == INVALID_GLYPH_GROUP)
      groupIndex = currentIndex;
    else if (groupIndex != currentIndex)
    {
#if defined(TRACK_GLYPH_USAGE)
      GlyphUsageTracker::Instance().AddUnexpectedGlyph(text, c, currentIndex, groupIndex);
#endif
      return INVALID_GLYPH_GROUP;
    }
  }

  // all glyphs in duplicated range
  if (groupIndex == INVALID_GLYPH_GROUP)
    groupIndex = FindGlyphsGroup(text[0]);

  return groupIndex;
}

size_t TextureManager::FindGlyphsGroup(TMultilineText const & text) const
{
  strings::UniString combinedString;
  MultilineTextToUniString(text, combinedString);

  return FindGlyphsGroup(combinedString);
}

size_t TextureManager::GetNumberOfUnfoundCharacters(strings::UniString const & text, HybridGlyphGroup const & group) const
{
  size_t cnt = 0;
  for (strings::UniChar const & c : text)
    if (group.m_glyphs.find(c) == group.m_glyphs.end())
      cnt++;

  return cnt;
}

void TextureManager::MarkCharactersUsage(strings::UniString const & text, HybridGlyphGroup & group)
{
  for (strings::UniChar const & c : text)
    group.m_glyphs.emplace(c);
}

size_t TextureManager::FindHybridGlyphsGroup(strings::UniString const & text)
{
  if (m_hybridGlyphGroups.empty())
  {
    m_hybridGlyphGroups.push_back(HybridGlyphGroup());
    return 0;
  }

  // if we have got the only hybrid texture (in most cases it is) we can omit checking of glyphs usage
  size_t const glyphsCount = m_hybridGlyphGroups.back().m_glyphs.size() + text.size();
  if (m_hybridGlyphGroups.size() == 1 && glyphsCount < m_maxGlypsCount)
    return 0;

  // looking for a hybrid texture which contains text entirely
  for (size_t i = 0; i < m_hybridGlyphGroups.size() - 1; i++)
    if (GetNumberOfUnfoundCharacters(text, m_hybridGlyphGroups[i]) == 0)
      return i;

  // check if we can contain text in the last hybrid texture
  size_t const unfoundChars = GetNumberOfUnfoundCharacters(text, m_hybridGlyphGroups.back());
  size_t const newCharsCount = m_hybridGlyphGroups.back().m_glyphs.size() + unfoundChars;
  if (newCharsCount >= m_maxGlypsCount)
    m_hybridGlyphGroups.push_back(HybridGlyphGroup());

  return m_hybridGlyphGroups.size() - 1;
}

size_t TextureManager::FindHybridGlyphsGroup(TMultilineText const & text)
{
  strings::UniString combinedString;
  MultilineTextToUniString(text, combinedString);

  return FindHybridGlyphsGroup(combinedString);
}

void TextureManager::Init(Params const & params)
{
  GLFunctions::glPixelStore(gl_const::GLUnpackAlignment, 1);

  m_symbolTexture = make_unique_dp<SymbolsTexture>(params.m_resPostfix);

  // initialize patterns
  list<buffer_vector<uint8_t, 8>> patterns;
  double const visualScale = params.m_visualScale;
  ParsePatternsList(params.m_patterns, [&patterns, visualScale](buffer_vector<double, 8>  const & pattern)
  {
    buffer_vector<uint8_t, 8> p;
    for (size_t i = 0; i < pattern.size(); i++)
      p.push_back(pattern[i] * visualScale);
    patterns.push_back(move(p));
  });

  size_t const stippleTextureHeight = max(my::NextPowOf2(patterns.size() + RESERVED_PATTERNS), MIN_STIPPLE_TEXTURE_HEIGHT);
  m_stipplePenTexture = make_unique_dp<StipplePenTexture>(m2::PointU(STIPPLE_TEXTURE_WIDTH, stippleTextureHeight));
  LOG(LDEBUG, ("Pattens texture size = ", m_stipplePenTexture->GetWidth(), m_stipplePenTexture->GetHeight()));

  ref_ptr<ColorTexture> stipplePenTextureTex = make_ref(m_stipplePenTexture);
  //for (auto it = patterns.begin(); it != patterns.end(); ++it)
  //  stipplePenTextureTex->ReservePattern(*it);

  // initialize colors
  list<dp::Color> colors;
  ParseColorsList(params.m_colors, [&colors](dp::Color const & color)
  {
    colors.push_back(color);
  });

  size_t const colorTextureSize = max(my::NextPowOf2(floor(sqrt(colors.size() + RESERVED_COLORS)) * PIXELS_PER_COLOR), MIN_COLOR_TEXTURE_SIZE);
  m_colorTexture = make_unique_dp<ColorTexture>(m2::PointU(colorTextureSize, colorTextureSize));
  LOG(LDEBUG, ("Colors texture size = ", m_colorTexture->GetWidth(), m_colorTexture->GetHeight()));

  ref_ptr<ColorTexture> colorTex = make_ref(m_colorTexture);
  for (auto it = colors.begin(); it != colors.end(); ++it)
    colorTex->ReserveColor(*it);

  m_glyphManager = make_unique_dp<GlyphManager>(params.m_glyphMngParams);
  m_maxTextureSize = min(2048, GLFunctions::glGetInteger(gl_const::GLMaxTextureSize));

  uint32_t const textureSquare = m_maxTextureSize * m_maxTextureSize;
  uint32_t const baseGlyphHeight = params.m_glyphMngParams.m_baseGlyphHeight;
  uint32_t const avarageGlyphSquare = baseGlyphHeight * baseGlyphHeight;

  m_glyphGroups.push_back(GlyphGroup());
  m_maxGlypsCount = ceil(0.9 * textureSquare / avarageGlyphSquare);
  m_glyphManager->ForEachUnicodeBlock([this](strings::UniChar const & start, strings::UniChar const & end)
  {
    if (m_glyphGroups.empty())
    {
      m_glyphGroups.push_back(GlyphGroup(start, end));
      return;
    }

    GlyphGroup & group = m_glyphGroups.back();
    ASSERT_LESS_OR_EQUAL(group.m_endChar, start, ());

    if (end - group.m_startChar < m_maxGlypsCount)
      group.m_endChar = end;
    else
      m_glyphGroups.push_back(GlyphGroup(start, end));
  });
}

void TextureManager::Invalidate(string const & resPostfix)
{
  ASSERT(m_symbolTexture != nullptr, ());
  ref_ptr<SymbolsTexture> symbolsTexture = make_ref(m_symbolTexture);
  symbolsTexture->Invalidate(resPostfix);
}

void TextureManager::GetSymbolRegion(string const & symbolName, SymbolRegion & region)
{
  GetRegionBase(make_ref(m_symbolTexture), region, SymbolsTexture::SymbolKey(symbolName));
}

void TextureManager::GetStippleRegion(TStipplePattern const & pen, StippleRegion & region)
{
  GetRegionBase(make_ref(m_stipplePenTexture), region, StipplePenKey(pen));
}

void TextureManager::GetColorRegion(Color const & color, ColorRegion & region)
{
  GetRegionBase(make_ref(m_colorTexture), region, ColorKey(color));
}

void TextureManager::GetGlyphRegions(TMultilineText const & text, TMultilineGlyphsBuffer & buffers)
{
  CalcGlyphRegions<TMultilineText, TMultilineGlyphsBuffer>(text, buffers);
}

void TextureManager::GetGlyphRegions(strings::UniString const & text, TGlyphsBuffer & regions)
{
  CalcGlyphRegions<strings::UniString, TGlyphsBuffer>(text, regions);
}

constexpr size_t TextureManager::GetInvalidGlyphGroup()
{
  return INVALID_GLYPH_GROUP;
}

} // namespace dp
