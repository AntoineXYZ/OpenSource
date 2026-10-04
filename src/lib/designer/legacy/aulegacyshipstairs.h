#ifndef AuLegacyShipStairs_H
#define AuLegacyShipStairs_H

#include "aulegacyshipmesh.h"

namespace AuLegacyShip
{
namespace Rendering
{
// Original straight, quarter-turn and U-turn stairs, including all eight
// encoded orientations. Floor openings come from authored open-below rooms.
void AppendStairGeometry(const Design& design, Mesh& mesh);
}
}

#endif
