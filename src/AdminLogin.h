#pragma once
#include "Bank.h"

// Shows the admin login screen (username + password, checked against
// data/admin_credentials.dat via AdminAuth). Replaces the old behavior of
// opening the Admin Dashboard immediately when "Admin Panel" is clicked on
// the launcher.
//
// On successful login this opens the Admin Dashboard itself (see UI.h) and
// blocks until that window is closed. On Cancel or closing the login
// window, it just returns -- control goes back to the launcher either way.
void RunAdminLoginUI(Bank& bank);
