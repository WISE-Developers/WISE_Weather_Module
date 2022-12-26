/***********************************************************************
 * REDapp - CWFGM_WEATHERSTREAM_IMPORT.java
 * Copyright (C) 2015-2019 The REDapp Development Team
 * Homepage: http://redapp.org
 * 
 * REDapp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * REDapp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with REDapp. If not see <http://www.gnu.org/licenses/>. 
 **********************************************************************/

package ca.wise.weather;

public abstract class WEATHERSTREAM_IMPORT {
	public static final long PURGE				= 0x0001;
	public static final long SUPPORT_APPEND		= 0x0002;
	public static final long SUPPORT_OVERWRITE	= 0x0004;
	public static final long INVALID_FAILURE    = 0x0100;
    public static final long INVALID_ALLOW      = 0x0200;
    public static final long INVALID_FIX        = 0x0400;
}
