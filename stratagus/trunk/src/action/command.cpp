//   ___________		     _________		      _____  __
//   \_	  _____/______   ____   ____ \_   ___ \____________ _/ ____\/  |_
//    |    __) \_  __ \_/ __ \_/ __ \/    \  \/\_  __ \__  \\   __\\   __\ 
//    |     \   |  | \/\  ___/\  ___/\     \____|  | \// __ \|  |   |  |
//    \___  /   |__|    \___  >\___  >\______  /|__|  (____  /__|   |__|
//	  \/		    \/	   \/	     \/		   \/
//  ______________________                           ______________________
//			  T H E   W A R   B E G I N S
//	   FreeCraft - A free fantasy real time strategy game engine
//
/**@name command.c	-	Give units a command. */
//
//	(c) Copyright 1998,2000,2001 by Lutz Sammer
//
//	$Id$

//@{

/*----------------------------------------------------------------------------
--	Includes
----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>

#include "freecraft.h"
#include "unittype.h"
#include "player.h"
#include "unit.h"
#include "actions.h"
#include "tileset.h"
#include "map.h"
#include "upgrade.h"
#include "pathfinder.h"
#include "spells.h"
#include "interface.h"

/*----------------------------------------------------------------------------
--	Functions
----------------------------------------------------------------------------*/

#ifdef NEW_ORDERS
/**
**	Release an order.
**
**	@param order	Pointer to order.
*/
local void ReleaseOrder(Order* order)
{
    if( order->Goal ) {
	RefsDebugCheck( !order->Goal->Refs );
	if( !--order->Goal->Refs ) {
	    ReleaseUnit(order->Goal);
	}
	order->Goal=NoUnitP;
    }
}

/**
**	Get next free order slot.
**
**	@param unit	pointer to unit.
**	@param flush	if true, flush order queue.
**
**	@return		Pointer to next free order slot.
*/
local Order* GetNextOrder(Unit* unit,int flush)
{
    if( flush ) {			// empty command queue
	if( unit->OrderCount>1 ) {
	    DebugLevel0Fn("FIXME: must free the order queue\n");
	}
	unit->OrderCount=unit->OrderFlush=1;
	// Order 0 must stopped in the action loop.
    } else if( unit->OrderCount==MAX_ORDERS ) {
	// FIXME: johns: wrong place for an error message.
	// FIXME: johns: should be checked by AI or the user interface
	if( unit->Player==ThisPlayer ) {
	    // FIXME: use a general notify call
            SetMessage( "Unit order list is full" );
	}
	return NULL;
    }

    return &unit->Orders[(int)unit->OrderCount++];
}

/**
**	Clear the saved action.
**
**	@param unit	Unit pointer, that get the saved action cleared.
**
**	If we make an new order, we must clear any saved actions.
**	Internal functions, must protect it, if needed.
*/
local void ClearSavedAction(Unit* unit)
{
    unit->SavedOrder.Action=UnitActionStill;	// clear saved action
    unit->SavedOrder.X=-1;
    unit->SavedOrder.Y=-1;
    ReleaseOrder(&unit->SavedOrder);
    unit->SavedOrder.Type=NULL;
    unit->SavedOrder.Arg1=NULL;
}

#else

/**
**	Get next command.
**
**	@param unit	pointer to unit.
**	@param flush	if true, flush command queue.
**
**	@return		Pointer to next free command.
*/
local Command* GetNextCommand(Unit* unit,int flush)
{
    if( flush ) {			// empty command queue
	unit->NextCount=0;
	unit->NextFlush=1;
    } else if( unit->NextCount==MAX_COMMANDS ) {
	// FIXME: johns: wrong place for an error message.
	// FIXME: johns: should be checked by AI or the user interface
	if( unit->Player==ThisPlayer ) {
            SetMessage( "Unit action list is full" );
	}
	return NULL;
    }

    return &unit->NextCommand[(int)unit->NextCount++];
}

/**
**	Clear the saved action.
**
**	@param unit	Unit pointer, that get the saved action cleared.
**
**	If we make an new order, we must clear any saved actions.
**	Internal functions, must protect it, if needed.
*/
local void ClearSavedAction(Unit* unit)
{
    unit->SavedCommand.Action=UnitActionStill;	// clear saved action
}

#endif

