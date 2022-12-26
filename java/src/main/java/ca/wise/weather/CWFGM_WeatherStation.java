/***********************************************************************
 * REDapp - CWFGM_WeatherStation.java
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

import java.util.LinkedList;
import java.util.ListIterator;

import ca.wise.grid.GRID_ATTRIBUTE;
import ca.hss.annotations.Source;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;

/**
 * This object maintains a collection of weather streams.
 *  
 */
@Source(sourceFile = "CWFGM_WeatherStation.cpp", project = "WeatherCOM")
public class CWFGM_WeatherStation implements Serializable, Cloneable {
	private static final long serialVersionUID = 4L;
	protected boolean m_bRequiresSave = false;
	
	protected double m_latitude;
	protected double m_longitude;
	protected double m_elevation;
	protected Object m_userData;
	protected String m_loadWarning;
	protected LinkedList<CWFGM_WeatherStream> m_streamList;
	
	public CWFGM_WeatherStation() {
		m_userData = null;
		m_latitude = 0.0;
		m_longitude = 0.0;
		m_elevation = -9999.0;
		m_streamList = new LinkedList<CWFGM_WeatherStream>();
	}
	
	/**
	 * Creates a new weather station with all the same properties of the object being called,
	 * returns a handle to the new object in 'newWeatherStation'.  Any weather streams associated
	 * with this object are also duplicated for the new station.
	 * @return A clone of the current weather station.
	 */
	@Override
	public CWFGM_WeatherStation clone() throws CloneNotSupportedException {
		CWFGM_WeatherStation retVal = (CWFGM_WeatherStation)super.clone();
		retVal.m_latitude = m_latitude;
		retVal.m_longitude = m_longitude;
		retVal.m_elevation = m_elevation;
		retVal.m_userData = m_userData;
		retVal.m_bRequiresSave = true;
		ListIterator<CWFGM_WeatherStream> itr = m_streamList.listIterator();
		while (itr.hasNext()) {
			CWFGM_WeatherStream s = itr.next();
			s.setWeatherStation(0, null);
			retVal.m_streamList.addLast(s);
		}
		return retVal;
	}
	
	/**
	 * Returns the number of streams associated with this weather station.
	 * Any number of streams can be associated with a given weather station and be assigned different
	 * values, options, and time ranges. However, only one stream from a station can be associated
	 * with a scenario at a given time.
	 * @return Number of streams.
	 */
	public int getStreamCount() {
		return m_streamList.size();
	}

	/**
	 * Adds a weather stream to this weather station.  This stream may be in any state (initialized, newly created, or containing data).
	 * Any calculated data in the weather stream will be marked invalid to use the location and time zone of this weather station.
	 * @param stream A weather stream object.
	 */
	public void addStream(CWFGM_WeatherStream stream) {
		addStream(stream, -1);
	}
	
	/**
	 * Adds a weather stream to this weather station.  This stream may be in any state (initialized, newly created, or containing data).
	 * Any calculated data in the weather stream will be marked invalid to use the location and time zone of this weather station.
	 * @param stream A weather stream object.
	 * @param index Index (0-based) for where to insert the stream into the set.
	 */
	public void addStream(CWFGM_WeatherStream stream, int index) {
		if (index < 0 || index >= m_streamList.size())
			m_streamList.addLast(stream);
		else
			m_streamList.add(index, stream);
		m_bRequiresSave = true;
	}
	
	/**
	 * Removes an association between a weather stream and this weather station.
	 * @param stream A weather stream object.
	 */
	public void removeStream(CWFGM_WeatherStream stream) {
		for (int i = 0; i < m_streamList.size(); i++) {
			CWFGM_WeatherStream s = m_streamList.get(i);
			if (s.equals(stream) || s == stream) {
				m_streamList.remove(i);
				m_bRequiresSave = true;
				break;
			}
		}
	}
	
	/**
	 * Given an index value, returns a specific stream associated with this station.
	 * @param index Index to a weather stream.
	 * @return Object to contain the specific requested weather stream.
	 */
	public CWFGM_WeatherStream streamAtIndex(int index) {
		if (index < 0 || index >= m_streamList.size())
			return null;
		return m_streamList.get(index);
	}
	
	/**
	 * Returns the index of 'stream' in this station's set of streams.
	 * @param stream A weather stream object.
	 * @return The index of stream.
	 */
	public int indexOfStream(CWFGM_WeatherStream stream) {
		int i = 0;
		ListIterator<CWFGM_WeatherStream> itr = m_streamList.listIterator();
		while (itr.hasNext()) {
			CWFGM_WeatherStream s = itr.next();
			if (s.equals(stream) || s == stream) {
				return i;
			}
			i++;
		}
		return -1;
	}
	
