cmake_minimum_required (VERSION 3.13)
if (MSVC)
set (CMAKE_SYSTEM_VERSION 10.0.22000.0)
endif (MSVC)
project (Weather VERSION ${PROMETHEUS_VERSION} DESCRIPTION "Weather Library")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE ON)

SET(LOWLEVEL_INCLUDE_DIR "error" CACHE STRING "The path to the includes from HSS LowLevel")
SET(WTIME_INCLUDE_DIR "error" CACHE STRING "The path to the WTime libraries includes")
SET(MULTITHREAD_INCLUDE_DIR "error" CACHE STRING "The path to the includes from HSS Multithread")
SET(MATH_INCLUDE_DIR "error" CACHE STRING "The path to HSS_Math include files")
SET(GEOGRAPHY_INCLUDE_DIR "error" CACHE STRING "The path to HSS_Math include files")
SET(THIRD_PARTY_INCLUDE_DIR "error" CACHE STRING "The path to third party include files")
SET(BOOST_INCLUDE_DIR "error" CACHE STRING "The path to boost include files")
SET(ERROR_CALC_INCLUDE_DIR "error" CACHE STRING "The path to the error calc include files")
SET(FWI_INCLUDE_DIR "error" CACHE STRING "The path to the FWI include files")
SET(FIREENGINE_INCLUDE_DIR "error" CACHE STRING "The path to the fire engine include files")
SET(FUEL_INCLUDE_DIR "error" CACHE STRING "The path to the fuel include files")
SET(GRID_INCLUDE_DIR "error" CACHE STRING "The path to the grid include files")
SET(REDAPP_INCLUDE_DIR "error" CACHE STRING "The path to the REDappWrapper include files")
SET(GDAL_INCLUDE_DIR "error" CACHE STRING "The path to the GDAL include files")
SET(GSL_INCLUDE_DIR "error" CACHE STRING "The path to the GSL include files")
SET(PROTOBUF_INCLUDE_DIR "error" CACHE STRING "The path to the protobuf include files")

find_library(FOUND_MULTITHREAD_LIBRARY_PATH NAMES Multithread REQUIRED PATHS ${LOCAL_LIBRARY_DIR})
find_library(FOUND_LOWLEVEL_LIBRARY_PATH NAMES LowLevel REQUIRED PATHS ${LOCAL_LIBRARY_DIR})
find_library(FOUND_WTIME_LIBRARY_PATH NAMES WTime REQUIRED PATHS ${LOCAL_LIBRARY_DIR})
find_library(FOUND_MATH_LIBRARY_PATH NAMES hssmath REQUIRED PATHS ${LOCAL_LIBRARY_DIR})
find_library(FOUND_GEOGRAPHY_LIBRARY_PATH NAMES hssgeography REQUIRED PATHS ${LOCAL_LIBRARY_DIR})
find_library(FOUND_PROTOBUF_LIBRARY_PATH NAMES protobuf libprotobuf REQUIRED PATHS ${PROTOBUF_LIBRARY_DIR})
find_library(FOUND_ERROR_CALC_LIBRARY_PATH NAMES ErrorCalc REQUIRED PATHS ${LOCAL_LIBRARY_DIR})
find_library(FOUND_FWI_LIBRARY_PATH NAMES fwi REQUIRED PATHS ${LOCAL_LIBRARY_DIR})
find_library(FOUND_FUEL_LIBRARY_PATH NAMES fuel REQUIRED PATHS ${LOCAL_LIBRARY_DIR})
find_library(FOUND_GRID_LIBRARY_PATH NAMES grid REQUIRED PATHS ${LOCAL_LIBRARY_DIR})
find_library(FOUND_REDAPP_LIBRARY_PATH NAMES REDappWrapper REQUIRED PATHS ${LOCAL_LIBRARY_DIR})
find_library(FOUND_GDAL_LIBRARY_PATH NAMES gdal gdal_i REQUIRED PATHS ${GDAL_LIBRARY_DIR})

if (MSVC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -DPROTOBUF_USE_DLLS -DBOOST_ALL_DYN_LINK -D_CRT_SECURE_NO_WARNINGS /Zc:__cplusplus")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD -D_AFXDLL")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd -D_AFXDLL /W4")

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost COMPONENTS atomic system iostreams REQUIRED PATHS ${BOOST_LIBRARY_DIR})
else ()
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")

find_package(Boost COMPONENTS system iostreams REQUIRED PATHS ${BOOST_LIBRARY_DIR})
endif (MSVC)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_NO_MFC")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -DDEBUG")