/*----------------------------------------------------------------------------
--	Commands
----------------------------------------------------------------------------*/

/**
**	Stop unit.
**
**	@param unit	pointer to unit.
*/
global void CommandStopUnit(Unit* unit)
{
#ifdef NEW_ORDERS
    Order* order;

    order=GetNextOrder(unit,1);		// Flush them.
    order->Action=UnitActionStill;
    order->X=-1;
    order->Y=-1;
    order->Goal=NoUnitP;
    order->Type=NULL;
    order->Arg1=NULL;
    ReleaseOrder(&unit->SavedOrder);
    ReleaseOrder(&unit->NewOrder);
    unit->SavedOrder=unit->NewOrder=*order;
#else
    unit->NextFlush=1;
    unit->NextCount=1;
    unit->NextCommand[0].Action=UnitActionStill;
    unit->NextCommand[0].Data.Move.Goal=NoUnitP;

    unit->PendCommand=unit->NextCommand[0];

    ClearSavedAction(unit);
#endif
}

/**
**	Stand ground.
**
**	@param unit	pointer to unit.
**	@param flush	if true, flush command queue.
*/
global void CommandStandGround(Unit* unit,int flush)
{
#ifdef NEW_ORDERS
    Order* order;

    if( (order=GetNextOrder(unit,flush)) ) {
	order->Action=UnitActionStandGround;
	order->X=-1;
	order->Y=-1;
	order->Goal=NoUnitP;
	order->Type=NULL;
	order->Arg1=NULL;
    }
#else
    Command* command;

    if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    command->Action=UnitActionStandGround;
    command->Data.Move.Goal=NoUnitP;

#endif
    ClearSavedAction(unit);
}

/**
**	Follow unit to new position
**
**	@param unit	pointer to unit.
**	@param dest	unit to be followed
**	@param flush	if true, flush command queue.
*/
global void CommandFollow(Unit* unit,Unit* dest,int flush)
{
#ifdef NEW_ORDERS
    Order* order;

    if( unit->Type->Building ) {
	// FIXME: should find a better way for pending orders.
	order=&unit->NewOrder;
    } else if( !(order=GetNextOrder(unit,flush)) ) {
	return;
    }

    order->Action=UnitActionFollow;
    ResetPath(*order);
    order->Goal=dest;
    RefsDebugCheck( !dest->Refs );
    dest->Refs++;
    order->RangeX=order->RangeY=1;
    order->X=-1;
    order->Y=-1;
    order->Type=NULL;
    order->Arg1=NULL;
#else
    Command* command;

    if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    command->Action=UnitActionFollow;
    ResetPath(*command);
    command->Data.Move.Goal=dest;
    RefsDebugCheck( !dest->Refs );
    dest->Refs++;
    command->Data.Move.Range=1;
    command->Data.Move.SX=-1;
    command->Data.Move.SY=-1;

#endif
    ClearSavedAction(unit);
}

/**
**	Move unit to new position
**
**	@param unit	pointer to unit.
**	@param x	X map position to move to.
**	@param y	Y map position to move to.
**	@param flush	if true, flush command queue.
*/
global void CommandMove(Unit* unit,int x,int y,int flush)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");
#else
    Command* command;

    IfDebug(
	if( x<0 || y<0 || x>=TheMap.Width || y>=TheMap.Height ) {
	    DebugLevel0Fn("Internal movement error\n");
	    return;
	}
    );

    //
    //	Check if unit is still valid? (NETWORK!)
    //
    if( !unit->Removed ) {

	if( unit->Type->Building ) {
	    // FIXME: should find a better way for pending commands.
	    command=&unit->PendCommand;
	} else if( !(command=GetNextCommand(unit,flush)) ) {
	    return;
	}

	command->Action=UnitActionMove;
	ResetPath(*command);
	command->Data.Move.Goal=NoUnitP;
	command->Data.Move.Range=0;
	command->Data.Move.SX=unit->X;
	command->Data.Move.SY=unit->Y;
	command->Data.Move.DX=x;
	command->Data.Move.DY=y;
    }
#endif
    ClearSavedAction(unit);
}

