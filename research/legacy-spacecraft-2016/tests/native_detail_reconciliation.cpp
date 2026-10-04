// Manual native regression harness. The assistant must never compile this.
#include "aulegacyshipdetails.h"
#include <QCoreApplication>
#include <QTextStream>
#include <cstdlib>

using namespace AuLegacyShip;
namespace
{
void Require(bool value, const char* message)
{
	if(!value) {QTextStream(stderr)<<message<<'\n';std::exit(1);}
}
quint16 Packed(int x,int y) {return (quint16(quint8(x))<<8)|quint8(y);}
Room Square(int id,int type=8,int deck=0,int x=0,int number=0)
{
	Room r; r.roomId=quint16(id);r.roomType=quint8(type);r.deck=qint8(deck);r.roomSubtype=quint8(number);
	r.packedVertices={Packed(x,0),Packed(x,4),Packed(x+4,4),Packed(x+4,0)};
	return r;
}
Detail Equipment(int id,int type,int x,int y,int deck=0,int room=0)
{
	Detail d;d.detailId=quint32(id);d.detailType=quint8(type);d.packedXY=Packed(x,y);d.deck=qint8(deck);d.roomId=quint16(room);return d;
}
void Verify(Design& d) {QString error;Require(ReconcileDetails(d,error),qPrintable(error));}
}
int main(int argc,char** argv)
{
	QCoreApplication app(argc,argv);
	Design d;
	d.rooms={Square(42)};
	d.details={Equipment(1,10,2,2)};
	Verify(d);
	Require(d.details.size()==1&&d.details[0].roomId==42,"Engine equipment must acquire its containing room ID.");
	d.rooms.clear();Verify(d);
	Require(d.details.isEmpty(),"Erasing the supporting room must remove engine equipment.");
	d.rooms={Square(42,2)};
	d.details={Equipment(1,6,2,2),Equipment(2,10,1,1)};
	Verify(d);
	Require(d.details.size()==1&&d.details[0].detailType==6,"Only bridge-allowed equipment may use the bridge fallback.");
	d.details={Equipment(1,1,1,1,0,42),Equipment(2,1,3,3,0,42)};
	Verify(d);
	Require(d.details.size()==1&&d.details[0].detailId==2,"Duplicate reconciliation must retain the last surviving record in list order.");
	d.rooms={Square(42,3)};
	d.details={Equipment(1,0x15,0,2),Equipment(2,0x15,0,0)};
	Verify(d);
	Require(d.details.size()==1&&d.details[0].detailType==0x25,"Side turret must face outward; hull vertices cannot host side details.");
	d.rooms={Square(42,3),Square(43,3,0,4)};
	d.details={Equipment(1,5,4,2)};
	Verify(d);
	Require(d.details.size()==1&&d.details[0].roomId==43,"Interior door requires two rooms and receives the last matching room ID.");
	d.rooms.removeLast();Verify(d);
	Require(d.details.isEmpty(),"Interior door cannot survive on an exterior wall.");
	d.rooms={Square(42,3),Square(43,3,1)};
	d.details={Equipment(1,8,2,2,1)};
	Verify(d);
	Require(d.details.size()==1&&d.details[0].roomId==43,"Interior hatch needs a room at its deck and one below.");
	d.rooms.removeLast();Verify(d);
	Require(d.details.isEmpty(),"Interior hatch is invalid after its upper room disappears.");
	d.rooms={Square(42,3)};
	d.details={Equipment(1,0x65,0,0)};
	Verify(d);
	Require(d.details.size()==1&&d.details[0].roomId==0,"Stair validation checks its same-deck footprint and preserves stored room ID.");
	d.rooms={Square(42,9)};Verify(d);
	Require(d.details.isEmpty(),"Stairs cannot use a fuel room.");
	d.rooms={Square(42,3)};
	d.details={Equipment(1,0x21,2,2,1)};
	Verify(d);
	Require(d.details.size()==1&&d.details[0].detailType==0x16&&d.details[0].roomId==42,"Exterior top turret must attach to the room below and retype.");
	d.details={Equipment(1,0x16,2,2)};
	Verify(d);
	Require(d.details.size()==1&&d.details[0].detailType==0x21,"Exterior bottom turret must retype inside its supporting room.");
	d.rooms={Square(42,21,0,0,7),Square(43,21,1,0,7)};
	d.details={Equipment(1,0x63,1,1,0,42),Equipment(2,0x63,1,1,1,43)};
	Verify(d);
	Require(d.details.size()==1&&d.details[0].detailId==2,"Weapon emitter limit applies across decks sharing a Room Number.");
	Room empty=Square(42,21,42,0,1);
	empty.packedVertices.clear();
	d.rooms={empty,Square(43,21,0,0,1)};
	d.details={Equipment(1,0x63,1,1,0,43),Equipment(2,0x63,1,1,1,42)};
	Verify(d);
	Require(d.details.size()==1&&d.details[0].detailId==1,"Discarded empty rooms must not supply a Room Number to weapon-emitter duplicate checks.");
	d.details={Equipment(1,0x4c,1,1,2)};
	Verify(d);
	Require(d.details.isEmpty(),"An empty source room on a high deck must not extend the valid pivot-point bounds.");
	Room hangar=Square(42,13);
	hangar.packedVertices={Packed(0,0),Packed(0,12),Packed(12,12),Packed(12,0)};
	d.rooms={hangar};
	Detail suv=Equipment(1,0x39,6,6);suv.commodityIdNormalized=82;
	d.details={suv};Verify(d);
	Require(d.details.size()==1,"SUV footprint must fit the large hangar using original model bounds.");
	d.rooms[0].packedVertices={Packed(4,4),Packed(4,8),Packed(8,8),Packed(8,4)};
	Verify(d);
	Require(d.details.isEmpty(),"SUV footprint must reject a small hangar even when its center remains inside.");
	d.rooms={Square(42,13)};
	Detail missingModel=Equipment(1,0x39,2,2);missingModel.commodityIdNormalized=54;
	d.details={missingModel};Verify(d);
	Require(d.details.size()==1,"A resource absent in the original distribution must keep its original zero-extent fallback.");
	d.rooms={Square(42,13)};
	Detail vehicle=Equipment(2,0x39,2,2);vehicle.commodityIdNormalized=0xfffe;
	d.details={Equipment(1,255,1,1),vehicle};
	QString error;
	Require(!ReconcileDetails(d,error)&&!error.isEmpty(),"Unavailable vehicle data must reject reconciliation explicitly.");
	Require(d.details.size()==2&&d.details[0].detailType==255,"Failed reconciliation must not partially delete equipment.");
	QTextStream(stdout)<<"Equipment reconciliation fixtures passed.\n";
	return 0;
}
