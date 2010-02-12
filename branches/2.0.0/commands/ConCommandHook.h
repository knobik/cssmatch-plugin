/* 
 * Copyright 2008-2010 Nicolas Maingot
 * 
 * This file is part of CSSMatch.
 * 
 * CSSMatch is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * CSSMatch is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with CSSMatch; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Portions of this code are also Copyright � 1996-2005 Valve Corporation, All rights reserved
 */

#ifndef __CONCOMMAND_HOOK_H__
#define __CONCOMMAND_HOOK_H__

#include "convar.h"

#include <string>

class IVEngineServer;

namespace cssmatch
{
	/** Callback for a hook ConCommand <br>
	 * The first parameter is the user who used the hooked command <br>
	 * The second is the IVEngineServer instance used to access the command arguments
	 */
	typedef bool (* HookCallback)(int,IVEngineServer *);

	/** Hook a command at runtime 
	 * Note that this object calls ConCommand::Init(), <br>
	 * don't construct it before invoking ConCommandBaseMgr::OneTimeInit
	 */
	class ConCommandHook : public ConCommand
	{
	protected:
		/** Pointer to the hooked command */
		ConCommand * hooked;

		/** Callback for this hook */
		HookCallback callback;
	public:
		/** Similar to the ConCommand's constructor
		 * @param name The name must be dynamically allocated with new !
		 * @param hookCallback The function to call when the hooked command is used
		 */
		ConCommandHook(const char * name, HookCallback hookCallback);

		/**
		 * @see ConCommand
		 */
		void Init();

		/** 
		 * @see ConCommand
		 */
		void Dispatch();
	};
}

#endif // __CONCOMMAND_HOOK_H__