/**
**	Repair unit
**
**	@param unit	pointer to unit.
**	@param x	X map position to repair.
**	@param y	Y map position to repair.
**	@param dest	or unit to be repaired. FIXME: not supported
**	@param flush	if true, flush command queue.
*/
global void CommandRepair(Unit* unit,int x,int y,Unit* dest,int flush)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");
#else
    Command* command;

    IfDebug(
	if( x<0 || y<0 || x>=TheMap.Width || y>=TheMap.Height ) {
	    DebugLevel0Fn("Internal movement error\n");
	    return;
	}
	// FIXME: dest until now not supported
	if( dest ) {
	    DebugLevel0Fn("not used %p\n",dest);
	}
    );


    if( unit->Type->Building ) {
	// FIXME: should find a better way for pending commands.
	command=&unit->PendCommand;
    } else if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    if ( !(dest=RepairableOnMapTile(x,y)) ) {
	// FIXME: don't work for automatic repairs.
	command->Action=UnitActionStill;
        return;
    }

    command->Action=UnitActionRepair;
    ResetPath(*command);
    command->Data.Move.Goal=dest;
    if( dest ) {
	dest->Refs++;
    }
    command->Data.Move.Range=REPAIR_RANGE;
    command->Data.Move.SX=unit->X;
    command->Data.Move.SY=unit->Y;
    command->Data.Move.DX=x;
    command->Data.Move.DY=y;

#endif
    ClearSavedAction(unit);
}

/**
**	Attack with unit at new position
**
**	@param unit	pointer to unit.
**	@param x	X map position to attack.
**	@param y	Y map position to attack.
**	@param attack	or unit to be attacked.
**	@param flush	if true, flush command queue.
*/
global void CommandAttack(Unit* unit,int x,int y,Unit* attack,int flush)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");
#else
    Command* command;

    IfDebug(
	if( x<0 || y<0 || x>=TheMap.Width || y>=TheMap.Height ) {
	    DebugLevel0Fn("Internal movement error\n");
	    return;
	}
	if( unit->Type->Vanishes ) {
	    DebugLevel0Fn("Internal error\n");
	    abort();
	}
    );

    DebugLevel3("%Zd attacks %Zd\n"
	,UnitNumber(unit),attack ? UnitNumber(attack) : 0);

    if( unit->Type->Building ) {
	// FIXME: should find a better way for pending commands.
	command=&unit->PendCommand;
    } else if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    command->Action=UnitActionAttack;
    ResetPath(*command);
    // choose goal and good attack range
    if( attack ) {
	command->Data.Move.Goal=attack;
	attack->Refs++;
	command->Data.Move.Range=unit->Stats->AttackRange;
    } else {
	command->Data.Move.Goal=NoUnitP;
	if( WallOnMap(x,y) ) {
	    command->Data.Move.Range=unit->Stats->AttackRange;
	} else {
	    command->Data.Move.Range=0;
	}
    }
    command->Data.Move.SX=unit->X;
    command->Data.Move.SY=unit->Y;
    command->Data.Move.DX=x;
    command->Data.Move.DY=y;

#endif
    ClearSavedAction(unit);
}

/**
**	Attack ground with unit.
**
**	@param unit	pointer to unit.
**	@param x	X map position to fire on.
**	@param y	Y map position to fire on.
**	@param flush	if true, flush command queue.
*/
global void CommandAttackGround(Unit* unit,int x,int y,int flush)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");
#else
    Command* command;

    IfDebug(
	if( x<0 || y<0 || x>=TheMap.Width || y>=TheMap.Height ) {
	    DebugLevel0Fn("Internal movement error\n");
	    return;
	}
	DebugCheck( unit->Type->Vanishes );
    );

    if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    command->Action=UnitActionAttackGround;
    ResetPath(*command);
    command->Data.Move.Goal=NoUnitP;
    command->Data.Move.Range=unit->Stats->AttackRange;
    command->Data.Move.SX=unit->X;
    command->Data.Move.SY=unit->Y;
    command->Data.Move.DX=x;
    command->Data.Move.DY=y;
    // FIXME: pathfinder didn't support this kind of target
    DebugLevel0("Fixme this next\n");

#endif
    ClearSavedAction(unit);
}

