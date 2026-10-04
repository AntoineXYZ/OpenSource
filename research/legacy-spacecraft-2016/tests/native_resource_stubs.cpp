// Only for the user-built CPU surface harness; resource models need the client
// resource libraries and are exercised by the separate interactive checks.
// Never link this file into the client or a resource-model test.
#include "aulegacyshipvehicles.h"

namespace AuLegacyShip
{
namespace Rendering
{
void AppendResourceEquipment(const Design&, Mesh&) {}
bool ResourceEquipmentSupported(int) { return false; }
bool VehicleSupported(int) { return false; }
QRectF VehicleFootprint(int) { return QRectF(); }
}
}
