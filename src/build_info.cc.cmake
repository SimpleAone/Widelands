#include "build_info.h"
#include <string>

static const std::string wl_bid = "@WL_VERSION@";
static const std::string wl_bt  = "@CMAKE_BUILD_TYPE@";
static const std::string wl_port_version = "@AMIGAOS4_PORT_VERSION@";
static const std::string wl_build_date = "@AMIGAOS4_BUILD_DATE@";
static const std::string wl_bverdetail = wl_bid + " " + wl_bt +
    (wl_port_version.empty() ? "" : " AmigaOS4 " + wl_port_version + " built " + wl_build_date);

const std::string & build_id()
{
	return wl_bid;
}

const std::string & build_type()
{
	return wl_bt;
}

const std::string & build_ver_details()
{
	return wl_bverdetail;
}