/**
**	Let an unit patrol from current to new position
**
**	@param unit	pointer to unit.
**	@param x	X map position to patrol between.
**	@param y	Y map position to patrol between.
**	@param flush	if true, flush command queue.
*/
global void CommandPatrolUnit(Unit* unit,int x,int y,int flush)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");
#else
    Command* command;

    IfDebug(
	if( x<0 || y<0 || x>=TheMap.Width || y>=TheMap.Height ) {
	    DebugLevel0Fn("Internal movement error\n");
	    return;
	}
    );

    if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    command->Action=UnitActionPatrol;
    ResetPath(*command);
    command->Data.Move.Goal=NoUnitP;
    command->Data.Move.Range=0;
    command->Data.Move.SX=unit->X;
    command->Data.Move.SY=unit->Y;
    command->Data.Move.DX=x;
    command->Data.Move.DY=y;

#endif
    ClearSavedAction(unit);
}

/**
**	Board a transporter with unit.
**
**	@param unit	pointer to unit.
**	@param dest	unit to be boarded.
**	@param flush	if true, flush command queue.
*/
global void CommandBoard(Unit* unit,Unit* dest,int flush)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");
#else
    Command* command;

    if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    command->Action=UnitActionBoard;
    ResetPath(*command);
    command->Data.Move.Goal=dest;
    dest->Refs++;
    command->Data.Move.Range=1;
    command->Data.Move.SX=unit->X;
    command->Data.Move.SY=unit->Y;
    command->Data.Move.DX=dest->X;
    command->Data.Move.DY=dest->Y;

#endif
    ClearSavedAction(unit);
}

/**
**	Unload a transporter.
**
**	@param unit	pointer to unit.
**	@param x	X map position to unload.
**	@param y	Y map position to unload.
**	@param what	unit to be unloaded, NoUnitP all.
**	@param flush	if true, flush command queue.
*/
global void CommandUnload(Unit* unit,int x,int y,Unit* what,int flush)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");
#else
    Command* command;

    IfDebug(
	if( x<0 || y<0 || x>=TheMap.Width || y>=TheMap.Height ) {
	    DebugLevel0Fn("Internal movement error\n");
	    return;
	}
    );

    if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    command->Action=UnitActionUnload;
    ResetPath(*command);
    command->Data.Move.Goal=what;
    if( what ) {
	what->Refs++;
    }
    command->Data.Move.Range=0;
    command->Data.Move.SX=unit->X;
    command->Data.Move.SY=unit->Y;
    command->Data.Move.DX=x;
    command->Data.Move.DY=y;

#endif
    ClearSavedAction(unit);
}

/**
**	Send a unit building
**
**	@param unit	pointer to unit.
**	@param x	X map position to build.
**	@param y	Y map position to build.
**	@param what	Unit type to build.
**	@param flush	if true, flush command queue.
*/
global void CommandBuildBuilding(Unit* unit,int x,int y
	,UnitType* what,int flush)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");
#else
    Command* command;

    if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    command->Action=UnitActionBuild;
    ResetPath(*command);
    command->Data.Move.Goal=NoUnitP;
    // FIXME: only quadratic buildings supported!!!
    if( what->ShoreBuilding ) {
	command->Data.Move.Range=what->TileWidth+1;
	// FIXME: this hack didn't work correct on map border
	command->Data.Move.DX=x ? x-1 : x;
	command->Data.Move.DY=y ? y-1 : y;
    } else {
	command->Data.Move.Range=what->TileWidth-1;
	command->Data.Move.DX=x;
	command->Data.Move.DY=y;
    }
    // FIXME: must change movement for not build goal!

    command->Data.Move.SX=unit->X;
    command->Data.Move.SY=unit->Y;

    command->Data.Build.BuildThis=what;

#endif
    ClearSavedAction(unit);
}

/**
**	Cancel the building construction.
**
**	@param unit	pointer to unit.
**	@param worker	pointer to unit.
*/
global void CommandCancelBuilding(Unit* unit,Unit* worker)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");
#else
#if 0
    if( unit->Command.Action==UnitActionBuilded ) {
	unit->NextCount=1;
	unit->NextFlush=1;

	unit->NextCommand[0].Action=UnitActionBuilded;
	unit->NextCommand[0].Data.Builded.Cancel=1;
	unit->NextCommand[0].Data.Builded.Worker=worker;

	unit->Wait=1;
	unit->Reset=1;
    }
#endif
    //
    //	Check if building is still under construction? (NETWORK!)
    //
    if( unit->Command.Action==UnitActionBuilded ) {
	unit->Command.Data.Builded.Cancel=1;
    }
