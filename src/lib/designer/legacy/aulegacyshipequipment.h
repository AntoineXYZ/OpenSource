#ifndef AuLegacyShipEquipment_H
#define AuLegacyShipEquipment_H

#include "aulegacyshipmesh.h"

namespace AuLegacyShip
{
namespace Rendering
{
// Procedural models and structural details recovered from the 2016 renderer.
// Resource-backed consoles, furniture and vehicles have a separate importer.
bool EquipmentSupported(int detailType);
void AppendEquipment(const Design& design, Mesh& mesh);
}
}

#endif
