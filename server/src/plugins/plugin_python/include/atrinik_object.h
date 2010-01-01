/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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

#ifndef ATRINIK_OBJECT_H
#define ATRINIK_OBJECT_H

/* First the required header files - only the CF module interface and Python */
#include <Python.h>
#include <plugin.h>
#include <plugin_python.h>

/* Atrinik_Object methods  */
static PyObject* Atrinik_Object_CheckTrigger(Atrinik_Object* self, PyObject* args);
static PyObject* Atrinik_Object_SetSaveBed(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_SwapApartments(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_SetSkill(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_GetSkill(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_ActivateRune(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_CastAbility(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_GetGod(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_SetGod(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_TeleportTo(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_InsertInside(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_Apply(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_PickUp(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_Drop(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_Deposit(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_Withdraw(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_Communicate(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_Say(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_SayTo(Atrinik_Object *whoptr, PyObject* args);
static PyObject *Atrinik_Object_SetGender(Atrinik_Object *self, PyObject *args);
static PyObject *Atrinik_Object_GetGender(Atrinik_Object *self, PyObject *args);
static PyObject *Atrinik_Object_SetRank(Atrinik_Object *self, PyObject *args);
static PyObject *Atrinik_Object_GetRank(Atrinik_Object *self, PyObject *args);
static PyObject* Atrinik_Object_SetAlignment(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_GetAlignmentForce(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_SetGuildForce(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_GetGuildForce(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_Fix(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_Kill(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_DoKnowSpell(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_AcquireSpell(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_DoKnowSkill(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_AcquireSkill(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_FindMarkedObject(Atrinik_Object *whoptr, PyObject* args);
static PyObject* Atrinik_Object_CreatePlayerForce(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_GetQuestObject(Atrinik_Object *whoptr, PyObject* args);
static PyObject *Atrinik_Object_StartQuest(Atrinik_Object *whoptr, PyObject *args);
static PyObject* Atrinik_Object_CreatePlayerInfo(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_GetPlayerInfo(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_GetNextPlayerInfo(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_CheckInvisibleInside(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_CreateInvisibleInside(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_CreateObjectInside(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_CheckInventory(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_Remove(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_SetPosition(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_IdentifyItem(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_Write(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_IsOfType(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_GetIP(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_GetArchName(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_ShowCost(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_GetItemCost(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_GetMoney(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_Save(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_PayForItem(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_PayAmount(Atrinik_Object *self, PyObject* args);
static PyObject* Atrinik_Object_SendCustomCommand(Atrinik_Object* self, PyObject* args);
static PyObject* Atrinik_Object_Clone(Atrinik_Object* self, PyObject* args);
static PyObject* Atrinik_Object_GetUnmodifiedAttribute(Atrinik_Object* self, PyObject* args);
static PyObject *Atrinik_Object_GetSaveBed(Atrinik_Object *whoptr, PyObject *args);
static PyObject *Atrinik_Object_GetObKeyValue(Atrinik_Object *whoptr, PyObject *args);
static PyObject *Atrinik_Object_SetObKeyValue(Atrinik_Object *whoptr, PyObject *args);
static PyObject *Atrinik_Object_GetEquipment(Atrinik_Object *whoptr, PyObject *args);
static PyObject *Atrinik_Object_GetName(Atrinik_Object *whatptr, PyObject *args);
static PyObject *Atrinik_Object_GetParty(Atrinik_Object *whatptr, PyObject *args);

/* Atrinik_Object SetGeters */
static int Object_SetFlag(Atrinik_Object* self, PyObject* val, int flagnp);
static PyObject* Object_GetFlag(Atrinik_Object* self, int flagno);
static int Object_SetAttribute(Atrinik_Object* self, PyObject *value, int fieldid);
static PyObject* Object_GetAttribute(Atrinik_Object* self, int fieldid);

/*****************************************************************************/
/* Crossfire object type part.                                               */
/* Using a custom type for CF Objects allows us to handle more errors, and   */
/* avoid server crashes due to buggy scripts                                 */
/* In the future even add methods to it?                                     */
/*****************************************************************************/

/* Object creator (not really needed, since the generic creator does the same thing...) */
static PyObject *
Atrinik_Object_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

/* str() function to get a string representation of this object */
static PyObject *
Atrinik_Object_str(Atrinik_Object *self);

/* Object deallocator (needed) */
static void
Atrinik_Object_dealloc(Atrinik_Object* self);

#endif
