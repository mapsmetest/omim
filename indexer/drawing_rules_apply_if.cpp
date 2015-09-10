#include "drawing_rules_apply_if.hpp"

#include "base/logging.hpp"

namespace drule
{

namespace
{

unique_ptr<ApplyIf> ParsePopulationLess(string const & apply_if)
{
  // less(population,<number>)

  char const t[] = "less(population,";
  if (apply_if.find(t) != 0)
    return unique_ptr<ApplyIf>();

  size_t const b = sizeof(t);
  size_t const e = apply_if.find(b, ')');
  if (e == string::npos)
    return unique_ptr<ApplyIf>();

  string const number(apply_if, b, e - b - 1);
  return make_unique<ApplyIfPopulationLess>(stoi(number));
}

unique_ptr<ApplyIf> ParsePopulationGreater(string const & apply_if)
{
  // greater(population,<number>)

  char const t[] = "greater(population,";
  if (apply_if.find(t) != 0)
    return unique_ptr<ApplyIf>();

  size_t const b = sizeof(t);
  size_t const e = apply_if.find(b, ')');
  if (e == string::npos)
    return unique_ptr<ApplyIf>();

  string const number(apply_if, b, e - b - 1);
  return make_unique<ApplyIfPopulationLess>(stoi(number));
}

unique_ptr<ApplyIf> ParsePopulationBetween(string const & apply_if)
{
  // between(population,<number1>,<number2>)

  char const t[] = "between(population,";
  if (apply_if.find(t) != 0)
    return unique_ptr<ApplyIf>();

  size_t const b1 = sizeof(t);
  size_t const e1 = apply_if.find(b1, ',');
  if (e1 == string::npos)
    return unique_ptr<ApplyIf>();

  size_t const b2 = e1 + 1;
  size_t const e2 = apply_if.find(b2, ')');
  if (e2 == string::npos)
    return unique_ptr<ApplyIf>();

  string const number1(apply_if, b1, e1 - b1 - 1);
  string const number2(apply_if, b2, e2 - b2 - 1);
  return make_unique<ApplyIfPopulationBetween>(stoi(number1), stoi(number2));
}

}  // namespace

// ----------------------------------------------------------------------------------------------------------------------------

bool ApplyIfAlways::Test(Context const & /* context */) const
{
  return true;
}

// ----------------------------------------------------------------------------------------------------------------------------

bool ApplyIfNever::Test(Context const & /* context */) const
{
  return false;
}

// ----------------------------------------------------------------------------------------------------------------------------

ApplyIfPopulationBetween::ApplyIfPopulationBetween(uint32_t from, uint32_t to)
  : m_from(from)
  , m_to(to)
{}

bool ApplyIfPopulationBetween::Test(Context const & context) const
{
  return context.m_population >= m_from && context.m_population <= m_to;
}

// ----------------------------------------------------------------------------------------------------------------------------

ApplyIfPopulationLess::ApplyIfPopulationLess(uint32_t than)
  : m_than(than)
{}

bool ApplyIfPopulationLess::Test(Context const & context) const
{
  return context.m_population <= m_than;
}

// ----------------------------------------------------------------------------------------------------------------------------

ApplyIfPopulationGreater::ApplyIfPopulationGreater(uint32_t than)
  : m_than(than)
{}

bool ApplyIfPopulationGreater::Test(Context const & context) const
{
  return context.m_population >= m_than;
}

// ----------------------------------------------------------------------------------------------------------------------------

unique_ptr<ApplyIf> ParseApplyIf(string const & apply_if)
{
  if (apply_if.empty())
    return make_unique<ApplyIfAlways>();

  typedef unique_ptr<ApplyIf>(* TParser)(string const &);

  TParser const p[] =
  {
    &ParsePopulationLess,
    &ParsePopulationGreater,
    &ParsePopulationBetween,
    nullptr
  };

  for (size_t i = 0; p[i] != nullptr; ++i)
  {
    unique_ptr<ApplyIf> res = (*p[i])(apply_if);
    if (nullptr != res)
      return res;
  }

  LOG(LERROR, ("ApplyIf was not parsed correctly, ApplyIfNever is used instead", apply_if));
  return make_unique<ApplyIfNever>();
}

}  // namespace drule
