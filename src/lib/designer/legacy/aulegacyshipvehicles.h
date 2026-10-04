#ifndef AuLegacyShipVehicles_H
#define AuLegacyShipVehicles_H

#include <QRectF>

namespace AuLegacyShip
{
struct Design;
namespace Rendering
{
struct Mesh;
bool VehicleSupported(int commodity);
bool ResourceEquipmentSupported(int detailType);
QRectF VehicleFootprint(int commodity);
void AppendResourceEquipment(const Design& design, Mesh& mesh);
}
}

#endif
