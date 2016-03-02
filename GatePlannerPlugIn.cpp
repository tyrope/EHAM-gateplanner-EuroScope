#include "stdafx.h"
#include "GatePlannerPlugIn.h"

// Plugin information
#define GP_PLUGIN_NAME      "EHAM Gateplanner Euroscope Plugin"
#define GP_PLUGIN_VERSION   "0.0.1.1"
#define GP_PLUGIN_AUTHOR    "Dimitri \"TyRope\" Molenaars"
#define GP_PLUGIN_COPYRIGHT "Copyright © 2016 Dimitri \"Tyrope\" Molenaars"

#define GP_API_URL          "http://ehamgateplanner.rammeloo.com/api.php?action=retrievegate&callsign="

// internal ID lists
const int TAG_ITEM_GATE_ASGN = 1;

//---Initialization----------------------------------------
CGatePlannerPlugIn * GatePlannerInstance = NULL;


CGatePlannerPlugIn::CGatePlannerPlugIn(void): CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE,
													  GP_PLUGIN_NAME, GP_PLUGIN_VERSION,
													  GP_PLUGIN_AUTHOR, GP_PLUGIN_COPYRIGHT) {
	// Register Tag item(s).
	RegisterTagItemType("Assigned Gate", TAG_ITEM_GATE_ASGN);
}
CGatePlannerPlugIn::~CGatePlannerPlugIn(void) {}

// Registration
void __declspec (dllexport)
EuroScopePlugInInit(EuroScopePlugIn::CPlugIn ** ppPlugInInstance) {
	// allocate
	*ppPlugInInstance = GatePlannerInstance = new CGatePlannerPlugIn;
}

// De-registration
void __declspec (dllexport)
EuroScopePlugInExit(void) {
	delete GatePlannerInstance;
}

//---Function calls----------------------------------------
void CGatePlannerPlugIn::OnGetTagItem(EuroScopePlugIn::CFlightPlan fp, EuroScopePlugIn::CRadarTarget rt,
				  int ItemCode, int TagData, char sItemString[16], int * pColorCode,
				  COLORREF * pRGB, double * pFontSize){

	// Check for flightplan validity
	if(!fp.IsValid()) { return; }

	switch(ItemCode) {
		case TAG_ITEM_GATE_ASGN:

			//---Pre-API Info Gathering------

			// Callsign
			char* cs = "";
			strcpy(cs, fp.GetCallsign());

			// List index
			int index = _getGateListIndex(cs);

			// Gate
			char* gate = sItemString;

			// API URL
			char* url = "";
			strcpy(url, GP_API_URL);
			strcat(url, cs);

			//---API Info Verification------

			//TODO: Contact API.
			// Below is debug info
			gate = "D3";
			bool rlflight = false;
			bool dvp = false;

			//---Post-API Info Parsing------
			m_knownGates[index].m_Callsign = cs;
			m_knownGates[index].m_Gate = gate;
			m_knownGates[index].m_isRealFlight = rlflight;
			m_knownGates[index].m_isDutchVaccPilot = dvp;

			// Put (new) gate into the ES UI.
			strcpy(sItemString, m_knownGates[index].m_Gate);
			break;
	} // End ItemCode switch
}

int CGatePlannerPlugIn::_getGateListIndex(char* callsign) {
	for(int i = 0; i < sizeof(m_knownGates); i++) {
		if(strcmp(m_knownGates[i].m_Callsign, callsign)) {
			return i;
		}
	}
	return sizeof(m_knownGates);
}
