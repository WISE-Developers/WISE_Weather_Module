/**
 * WISE_Weather_Module: WeatherCOM.h
 * Copyright (C) 2023  WISE
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if defined(_MSC_VER) || defined(__CYGWIN__)
#  ifdef WEATHER_EXPORTS
#    ifdef __GNUC__
#      define WEATHERCOM_API __attribute__((dllexport))
#      define NO_THROW __attribute__((nothrow))
#    else
#      define WEATHERCOM_API __declspec(dllexport)
#      define NO_THROW __declspec(nothrow)
#    endif
#  else
#    ifdef __GNUC__
#      define WEATHERCOM_API __attribute__((dllimport))
#      define NO_THROW __attribute__((nothrow))
#    else
#      define WEATHERCOM_API __declspec(dllimport)
#      define NO_THROW __declspec(nothrow)
#    endif
#  endif
#else
#  define NO_THROW __attribute__((nothrow))
#  if __GNUC__ >= 4
#    define WEATHERCOM_API __attribute__((visibility("default")))
#  else
#    define WEATHERCOM_API
#  endif
#endif
