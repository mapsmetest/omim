#pragma once

#include "std/string.hpp"
#include "std/unique_ptr.hpp"

namespace drule
{

struct Context
{
  uint32_t m_population;

  Context(uint32_t population)
    : m_population(population)
  {}
};

class ApplyIf
{
public:
  virtual bool Test(Context const & context) const = 0;
  virtual ~ApplyIf() = default;
};

class ApplyIfAlways : public ApplyIf
{
public:
  bool Test(Context const & context) const override;
};

class ApplyIfNever : public ApplyIf
{
public:
  bool Test(Context const & context) const override;
};

class ApplyIfPopulationBetween : public ApplyIf
{
public:
  ApplyIfPopulationBetween(uint32_t from, uint32_t to);
  bool Test(Context const & context) const override;

private:
  uint32_t const m_from;
  uint32_t const m_to;
};

class ApplyIfPopulationLess : public ApplyIf
{
public:
  ApplyIfPopulationLess(uint32_t than);
  bool Test(Context const & context) const override;

private:
  uint32_t const m_than;
};

class ApplyIfPopulationGreater : public ApplyIf
{
public:
  ApplyIfPopulationGreater(uint32_t than);
  bool Test(Context const & context) const override;

private:
  uint32_t const m_than;
};

unique_ptr<ApplyIf> ParseApplyIf(string const & apply_if);

}  // namespace drule