	/**
	 * Gets the value of an "option" and saves it in the "value" variable provided.
	 * @param option The option that you want the value of (Longitude, Latitude, Elevation or load warning).
	 * @return The requested attribute.
	 * @throws IllegalArgumentException Thrown if the option is not longitude, latitude, elevation or load warning.
	 */
	public Object getAttribute(int option) throws IllegalArgumentException {
		switch (option) {
		case GRID_ATTRIBUTE.LATITUDE:
			Double lat = m_latitude;
			return lat;
		case GRID_ATTRIBUTE.LONGITUDE:
			Double lng = m_longitude;
			return lng;
		case GRID_ATTRIBUTE.DEFAULT_ELEVATION:
			Double elev = m_elevation;
			return elev;
		case GRID_ATTRIBUTE.LOAD_WARNING:
			String warn = m_loadWarning;
			return warn;
		default:
			throw new IllegalArgumentException();
		}
	}
	
	/**
	 * Sets the value of an "option" to the value of the "value" variable provided.
	 * @param option The option that you want the value of (Longitude, Latitude or Default Elevation).
	 * @param var The new value for the "option".
	 * @throws IllegalArgumentException Thrown if the option is not longitude, latitude or elevation.
	 */
	public void setAttribute(int option, Object var) throws IllegalArgumentException {
		switch (option) {
		case GRID_ATTRIBUTE.LATITUDE:
			if (var instanceof Double) {
				m_latitude = ((Double)var).doubleValue();
				m_bRequiresSave = true;
			}
			break;
		case GRID_ATTRIBUTE.LONGITUDE:
			if (var instanceof Double) {
				m_longitude = ((Double)var).doubleValue();
				m_bRequiresSave = true;
			}
			break;
		case GRID_ATTRIBUTE.DEFAULT_ELEVATION:
			if (var instanceof Double) {
				m_elevation = ((Double)var).doubleValue();
				resetStreams();
				m_bRequiresSave = true;
			}
			break;
		default:
			throw new IllegalArgumentException();
		}
	}
	
	protected void resetStreams() {
		ListIterator<CWFGM_WeatherStream> itr = m_streamList.listIterator();
		while (itr.hasNext()) {
			itr.next().setWeatherStation(0, null);
		}
	}
	
	/**
	 * This property is unused by this object, and is available for exclusive use by the client code.  It is a VARIANT value to ensure
	 * that the client code can store a pointer value (if it chooses) for use in manual subclassing this object.  This value is not loaded or
	 * saved during serialization operations, and it is the responsibility of the client code to manage any value or object stored here.
	 * @param val Replacement value for UserData.
	 */
	public void setUserData(Object val) {
		m_userData = val;
	}
	
	/**
	 * This property is unused by this object, and is available for exclusive use by the client code.  It is a VARIANT value to ensure
	 * that the client code can store a pointer value (if it chooses) for use in manual subclassing this object.  This value is not loaded or
	 * saved during serialization operations, and it is the responsibility of the client code to manage any value or object stored here.
	 * @return Value of UserData.
	 */
	public Object getUserData() {
		return m_userData;
	}
	
	private void readObject(ObjectInputStream inStream) throws IOException, ClassNotFoundException {
		int version = inStream.readInt();
		if (version == 0) {
			m_loadWarning += "Weather Station: Invalid version.\n";
			//AfxThrowArchiveException(CArchiveException::badSchema, "CWFGM Weather Station");
		}
		if (version > serialVersionUID) {
			m_loadWarning += "Weather Station: Version too new.\n";
			//AfxThrowArchiveException(CArchiveException::badSchema, "CWFGM Weather Station");
		}
		m_latitude = inStream.readDouble();
		m_longitude = inStream.readDouble();
		if (version >= 2)
			m_elevation = inStream.readDouble();

		long cnt;
		if (version == 3)
			cnt = inStream.readShort();
		else
			cnt = inStream.readLong();

		for (long i = 0; i < cnt; i++) {
			CWFGM_WeatherStream ws = new CWFGM_WeatherStream();
			Object varTemp = inStream.readObject();
			
			if (varTemp.getClass() == ws.getClass()) {
				ws = (CWFGM_WeatherStream)varTemp;
				ws.setWeatherStation(0x12345678, this);
				m_streamList.addLast(ws);
			}
			else {
				m_loadWarning += "Failed to load weather stream " + i + "\n";
			}
		}
	}
	
	private void writeObject(ObjectOutputStream outStream) throws IOException {
		outStream.writeInt((int)serialVersionUID);
		outStream.writeDouble(m_latitude);
		outStream.writeDouble(m_longitude);
		outStream.writeDouble(m_elevation);
		
		outStream.writeLong(m_streamList.size());
		
		ListIterator<CWFGM_WeatherStream> itr = m_streamList.listIterator();
		while (itr.hasNext()) {
			outStream.writeObject(itr.next());
		}
	}
}
