#pragma once

#include <Windows.h> // Remove compile errors regarding EuroScopePlugIn.h
#include <EuroScopePlugIn.h>

class CKnownGates {
public:
	char* m_Callsign;
	char* m_Gate;
	bool m_isRealFlight;
	bool m_isDutchVaccPilot;
};

class CGatePlannerPlugIn:
	public EuroScopePlugIn::CPlugIn {

protected:
	CKnownGates* m_knownGates;
	int _getGateListIndex(char* callsign);

public:
	CGatePlannerPlugIn();
	virtual ~CGatePlannerPlugIn();

	virtual void OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan,
							  EuroScopePlugIn::CRadarTarget RadarTarget,
							  int ItemCode,
							  int TagData,
							  char sItemString[16],
							  int * pColorCode,
							  COLORREF * pRGB,
							  double * pFontSize);
};
