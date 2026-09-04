/* AmigaOS4 newlib singleplayer build: link-compatible offline services. */
#include "network/net_addons.h"

#include <stdexcept>

namespace AddOns {
namespace {
[[noreturn]] void offline() {
	throw std::runtime_error("Online add-on services are disabled in this build");
}
}  // namespace

NetAddons::~NetAddons() = default;
std::vector<std::string> NetAddons::refresh_remotes(bool) { return {}; }
AddOnInfo NetAddons::fetch_one_remote(const std::string&) { return {}; }
void NetAddons::download_addon(const std::string&, const std::string&, const CallbackFn&) { offline(); }
void NetAddons::download_map(const std::string&, const std::string&) { offline(); }
void NetAddons::download_i18n(const std::string&, const std::string&, const CallbackFn&, const CallbackFn&) { offline(); }
std::string NetAddons::download_screenshot(const std::string&, const std::string&) { offline(); }
int NetAddons::get_vote(const std::string&) { return -1; }
void NetAddons::vote(const std::string&, unsigned) { offline(); }
void NetAddons::comment(const AddOnInfo&, const std::string&, const size_t*) { offline(); }
void NetAddons::upload_addon(const std::string&, const CallbackFn&, const CallbackFn&) { offline(); }
void NetAddons::upload_screenshot(const std::string&, const std::string&, const std::string&) { offline(); }
void NetAddons::admin_action(AdminAction, const AddOnInfo&, const std::string&) { offline(); }
void NetAddons::contact(const std::string&) { offline(); }
void NetAddons::set_login(const std::string& username, const std::string& password) {
	last_username_ = username;
	last_password_ = password;
}
}  // namespace AddOns