#endif
    ClearSavedAction(unit);
}

/**
**	Send unit harvest
**
**	@param unit	pointer to unit.
**	@param x	X map position for harvest.
**	@param y	Y map position for harvest.
**	@param flush	if true, flush command queue.
*/
global void CommandHarvest(Unit* unit,int x,int y,int flush)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");

    unit->Data.Harvest.WoodToHarvest=CHOP_FOR_WOOD;
#else
    Command* command;

    if( unit->Type->Building ) {
	// FIXME: should find a better way for pending commands.
	command=&unit->PendCommand;
    } else if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    unit->WoodToHarvest=CHOP_FOR_WOOD;

    command->Action=UnitActionHarvest;
    ResetPath(*command);

    command->Data.Move.Goal=NoUnitP;
    command->Data.Move.Range=2;
    command->Data.Move.SX=unit->X;
    command->Data.Move.SY=unit->Y;
    command->Data.Move.DX=x-1;
    command->Data.Move.DY=y-1;

    ClearSavedAction(unit);
#endif
}

/**
**	Send unit mine gold.
**
**	@param unit	pointer to unit.
**	@param dest	destination unit.
**	@param flush	if true, flush command queue.
*/
global void CommandMineGold(Unit* unit,Unit* dest,int flush)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");
#else
    Command* command;

    if( unit->Type->Building ) {
	// FIXME: should find a better way for pending commands.
	command=&unit->PendCommand;
    } else if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    command->Action=UnitActionMineGold;
    ResetPath(*command);
    command->Data.Move.Goal=dest;
    RefsDebugCheck( !dest->Refs );
    dest->Refs++;
    command->Data.Move.Range=1;
    command->Data.Move.SX=unit->X;
    command->Data.Move.SY=unit->Y;
    command->Data.Move.DX=-1;
    command->Data.Move.DY=-1;

#endif
    ClearSavedAction(unit);
}

/**
**	Send unit haul oil.
**
**	@param unit	pointer to unit.
**	@param dest	destination unit.
**	@param flush	if true, flush command queue.
*/
global void CommandHaulOil(Unit* unit,Unit* dest,int flush)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");
#else
    Command* command;

    if( unit->Type->Building ) {
	// FIXME: should find a better way for pending commands.
	command=&unit->PendCommand;
    } else if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    command->Action=UnitActionHaulOil;
    ResetPath(*command);
    command->Data.Move.Goal=dest;
    RefsDebugCheck( !dest->Refs );
    dest->Refs++;
    command->Data.Move.Range=1;
    command->Data.Move.SX=unit->X;
    command->Data.Move.SY=unit->Y;
    command->Data.Move.DX=-1;
    command->Data.Move.DY=-1;

#endif
    ClearSavedAction(unit);
}

/**
**	Let unit returning goods.
**
**	@param unit	pointer to unit.
**	@param flush	if true, flush command queue.
*/
global void CommandReturnGoods(Unit* unit,int flush)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");
#else
    // FIXME: flush, and command que not supported!

    unit->NextCount=1;
    unit->NextFlush=1;

    unit->NextCommand[0].Action=UnitActionReturnGoods;
    ResetPath(unit->NextCommand[0]);
    unit->NextCommand[0].Data.Move.Goal=NoUnitP;
    unit->NextCommand[0].Data.Move.Range=1;
    unit->NextCommand[0].Data.Move.SX=unit->X;
    unit->NextCommand[0].Data.Move.SY=unit->Y;

#endif
    ClearSavedAction(unit);
}

/**
**	Building starts training an unit.
**
**	@param unit	pointer to unit.
**	@param type	unit type to train.
**	@param flush	if true, flush command queue.
*/
global void CommandTrainUnit(Unit* unit,UnitType* type,int flush)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");