find_package(OpenMP REQUIRED)
if (OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif ()

add_library(weather SHARED
    cpp/cwfgmWeatherStation.pb.cc
    cpp/cwfgmWeatherStream.pb.cc
    cpp/dailyConditions.pb.cc
    cpp/weatherGridFilter.pb.cc
    cpp/weatherStream.pb.cc
    cpp/windGrid.pb.cc
    cpp/CWFGM_WeatherGrid.cpp
    cpp/CWFGM_WeatherGridFilter.cpp
    cpp/CWFGM_WeatherGridFilter.Serialize.cpp
    cpp/CWFGM_WeatherStation.cpp
    cpp/CWFGM_WeatherStation.Serialize.cpp
    cpp/CWFGM_WeatherStream.cpp
    cpp/CWFGM_WeatherStream.Serialize.cpp
    cpp/CWFGM_WindDirectionGrid.cpp
    cpp/CWFGM_WindDirectionGrid.Serialize.cpp
    cpp/CWFGM_WindSpeedGrid.cpp
    cpp/CWFGM_WindSpeedGrid.Serialize.cpp
    cpp/DailyWeather.cpp
    cpp/DayCondition.cpp
    cpp/WeatherCache.cpp
    cpp/WeatherStream.cpp
    cpp/WeatherUtilities.cpp
)

target_include_directories(weather
    PUBLIC ${MATH_INCLUDE_DIR}
    PUBLIC ${FWI_INCLUDE_DIR}
    PUBLIC ${LOWLEVEL_INCLUDE_DIR}
    PUBLIC ${MULTITHREAD_INCLUDE_DIR}
    PUBLIC ${WTIME_INCLUDE_DIR}
    PUBLIC ${PROTOBUF_INCLUDE_DIR}
    PUBLIC ${FUEL_INCLUDE_DIR}
    PUBLIC ${GEOGRAPHY_INCLUDE_DIR}
    PUBLIC ${ERROR_CALC_INCLUDE_DIR}
    PUBLIC ${FIREENGINE_INCLUDE_DIR}
    PUBLIC ${GDAL_INCLUDE_DIR}
    PUBLIC ${GRID_INCLUDE_DIR}
    PUBLIC ${REDAPP_INCLUDE_DIR}
    PUBLIC ${THIRD_PARTY_INCLUDE_DIR}
    PUBLIC ${BOOST_INCLUDE_DIR}
    PUBLIC ${GSL_INCLUDE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(weather PROPERTIES VERSION ${CMAKE_PROJECT_VERSION})
set_target_properties(weather PROPERTIES SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR})
set_target_properties(weather PROPERTIES DEFINE_SYMBOL "WEATHER_EXPORTS")

set_target_properties(weather PROPERTIES
    PUBLIC_HEADER include/CWFGM_WeatherGrid.h
    PUBLIC_HEADER include/CWFGM_WeatherGridFilter.h
    PUBLIC_HEADER include/CWFGM_WeatherStation.h
    PUBLIC_HEADER include/CWFGM_WeatherStream.h
    PUBLIC_HEADER include/CWFGM_WindDirectionGrid.h
    PUBLIC_HEADER include/CWFGM_WindSpeedGrid.h
    PUBLIC_HEADER include/cwfgmWeatherStation.pb.h
    PUBLIC_HEADER include/cwfgmWeatherStream.pb.h
    PUBLIC_HEADER include/DailyCondition.h
    PUBLIC_HEADER include/dailyConditions.pb.h
    PUBLIC_HEADER include/DailyWeather.h
    PUBLIC_HEADER include/DayCondition.h
    PUBLIC_HEADER include/WeatherCom_ext.h
    PUBLIC_HEADER include/WeatherCOM.h
    PUBLIC_HEADER include/WeatherCondition.h
    PUBLIC_HEADER include/weatherGridFilter.pb.h
    PUBLIC_HEADER include/WeatherStream.h
    PUBLIC_HEADER include/weatherStream.pb.h
    PUBLIC_HEADER include/WeatherUtilities.h
    PUBLIC_HEADER include/WindGrid.h
    PUBLIC_HEADER include/windGrid.pb.h
)

target_link_libraries(weather ${FOUND_GDAL_LIBRARY_PATH} ${Boost_LIBRARIES} ${FOUND_PROTOBUF_LIBRARY_PATH})
target_link_libraries(weather ${FOUND_FWI_LIBRARY_PATH} ${FOUND_FUEL_LIBRARY_PATH} ${FOUND_GRID_LIBRARY_PATH} ${FOUND_REDAPP_LIBRARY_PATH})
target_link_libraries(weather ${FOUND_WTIME_LIBRARY_PATH} ${FOUND_LOWLEVEL_LIBRARY_PATH} ${FOUND_MULTITHREAD_LIBRARY_PATH} ${FOUND_ERROR_CALC_LIBRARY_PATH} ${FOUND_MATH_LIBRARY_PATH} ${FOUND_GEOGRAPHY_LIBRARY_PATH})
if (MSVC)
else ()
target_link_libraries(weather -lstdc++fs)
endif (MSVC)
