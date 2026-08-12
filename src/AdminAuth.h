#pragma once
#include <string>

// Handles the admin login credential file (data/admin_credentials.dat).
// Format: one line, "username|hexhash" (hash, not the plain password).
// Same plain-text-on-disk approach as FileManager, just hashed instead
// of storing the password itself.
namespace AdminAuth {

// Creates data/admin_credentials.dat with the default credentials
// (username "admin", password "admin123") if it does not already exist.
// Safe to call every time the app starts.
void ensureCredentialsFile(const std::string& dataDir);

// Checks a username/password pair against the stored, hashed credentials.
bool verify(const std::string& dataDir, const std::string& username, const std::string& password);

}  // namespace AdminAuth