#else
    //
    //	Check if enough resources remains? (NETWORK!)
    //
    if( !PlayerCheckFood(unit->Player,type)
	    || PlayerCheckUnitType(unit->Player,type) ) {
	return;
    }
    PlayerSubUnitType(unit->Player,type);

    //
    //	Not already training?
    //
    if( unit->Command.Action!=UnitActionTrain ) {
	Command* command;

	DebugCheck( unit->Wait>6 );

	command=unit->NextCommand;

	unit->NextCount=unit->NextFlush=1;

	if( command->Action!=UnitActionTrain ) {
	    command->Action=UnitActionTrain;
	    command->Data.Train.Ticks=command->Data.Train.Count=0;
	}

	//
	//	Training slots are all already full. (NETWORK!)
	//
	if( command->Data.Train.Count>=MAX_UNIT_TRAIN ) {
	    DebugLevel0Fn("Unit queue full!\n");
	    //PlayerAddUnitType(unit->Player,type);
	    return;
	}
	command->Data.Train.What[command->Data.Train.Count++]=type;

    } else {
	//
	//	Training slots are all already full. (NETWORK!)
	//
	if( unit->Command.Data.Train.Count>=MAX_UNIT_TRAIN ) {
	    DebugLevel0Fn("Unit queue full!\n");
	    PlayerAddUnitType(unit->Player,type);
	    return;
	}
	unit->Command.Data.Train.What[unit->Command.Data.Train.Count++]=type;

	//
	//	Update interface.
	//
	if( unit->Player==ThisPlayer && unit->Selected ) {
	    MustRedraw|=RedrawInfoPanel;
	}
    }

#endif
    ClearSavedAction(unit);
}

/**
**	Cancel the training of an unit.
**
**	@param unit	pointer to unit.
**	@param slot	slot number to cancel.
*/
global void CommandCancelTraining(Unit* unit,int slot)
{
#ifdef NEW_ORDERS
    DebugLevel0Fn("FIXME: not written\n");
#else
    int i;
    int n;

    // FIXME: over network we could cancel the wrong slot.

    //
    //	Check if unit is still training 'slot'? (NETWORK!)
    //
    n=unit->Command.Data.Train.Count;
    if( unit->Command.Action==UnitActionTrain && slot<n ) {
	PlayerAddCostsFactor(unit->Player,
		unit->Command.Data.Train.What[slot]
			->Stats[unit->Player->Player].Costs,
		CancelTrainingCostsFactor);

	if ( --n ) {
	    for( i = slot; i < n; i++ ) {
		unit->Command.Data.Train.What[i] =
			unit->Command.Data.Train.What[i+1];
	    }
	    if( !slot ) {
		unit->Command.Data.Train.Ticks=0;
	    }
	    unit->Command.Data.Train.Count=n;
	} else {
	    unit->Command.Action=UnitActionStill;
	}

	//
	//	Update interface.
	//
	if( unit->Player==ThisPlayer && unit->Selected ) {
	    UpdateButtonPanel();
	    MustRedraw|=RedrawPanels;
	}

	unit->Wait=unit->Reset=1;	// immediately start next training
    }

#endif
    ClearSavedAction(unit);
}

/**
**	Building starts upgrading to.
**
**	@param unit	pointer to unit.
**	@param type	upgrade to type
**	@param flush	if true, flush command queue.
*/
global void CommandUpgradeTo(Unit* unit,UnitType* type,int flush)
{
#ifdef NEW_ORDERS
    Order* order;

    DebugLevel0Fn("FIXME: must support order queing!!");
    order=GetNextOrder(unit,1);		// Flush them.

    order->Action=UnitActionUpgradeTo;
    order->X=-1;
    order->Y=-1;
    order->Goal=NoUnitP;
    order->Type=NULL;
    order->Arg1=type;
#else
    //
    //	Check if enough resources remains? (NETWORK!)
    //
    if( PlayerCheckUnitType(unit->Player,type) ) {
	return;
    }
    PlayerSubUnitType(unit->Player,type);

    unit->NextCount=1;
    unit->NextFlush=1;

    unit->NextCommand[0].Action=UnitActionUpgradeTo;
    unit->NextCommand[0].Data.UpgradeTo.Ticks=0;
    unit->NextCommand[0].Data.UpgradeTo.What=type;

    unit->Wait=1;			// FIXME: correct this
    unit->Reset=1;

#endif
    ClearSavedAction(unit);
}

