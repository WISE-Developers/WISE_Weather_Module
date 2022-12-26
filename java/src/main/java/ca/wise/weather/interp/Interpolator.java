/***********************************************************************
 * REDapp - Interpolator.java
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

package ca.wise.weather.interp;

import java.util.ArrayList;
import java.util.List;

import org.apache.commons.math3.analysis.interpolation.SplineInterpolator;
import org.apache.commons.math3.analysis.polynomials.PolynomialFunctionLagrangeForm;
import org.apache.commons.math3.analysis.polynomials.PolynomialSplineFunction;

/**
 * Interpolates weather information using spline interpolation from the Apache Commons Math library.
 * 
 * @author Travis
 */
public class Interpolator {
	public static void main(String[] args) {
		double[] times = { 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 };
		double[] temps = { 12.4, 11.8, 11.1, 10.0, 12.7, 14.1, 16.4, 17.4, 18.9, 20.0, 20.9, 21.5, 22.3, 21.9, 22.2, 21.2, 20.0 };
		
		HourValue[] values = new HourValue[times.length];
		for (int i = 0; i < times.length; i++) {
			values[i] = new HourValue(times[i], temps[i]);
		}
		
		Interpolator inter = new Interpolator();
		HourValue[] retval = inter.splineInterpolate(values);
		retval = inter.extrapolate(retval, 0, 23);
		
		for (int i = 0; i < retval.length; i++) {
			System.out.println("(" + retval[i].houroffset + ", " + retval[i].value + ")");
		}
		
		return;
	}
	
	/**
	 * Use a spline function to interpolate a set of known values at known hour offsets
	 * from an arbitrary base time.
	 * @param knownValues The list of known values and time offsets.
	 * @return The list of interpolated values (including the known values).
	 */
	public HourValue[] splineInterpolate(HourValue[] knownValues) {
		if (knownValues.length < 2)
			return null;
		SplineInterpolator spline = new SplineInterpolator();
		double[] times = new double[knownValues.length];
		double[] vals = new double[knownValues.length];
		int index = 0;
		for (HourValue v : knownValues) {
			times[index] = v.houroffset;
			vals[index] = v.value;
			index++;
		}
		PolynomialSplineFunction funct = spline.interpolate(times, vals);
		List<HourValue> retval = new ArrayList<HourValue>();
		double hour = Math.ceil(knownValues[0].houroffset);
		while (hour <= knownValues[knownValues.length - 1].houroffset) {
			retval.add(new HourValue(hour, funct.value(hour)));
			hour += 1.0;
		}
		return retval.toArray(new HourValue[0]);
	}
	
	protected List<HourValue> extrapolateBeginning(HourValue[] knownValues, double startOffset) {
		List<HourValue> list = new ArrayList<HourValue>();
		boolean trendUp;
		list.add(knownValues[0]);
		if (knownValues[1].value > knownValues[0].value)
			trendUp = true;
		else
			trendUp = false;
		list.add(knownValues[1]);
		for (int i = 2; i < knownValues.length; i++) {
			if (trendUp) {
				if (knownValues[i].value > knownValues[i - 1].value) {
					list.add(knownValues[i]);
				}
				else {
					break;
				}
			}
			else {
				if (knownValues[i].value < knownValues[i - 1].value) {
					list.add(knownValues[i]);
				}
				else {
					break;
				}
			}
		}
		if (list.size() < 3) {
			return null;
		}
		double[] times = new double[list.size()];
		double[] vals = new double[list.size()];
		for (int i = 0; i < list.size(); i++) {
			times[i] = list.get(i).houroffset;
			vals[i] = list.get(i).value;
		}
		PolynomialFunctionLagrangeForm funct = new PolynomialFunctionLagrangeForm(times, vals);
		double hour = startOffset;
		List<HourValue> retval = new ArrayList<HourValue>();
		while (hour < knownValues[0].houroffset) {
			retval.add(new HourValue(hour, funct.value(hour)));
			hour++;
		}
		
		return retval;
	}
	
	protected List<HourValue> extrapolateEnding(HourValue[] knownValues, double endOffset) {
		List<HourValue> list = new ArrayList<HourValue>();
		boolean trendUp;
		list.add(knownValues[knownValues.length - 1]);
		if (knownValues[knownValues.length - 2].value > knownValues[knownValues.length - 1].value)
			trendUp = true;
		else
			trendUp = false;
		list.add(knownValues[knownValues.length - 2]);
		for (int i = knownValues.length - 3; i >= 0; i--) {
			if (trendUp) {
				if (knownValues[i].value > knownValues[i + 1].value) {
					list.add(knownValues[i]);
				}
				else {
					break;
				}
			}
			else {
				if (knownValues[i].value < knownValues[i + 1].value) {
					list.add(knownValues[i]);
				}
				else {
					break;
				}
			}
		}
		double[] times = new double[list.size()];
		double[] vals = new double[list.size()];
		for (int i = 0; i < list.size(); i++) {
			times[i] = list.get(i).houroffset;
			vals[i] = list.get(i).value;
		}
		PolynomialFunctionLagrangeForm funct = new PolynomialFunctionLagrangeForm(times, vals);
		double hour = knownValues[knownValues.length - 1].houroffset + 1;
		List<HourValue> retval = new ArrayList<HourValue>();
		while (hour <= endOffset) {
			retval.add(new HourValue(hour, funct.value(hour)));
			hour++;
		}
		return retval;
	}
	
	public HourValue[] extrapolate(HourValue[] knownValues, double startOffset, double endOffset) {
		if (knownValues.length < 2)
			return null;
		List<HourValue> retval = new ArrayList<HourValue>();
		if (startOffset >= 0) {
			List<HourValue> start = extrapolateBeginning(knownValues, startOffset);
			if (start != null)
				retval.addAll(start);
		}
		for (int i = 0; i < knownValues.length; i++) {
			retval.add(knownValues[i]);
		}
		if (endOffset >= 0) {
			List<HourValue> end = extrapolateEnding(knownValues, endOffset);
			if (end != null)
				retval.addAll(end);
		}
		
		return retval.toArray(new HourValue[0]);
	}

	public static class HourValue {
		public double houroffset;
		public double value;

		public HourValue(double houroffset, double value) { this.houroffset = houroffset; this.value = value; }
		public HourValue() { }

		@Override
		public String toString() {
			return "(" + houroffset + ", " + value + ")";
		}
	}
}
