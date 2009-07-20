/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 2 of the License, or     *
* (at your option) any later version.                                   *
*                                                                       *
* This program is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
* GNU General Public License for more details.                          *
*                                                                       *
* You should have received a copy of the GNU General Public License     *
* along with this program; if not, write to the Free Software           *
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
*                                                                       *
* The author can be reached at admin@atrinik.org                        *
************************************************************************/

#ifndef PLUGPROTO_H_
#define PLUGPROTO_H_

f_plugin HookList[NR_OF_HOOKS] =
{
    NULL,
    CFWLog,
    CFWNewInfoMap,
    CFWSpringTrap,
    CFWCastSpell,
    CFWCmdRSkill,
    CFWBecomeFollower,
    CFWPickup,
    CFWGetMapObject,
    CFWESRVSendItem,
    CFWFindPlayer,
    CFWManualApply,
    CFWCmdDrop,
    CFWCmdTake,
    CFWCmdTitle,
    CFWTransferObject,
    CFWKillObject,
    CFWDoLearnSpell,
    CFWGetSpellNr,
    CFWCheckSpellKnown,
    CFWESRVSendInventory,
    CFWCreateArtifact,
    CFWGetArchetype,
    CFWUpdateSpeed,
    CFWUpdateObject,
    CFWFindAnimation,
    CFWGetArchetypeByObjectName,
    CFWInsertObjectInMap,
    CFWReadyMapName,
    CFWAddExp,
    CFWDetermineGod,
    CFWFindGod,
    RegisterGlobalEvent,
    UnregisterGlobalEvent,
    CFWDumpObject,
    CFWLoadObject,
    CFWRemoveObject,
    CFWAddString,
    CFWFreeString,
    CFWAddRefcount,
    CFWGetFirstMap,
    CFWGetFirstPlayer,
    CFWGetFirstArchetype,
    CFWQueryCost,
    CFWQueryMoney,
    CFWPayForItem,
    CFWPayForAmount,
    CFWNewDrawInfo,
    CFWSendCustomCommand,
    CFWCFTimerCreate,
    CFWCFTimerDestroy,
    CFWMovePlayer,
    CFWMoveObject,
    CFWSetAnimation,
    CFWCommunicate,
    CFWFindBestObjectMatch,
    CFWApplyBelow,
    CFWFreeObject,
    CFWObjectCreateClone,
    CFWTeleportObject,
    CFWDoLearnSkill,
    CFWFindMarkedObject,
    CFWIdentifyObject,
    CFWGetSkillNr,
    CFWCheckSkillKnown,
	CFWNewInfoMapExcept,
    CFWInsertObjectInObject,
	CFWFixPlayer,
	CFWPlaySoundMap,
    CFWOutOfMap,
    CFWCreateObject,
    CFWShowCost,
    CFWDeposit,
    CFWWithdraw,
	CFWSwapApartments,
	CFWPlayerExists
};

#endif /*PLUGPROTO_H_*/