/**
**	Cancel building upgrading to.
**
**	@param unit	pointer to unit.
*/
global void CommandCancelUpgradeTo(Unit* unit)
{
#ifdef NEW_ORDERS
    Order* order;

    DebugLevel0Fn("FIXME: must support order queing!!");
    order=GetNextOrder(unit,1);		// Flush them.

    order->Action=UnitActionStill;
    order->X=-1;
    order->Y=-1;
    order->Goal=NoUnitP;
    order->Type=NULL;
    order->Arg1=NULL;
    DebugLevel0Fn("FIXME:\n");
#else
    //
    //	Check if unit is still upgrading? (NETWORK!)
    //
    if( unit->Command.Action == UnitActionUpgradeTo ) {
		    
	PlayerAddCostsFactor(unit->Player,
		unit->Command.Data.UpgradeTo.What->Stats->Costs,
		CancelUpgradeCostsFactor);

	unit->Command.Action=UnitActionStill;

	//
	//	Update interface.
	//
	if( unit->Player==ThisPlayer && unit->Selected ) {
	    UpdateButtonPanel();
	    MustRedraw|=RedrawPanels;
	}

	unit->Wait=unit->Reset=1;	// immediately start next command.
    }

#endif
    ClearSavedAction(unit);
}

/**
**	Building starts researching.
**
**	@param unit	pointer to unit.
**	@param what	what to research.
**	@param flush	if true, flush command queue.
*/
global void CommandResearch(Unit* unit,Upgrade* what,int flush)
{
#ifdef NEW_ORDERS
    Order* order;

    DebugLevel0Fn("FIXME: must support order queing!!\n");
    order=GetNextOrder(unit,1);		// Flush them.

    order->Action=UnitActionResearch;
    order->X=-1;
    order->Y=-1;
    order->Goal=NoUnitP;
    order->Type=NULL;
    order->Arg1=what;
#else
    //
    //	Check if enough resources remains? (NETWORK!)
    //
    if( PlayerCheckCosts(unit->Player,what->Costs) ) {
	return;
    }
    PlayerSubCosts(unit->Player,what->Costs);

    unit->NextCount=1;
    unit->NextFlush=1;

    DebugLevel0Fn("FIXME: must support command queing!!\n");
    unit->NextCommand[0].Action=UnitActionResearch;
    unit->NextCommand[0].Data.Research.Ticks=0;
    unit->NextCommand[0].Data.Research.What=what;

    unit->Wait=1;			// FIXME: correct this
    unit->Reset=1;

#endif
    ClearSavedAction(unit);
}

/**
**	Cancel Building researching.
**
**	@param unit	pointer to unit.
*/
global void CommandCancelResearch(Unit* unit)
{
#ifdef NEW_ORDERS
    Order* order;

    DebugLevel0Fn("FIXME: must support order queing!!");
    order=GetNextOrder(unit,1);		// Flush them.

    order->Action=UnitActionStill;
    order->X=-1;
    order->Y=-1;
    order->Goal=NoUnitP;
    order->Type=NULL;
    order->Arg1=NULL;
    DebugLevel0Fn("FIXME:\n");
#else
    //
    //	Check if unit is still researching? (NETWORK!)
    //
    if( unit->Command.Action == UnitActionResearch ) {
	PlayerAddCostsFactor(unit->Player,
		unit->Command.Data.UpgradeTo.What->Stats->Costs,
		CancelResearchCostsFactor);

	unit->Command.Action=UnitActionStill;

	//
	//	Update interface.
	//
	if( unit->Player==ThisPlayer && unit->Selected ) {
	    UpdateButtonPanel();
	    MustRedraw|=RedrawPanels;
	}

	unit->Wait=unit->Reset=1;	// immediately start next command.
    }
#endif

    ClearSavedAction(unit);
}

