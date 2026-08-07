#include <iostream>

#include "Bank.h"
#include "UI.h"
#include "ATM.h"

int main() {
    Bank bank("data");

    int choice;

    while (true) {
        std::cout << "\n=====================================\n";
        std::cout << "      BANK MANAGEMENT SYSTEM\n";
        std::cout << "=====================================\n";
        std::cout << "1. Admin Panel\n";
        std::cout << "2. ATM\n";
        std::cout << "0. Exit\n";
        std::cout << "-------------------------------------\n";
        std::cout << "Enter your choice: ";

        std::cin >> choice;

        switch (choice) {
        case 1:
            RunAdminUI(bank);
            break;

        case 2:
            RunATMUI(bank);
            break;

        case 0:
            std::cout << "Goodbye!\n";
            return 0;

        default:
            std::cout << "Invalid choice.\n";
            break;
        }
    }

    return 0;
}