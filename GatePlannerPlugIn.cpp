#include "stdafx.h"
#include "GatePlannerPlugIn.h"

// Plugin information
#define GP_PLUGIN_NAME      "EHAM Gateplanner"
#define GP_PLUGIN_VERSION   "0.0.1.1"
#define GP_PLUGIN_AUTHOR    "Dimitri \"TyRope\" Molenaars"
#define GP_PLUGIN_COPYRIGHT "Copyright © 2016 Dimitri \"Tyrope\" Molenaars"

const std::string GP_URL = "http://ehamgateplanner.rammeloo.com/api.php?action=retrievegate&callsign=";

// internal ID lists
const int TAG_ITEM_GATE_ASGN = 1;

//-CGatePlannerJSON definitions----------------------------
CGatePlannerJSON::CGatePlannerJSON() {
    CGatePlannerJSON("{\"callsign\": \"XXX9999\",\"gate\": \"UNK\",\"reallife\": \"no\",\"isatc\": \"no\"}");
}

CGatePlannerJSON::CGatePlannerJSON(std::string data) {
    //Remove outer {}s
    size_t openCurly = data.find('{'); // Should be 0.
    size_t closeCurly = data.find_last_of('}'); // Should be length-1.
    data = data.substr(openCurly + 1, closeCurly-1);

    //Split by commas
    std::map<int, std::string> jsonset, pieces = string_split(data, ',');
    std::string key, value;
    size_t startPos, endPos;

    // Loop over each JSON k/v set.
    for(int i = 0; i < sizeof(pieces); i++) {

        // New k/v set.
        jsonset = string_split(pieces[i], ':');

        startPos = jsonset[0].find_first_of('"');
        endPos = jsonset[0].find_last_of('"');
        key = jsonset[0].substr(startPos+1, endPos - startPos-1);

        startPos = jsonset[1].find_first_of('"');
        endPos = jsonset[1].find_last_of('"');
        value = jsonset[1].substr(startPos+1, endPos - startPos-1);

        // Assign value to the correct member.
        if(key == "callsign") {
            this->Callsign = value;
        } else if(key == "gate") {
            this->Gate = value;
        } else if(key == "reallife") {
            this->IsRealFlight = (value == "yes");
        } else if(key == "isatc") {
            this->IsDutchVaccPilot = (value == "yes");
        }

    } // end for pieces
}

CGatePlannerJSON::~CGatePlannerJSON() {}

std::map<int, std::string> CGatePlannerJSON::string_split(std::string data, char delimiter) {
    std::map<int, std::string> pieces;
    size_t pos = 0, prevpos = 0, c = 0;
    while((pos = data.find_first_of(delimiter, prevpos)) != std::string::npos) {
        pieces[c] = data.substr(prevpos, pos - prevpos);
        prevpos = pos + 1;
        c++;
    }
    // No , found doesn't mean we're done, add the last section.
    pieces[c] = data.substr(prevpos);
    return pieces;
}


//-CGatePlannerPlugIn definitions--------------------------
//---Initialization----------------------------------------
CGatePlannerPlugIn * GatePlannerInstance = NULL;

CGatePlannerPlugIn::CGatePlannerPlugIn(void): CPlugIn(COMPATIBILITY_CODE,
                                                      GP_PLUGIN_NAME,
                                                      GP_PLUGIN_VERSION,
                                                      GP_PLUGIN_AUTHOR,
                                                      GP_PLUGIN_COPYRIGHT) {
    // Initialize m_knownGates with an invalid flightplan.
    CGatePlannerJSON json = CGatePlannerJSON();
    m_knownFlightInfo.insert(std::pair<std::string, CGatePlannerJSON>(json.Callsign, json));

    // Register Tag item(s).
    RegisterTagItemType("Assigned Gate", TAG_ITEM_GATE_ASGN);
}
CGatePlannerPlugIn::~CGatePlannerPlugIn(void) {}

// Registration
void __declspec (dllexport)
EuroScopePlugInInit(CPlugIn ** ppPlugInInstance) {
    // allocate
    *ppPlugInInstance = GatePlannerInstance = new CGatePlannerPlugIn;
}

// De-registration
void __declspec (dllexport)
EuroScopePlugInExit(void) {
    delete GatePlannerInstance;
}

//---CPlugIn overrides-------------------------------------
void CGatePlannerPlugIn::OnGetTagItem(CFlightPlan FlightPlan, CRadarTarget RadarTarget,
                                      int ItemCode, int TagData,
                                      char sItemString[16], int * pColorCode,
                                      COLORREF * pRGB, double * pFontSize) {

    // Only work on tag items we actually care about.
    switch(ItemCode) {
        case TAG_ITEM_GATE_ASGN:
            // Check for flightplan validity
            if(!FlightPlan.IsValid()) {
                return;
            }

            //---Pre-API Info Gathering------
            std::string cs = FlightPlan.GetCallsign();

            //---API Info Retrieval------
            CGatePlannerJSON res = GetAPIInfo(cs);

            //---API Info Verification------
            if(cs == res.Callsign) {
                m_knownFlightInfo[cs] = res;
                // Put (new) gate into the ES UI.
                strcpy_s(sItemString, 4, m_knownFlightInfo[cs].Gate.c_str());
            } else {
                // Invalid callsign?
                sItemString = "ERR";
            }
    } // End ItemCode switch
}

void CGatePlannerPlugIn::OnFlightPlanDisconnect(CFlightPlan FlightPlan) {
    std::string cs = FlightPlan.GetCallsign();
    m_knownFlightInfo.erase(cs);
}


//---Non-inherit Function Definitions----------------------
CGatePlannerJSON CGatePlannerPlugIn::GetAPIInfo(std::string callsign) {
    std::string data = "";

    //TODO remove debug & talk to API.
    return CGatePlannerJSON("{\"callsign\": \""+callsign+"\",\"gate\": \"E19\",\"reallife\": \"no\",\"isatc\": \"no\"}");


    // Build API endpoint
    std::string uri = GP_URL + callsign;

    //Boost::Asio things.
    boost::asio::io_service io_service;
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(uri, "http");
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::socket socket(io_service);

    // Connect.
    boost::asio::connect(socket, endpoint_iterator);

    //Retrieve data
    for(;;) {
        boost::array<char, 128> buf;
        boost::system::error_code error;

        size_t len = socket.read_some(boost::asio::buffer(buf), error);
        try {
            if(error == boost::asio::error::eof) {
                break; // Connection closed cleanly by peer.
            } else if(error) {
                throw boost::system::system_error(error); // Some other error.
            }
            data = data + buf.data();
        } catch(std::exception&) {
            //TODO catch exception
        }
    }

    // Parse data
    return CGatePlannerJSON(data);
}