/**
**	Demolish at position
**
**	@param unit	pointer to unit.
**	@param x	X map position to move to.
**	@param y	Y map position to move to.
**	@param dest	if != NULL, pointer to unit to destroy.
**	@param flush	if true, flush command queue.
*/
global void CommandDemolish(Unit* unit,int x,int y,Unit* dest,int flush)
{
#ifdef NEW_ORDERS
    Order* order;

    IfDebug(
	if( x<0 || y<0 || x>=TheMap.Width || y>=TheMap.Height ) {
	    DebugLevel0Fn("Internal movement error\n");
	    return;
	}
    );

    if( unit->Type->Building ) {
	// FIXME: should find a better way for pending orders.
	order=&unit->NewOrder;
    } else if( !(order=GetNextOrder(unit,flush)) ) {
	return;
    }

    order->Action=UnitActionDemolish;
    ResetPath(*order);
    // choose goal and good attack range
    if( dest ) {
	order->Goal=dest;
	dest->Refs++;
	order->RangeX=order->RangeY=1;
    } else {
	order->Goal=NoUnitP;
	if( WallOnMap(x,y) || ForestOnMap(x,y) || RockOnMap(x,y) ) {
	    order->RangeX=order->RangeY=1;
	} else {
	    order->RangeX=order->RangeY=0;
	}
    }
    order->X=x;
    order->Y=y;
#else
    Command* command;

    IfDebug(
	if( x<0 || y<0 || x>=TheMap.Width || y>=TheMap.Height ) {
	    DebugLevel0Fn("Internal movement error\n");
	    return;
	}
    );

    if( unit->Type->Building ) {
	// FIXME: should find a better way for pending commands.
	command=&unit->PendCommand;
    } else if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    command->Action=UnitActionDemolish;
    ResetPath(*command);
    // choose goal and good attack range
    if( dest ) {
	command->Data.Move.Goal=dest;
	dest->Refs++;
	command->Data.Move.Range=1;
    } else {
	command->Data.Move.Goal=NoUnitP;
	if( WallOnMap(x,y) ) {
	    command->Data.Move.Range=1;
	} else {
	    command->Data.Move.Range=0;
	}
    }
    command->Data.Move.SX=unit->X;
    command->Data.Move.SY=unit->Y;
    command->Data.Move.DX=x;
    command->Data.Move.DY=y;

#endif
    ClearSavedAction(unit);
}

/**
**	Cast a spell at position or unit.
**
**	@param unit	pointer to unit.
**	@param x	X map position to spell cast on.
**	@param y	Y map position to spell cast on.
**	@param dest	Spell cast on unit (if exist).
**	@param spellid  Spell type id.
**	@param flush	if true, flush command queue.
*/
global void CommandSpellCast(Unit* unit,int x,int y,Unit* dest,int spellid
	,int flush)
{
#ifdef NEW_ORDERS
    Order* order;
    SpellType* spell;

    IfDebug(
	if( x<0 || y<0 || x>=TheMap.Width || y>=TheMap.Height ) {
	    DebugLevel0("Internal movement error\n");
	    return;
	}
	if( unit->Type->Vanishes ) {
	    DebugLevel0("Internal error\n");
	    abort();
	}
    );

    DebugLevel3Fn(": %Zd spell-casts on %Zd\n"
	,UnitNumber(unit),dest ? UnitNumber(dest) : 0);

    if( unit->Type->Building ) {
	// FIXME: should find a better way for pending commands.
	order=&unit->NewOrder;
    } else if( !(order=GetNextOrder(unit,flush)) ) {
	return;
    }

    spell=SpellTypeById( spellid );

    order->Action=UnitActionSpellCast;
    ResetPath(*order);
    order->RangeX=order->RangeY=spell->Range;
    order->Goal=dest;
    if (dest) {
	dest->Refs++;
    }
    order->X=x;
    order->Y=y;
    order->Arg1=spell;
#else
    Command* command;
    const SpellType* spell;

    IfDebug(
	if( x<0 || y<0 || x>=TheMap.Width || y>=TheMap.Height ) {
	    DebugLevel0("Internal movement error\n");
	    return;
	}
	if( unit->Type->Vanishes ) {
	    DebugLevel0("Internal error\n");
	    abort();
	}
    );

    DebugLevel3Fn(": %Zd spell-casts on %Zd\n"
	,UnitNumber(unit),dest ? UnitNumber(dest) : 0);

    if( unit->Type->Building ) {
	// FIXME: should find a better way for pending commands.
	command=&unit->PendCommand;
    } else if( !(command=GetNextCommand(unit,flush)) ) {
	return;
    }

    spell = SpellTypeById( spellid );

    command->Action=UnitActionSpellCast;
    ResetPath(*command);

    if (dest)
      dest->Refs++;

    command->Data.Move.Goal=dest;
    command->Data.Move.Range=spell->Range;
    command->Data.Move.SX=unit->X;
    command->Data.Move.SY=unit->Y;
    command->Data.Move.DX=x;
    command->Data.Move.DY=y;
    command->Data.Move.SpellId = spellid;

#endif
    ClearSavedAction(unit);
}

//@}
