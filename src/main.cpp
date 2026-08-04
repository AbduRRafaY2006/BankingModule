#include "Bank.h"
#include "UI.h"

int main() {
    Bank bank("data");  // same file-based storage as the FTXUI version
    RunAdminUI(bank);
    return 0;
}
