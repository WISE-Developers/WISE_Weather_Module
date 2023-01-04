/***********************************************************************
 * REDapp - CWFGM_WEATHER_OPTION.java
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

public abstract class WEATHER_OPTION {
	/**
	 * The temperature alpha attribute. This attribute must be a {@link java.lang.Double} value.
	 */
	public static final int TEMP_ALPHA				= 10500;
	/**
	 * The temperature beta attribute. This attribute must be a {@link java.lang.Double} value.
	 */
	public static final int TEMP_BETA				= 10501;
	/**
	 * The temperature gamma attribute. This attribute must be a {@link java.lang.Double} value.
	 */
	public static final int TEMP_GAMMA				= 10502;
	/**
	 * The wind alpha attribute. This attribute must be a {@link java.lang.Double} value.
	 */
	public static final int WIND_ALPHA				= 10503;
	/**
	 * The wind beta attribute. This attribute must be a {@link java.lang.Double} value.
	 */
	public static final int WIND_BETA				= 10504;
	/**
	 * The wind gamma attribute. This attribute must be a {@link java.lang.Double} value.
	 */
	public static final int WIND_GAMMA				= 10505;
	/**
	 * The initial FFMC attribute. This attribute must be a {@link java.lang.Double} value.
	 */
	public static final int INITIAL_FFMC			= 10510;
	/**
	 * The initial DC attribute. This attribute must be a {@link java.lang.Double} value.
	 */
	public static final int INITIAL_DC				= 10511;
	/**
	 * The initial DMC attribute. This attribute must be a {@link java.lang.Double} value.
	 */
	public static final int INITIAL_DMC				= 10512;
	/**
	 * The initial BUI attribute. This attribute must be a {@link java.lang.Double} value.
	 */
	public static final int INITIAL_BUI				= 10513;
	/**
	 * The initial hourly FFMC time attribute. This attribute must be a {@link java.lang.Long} value.
	 */
	public static final int INITIAL_HFFMCTIME		= 10514;
	/**
	 * The initial hourly FFMC attribute. This attribute must be a {@link java.lang.Double} value.
	 */
	public static final int INITIAL_HFFMC			= 10515;
	/**
	 * The initial rain attribute. This attribute must be a {@link java.lang.Double} value.
	 */
	public static final int INITIAL_RAIN			= 10516;
	/**
	 * The start time attribute. This attribute must be a {@link java.lang.Long} value.
	 */
	public static final int START_TIME				= 10517;
	/**
	 * The end time attribute. This attribute must be a {@link java.lang.Long} value.
	 */
	public static final int END_TIME				= 10518;
	/**
	 * Use the Van Wagner equations for FFMC. This attribute must be a {@link java.lang.Boolean} value.
	 */
	public static final int FFMC_VANWAGNER			= 10540;
	/**
	 * Use the equilibrium equations for FFMC. This attribute must be a {@link java.lang.Boolean} value.
	 */
	public static final int FFMC_EQUILIBRIUM		= 10541;
	/**
	 * Use the Lawson equations for FFMC. This attribute must be a {@link java.lang.Boolean} value.
	 */
	public static final int FFMC_LAWSON				= 10542;
	/**
	 * Use the specified FWI value. This attribute must be a {@link java.lang.Boolean} value.
	 */
	public static final int FWI_USE_SPECIFIED		= 10544;
	/**
	 * Was an FWI value specified. This attribute must be a {@link java.lang.Boolean} value.
	 */
	public static final int FWI_ANY_SPECIFIED		= 10545;
	/**
	 * Is there an origin file. This attribute must be a {@link java.lang.Boolean} value.
	 */
	public static final int ORIGIN_FILE				= 10550;
	/**
	 * Is the sunrise missing. This attribute must be a {@link java.lang.Boolean} value.
	 */
	public static final int WARNONSUNRISE			= 10572;
	/**
	 * Is the sunset missing. This attribute must be a {@link java.lang.Boolean} value.
	 */
	public static final int WARNONSUNSET			= 10573;
}
