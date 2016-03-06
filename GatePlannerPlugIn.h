#pragma once

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <Windows.h>
#include <EuroScopePlugIn.h>
#include <map>
#include <iostream>

using boost::asio::ip::tcp;

using namespace EuroScopePlugIn;

class CGatePlannerJSON {
public:
    CGatePlannerJSON();
    CGatePlannerJSON(std::string data);
    virtual ~CGatePlannerJSON();
    std::map<int, std::string> string_split(std::string data, char delimiter);
    std::string Callsign;
    std::string Gate;
    bool IsRealFlight;
    bool IsDutchVaccPilot;
};

class CGatePlannerPlugIn:
    public EuroScopePlugIn::CPlugIn {

protected:
    CGatePlannerJSON GetAPIInfo(std::string callsign);

public:
    CGatePlannerPlugIn();
    virtual ~CGatePlannerPlugIn();
    std::map<std::string, CGatePlannerJSON> m_knownFlightInfo;
    virtual void OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan,
                              EuroScopePlugIn::CRadarTarget RadarTarget,
                              int ItemCode,
                              int TagData,
                              char sItemString[16],
                              int * pColorCode,
                              COLORREF * pRGB,
                              double * pFontSize);
    virtual void OnFlightPlanDisconnect(CFlightPlan FlightPlan);
};
