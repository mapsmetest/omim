#pragma once

#include "shape.hpp"

namespace gui
{

class Ruler : public Shape
{
public:
  Ruler(gui::Position const & position) : Shape(position) {}
  dp::TransferPointer<ShapeRenderer> Draw(dp::RefPointer<dp::TextureManager> tex) const override;

private:
  void DrawRuler(ShapeControl & control, dp::RefPointer<dp::TextureManager> tex) const;
  void DrawText(ShapeControl & control, dp::RefPointer<dp::TextureManager> tex) const;
};

}
