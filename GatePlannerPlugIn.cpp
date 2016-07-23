#include "GatePlannerPlugIn.hpp"

// Plugin information
#define GP_PLUGIN_NAME      "EHAM Gateplanner"
#define GP_PLUGIN_VERSION   "0.0.1_A7"
#define GP_PLUGIN_AUTHOR    "Dimitri \"TyRope\" Molenaars & Thimo Koolen"
#define GP_PLUGIN_COPYRIGHT "Copyright © 2016 Dimitri \"Tyrope\" Molenaars & Thimo Koolen"

//API URL definitions
const std::string GP_API_HOST = "ehamgateplanner.rammeloo.com";
//API endpoints
//#ifdef _DEBUG
//const std::string GP_API_ENDPOINT = "/api.php?action=testplugin&callsign=";
//#else
//const std::string GP_API_ENDPOINT = "/api.php?action=retrievegate&callsign=";
//#endif
const std::string GP_API_ENDPOINT = "/api.php?action=retrievegate&callsign=";

//faked API responses.
const std::string GP_API_REPLY_ERR_PREFIX = "{\"callsign\": \"";
const std::string GP_API_REPLY_ERR_SUFFIX = "\",\"gate\": \"ERR\",\"reallife\": \"ERR\",\"isatc\": \"ERR\",\"iscommunicated\": \"ERR\"}";

// internal ID lists
const int TAG_ITEM_GATE_ASGN = 1;

// Time (in seconds) before we request new information about this flight from the API.
const int DATA_RETENTION_LENGTH = 30;

//-CGatePlannerJSON definitions----------------------------
CGatePlannerJSON::CGatePlannerJSON() {
    // Act as if we contacted the API and it has no idea what we are talking about.
    CGatePlannerJSON("{\"callsign\": \"unknown\",\"gate\": \"UNK\",\"reallife\": \"unk\",\"isatc\": \"unk\",\"iscommunicated\": \"unk\"}");
}

CGatePlannerJSON::CGatePlannerJSON(std::string data) {

    // API result types.
    API_RESULT_NO = 0x00;
    API_RESULT_YES = 0x01;
    API_RESULT_UNK = 0x10;
    API_RESULT_ERR = 0x11;

    //Remove outer {}s
    size_t openCurly = data.find('{'); // Should be 0.
    size_t closeCurly = data.find_last_of('}'); // Should be length-1.
    data = data.substr(openCurly + 1, closeCurly - 1);

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
        key = jsonset[0].substr(startPos + 1, endPos - startPos - 1);

        startPos = jsonset[1].find_first_of('"');
        endPos = jsonset[1].find_last_of('"');
        value = jsonset[1].substr(startPos + 1, endPos - startPos - 1);

        // Assign value to the correct member.
        if(key == "callsign") {
            this->Callsign = value;
        } else if(key == "gate") {
            this->Gate = value;
        } else if(key == "reallife") {
            if(value == "yes") {
                this->IsRealFlight = API_RESULT_YES;
            } else if(value == "no") {
                this->IsRealFlight = API_RESULT_NO;
            } else if(value == "unk") {
                this->IsRealFlight = API_RESULT_UNK;
            } else {
                this->IsRealFlight = API_RESULT_ERR;
            }
        } else if(key == "isatc") {
            if(value == "yes") {
                this->IsDutchVaccPilot = API_RESULT_YES;
            } else if(value == "no") {
                this->IsDutchVaccPilot = API_RESULT_NO;
            } else if(value == "unk") {
                this->IsDutchVaccPilot = API_RESULT_UNK;
            } else {
                this->IsDutchVaccPilot = API_RESULT_ERR;
            }
        } // end key == "??"
    } // end for pieces

    // Set the data age.
    this->lastModified = time(NULL);
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
    // No comma found doesn't mean we're done, add the last section.
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

	DisplayUserMessage("EHAM Gateplanner", "Planner", "Gateplanner Plugin loaded. Version: ALPHA", true, true, true, true, false);
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

            CGatePlannerJSON res;

            //---Should we get new info?
            int lastMod = m_knownFlightInfo[cs].lastModified + DATA_RETENTION_LENGTH;
            int now = time(NULL);

            if(lastMod < now && lastMod > 0) { // Greater than 0 to prevent unset values being strange.
                // Information too recent.
                res = m_knownFlightInfo[cs];
            } else {
                //---API Info Retrieval------
                res = GetAPIInfo(cs);
            }
			

            //---API Info Verification------
            if(cs == res.Callsign) {
                m_knownFlightInfo[cs] = res;
                // Put (new) gate into the ES UI.
                //strcpy_s(sItemString, 4, m_knownFlightInfo[cs].Gate.c_str());
				DisplayUserMessage("EHAM Gateplanner", "Planner", "Ping", true, true, true, true, false);
				strcpy_s(sItemString, 4, "TE");
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

    // Which API endpoint?
    std::string uri = GP_API_ENDPOINT + callsign;

    //-Here be dragons.-------------------------
    try {
        // Initialize the asio service.
        boost::asio::io_service io_service;
        // Get a list of endpoints corresponding to the server name.
        tcp::resolver resolver(io_service);
        tcp::resolver::query query(GP_API_HOST, "http");
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
        // Try each endpoint until we successfully establish a connection.
        tcp::socket socket(io_service);
        boost::asio::connect(socket, endpoint_iterator);
        // Form the request.
        boost::asio::streambuf request;
        std::ostream request_stream(&request);
        request_stream << "GET " << uri << " HTTP/1.0\r\n";
        request_stream << "Host: " << GP_API_HOST << "\r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "Connection: close\r\n\r\n";

        // Send the request.
        boost::asio::write(socket, request);
        // Read the response line.
        boost::asio::streambuf response;
        boost::asio::read_until(socket, response, "\r\n");
        // Check that response is OK.
        std::istream response_stream(&response);
        std::string http_version;
        response_stream >> http_version;
        unsigned int status_code;
        response_stream >> status_code;
        std::string status_message;
        std::getline(response_stream, status_message);
        if(!response_stream || http_version.substr(0, 5) != "HTTP/" || status_code != 200) {
            return CGatePlannerJSON(GP_API_REPLY_ERR_PREFIX + callsign + GP_API_REPLY_ERR_SUFFIX);
        }
        // Read the response headers, which are terminated by a blank line.
        boost::asio::read_until(socket, response, "\r\n\r\n");
        // Process the response headers.
        std::string header;
        while(std::getline(response_stream, header) && header != "\r")
            continue;
        // Write whatever content we already have to output.
        if(response.size() > 0) {
            std::istream(&response) >> data;
        }
        // Read until EOF, writing data to output as we go
        boost::system::error_code error;
        while(boost::asio::read(socket, response,
            boost::asio::transfer_at_least(1), error)) {
            std::istream(&response) >> data;
        }
        if(error != boost::asio::error::eof) {
            throw boost::system::system_error(error);
        }
        //-End of dragons.--------------------------
    } catch(std::exception&) {
        return CGatePlannerJSON(GP_API_REPLY_ERR_PREFIX + callsign + GP_API_REPLY_ERR_SUFFIX);
    }

    // Parse data
    return CGatePlannerJSON(data);
}
