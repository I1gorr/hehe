#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <future>
#include <termios.h>
#include <sys/select.h>
#include "ProcessInfo.h"

using namespace std;

string priorityProcess = "";

// Function to get and print process details from processes.csv
vector<ProcessInfo> getProcessDetails() {
    ifstream file("processes.csv");
    if (!file.is_open()) {
        cerr << "Error: Unable to open processes.csv" << endl;
        return {};
    }

    string line;
    getline(file, line); // Skip header

    vector<ProcessInfo> processes;
    double totalCPUUsage = 0.0;

    cout << "\n----- Process Details from processes.csv -----\n";
    cout << "Date\t\tPID\tProcess Name\tMemory Usage\tCPU Usage\n";

    while (getline(file, line)) {
        stringstream ss(line);
        ProcessInfo process;

        getline(ss, process.date, ',');
        getline(ss, process.pid, ',');
        getline(ss, process.name, ',');
        getline(ss, process.memUsage, ',');
        ss >> process.cpuUsage;

        cout << process.date << "\t" << process.pid << "\t" << process.name << "\t" 
             << process.memUsage << "\t" << process.cpuUsage << "%" << endl;

        totalCPUUsage += process.cpuUsage;
        processes.push_back(process);
    }

    file.close();
    cout << "---------------------------------------------\n";
    return processes;
}

// Function to set power profile
void adjustPowerProfile(double totalCPUUsage, const vector<ProcessInfo>& processes) {
    string profile;
    
    bool priorityRunning = false;
    for (const auto& process : processes) {
        if (process.name == priorityProcess) {
            priorityRunning = true;
            break;
        }
    }
    
    if (priorityRunning) {
        profile = "latency-performance";
    } else if (totalCPUUsage < 20.0) {
        profile = "powersave";
    } else if (totalCPUUsage < 50.0) {
        profile = "balanced";
    } else {
        profile = "latency-performance";
    }

    string command = "bash -c 'tuned-adm profile " + profile + "'";
    cout << "Adjusting power profile to: " << profile << endl;

    int result = system(command.c_str());
    if (result != 0) {
        cerr << "Failed to set power profile. Check permissions or environment variables." << endl;
    }
}

// Function to kill a process by name
void killProcessByName(const string& processName) {
    string command = "pkill -f " + processName;
    cout << "Attempting to kill process: " << processName << endl;

    int result = system(command.c_str());
    if (result == 0) {
        cout << "Process " << processName << " terminated successfully." << endl;
    } else {
        cerr << "Failed to terminate process " << processName << ". It may not exist." << endl;
    }
}

// Function to ask user if they want to kill a process (with a timeout)
void askToKillProcess() {
    cout << "Do you want to kill a process? (yes/no): ";
    
    fd_set set;
    struct timeval timeout;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    
    timeout.tv_sec = 10;  // 10 seconds timeout
    timeout.tv_usec = 0;
    
    int ret = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);
    
    if (ret > 0) {  
        string response;
        cin >> response;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');  // Clear any leftover input
        if (response == "yes") {
            cout << "Enter the process name to kill: ";
            string processName;
            cin >> processName;
            cin.ignore(numeric_limits<streamsize>::max(), '\n');  // Clear input buffer again
            killProcessByName(processName);
        }
    } else {
        cout << "No response in 10 seconds. Moving forward...\n";
    }

    tcflush(STDIN_FILENO, TCIFLUSH);  // Ensure standard input is reset
    cout << "askToKillProcess() returned, continuing...\n";
}

void askForPriorityProcess() {
    cout << "Would you like to set a priority process? (yes/no): ";
    string response;
    cin >> response;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    
    if (response == "yes") {
        cout << "Enter the process name to prioritize: ";
        cin >> priorityProcess;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Priority process set to: " << priorityProcess << endl;
    }
}

int main() {
    askForPriorityProcess();
    
    while (true) {
        cout << "Running csvExport to collect process data..." << endl;
        system("./csvExport");  

        this_thread::sleep_for(chrono::seconds(10));  // Wait for csvExport to complete

        cout << "Reading processes.csv to determine CPU usage..." << endl;
        vector<ProcessInfo> processes = getProcessDetails();

        // Detect number of CPU cores dynamically
        int numCores = sysconf(_SC_NPROCESSORS_ONLN);
        if (numCores <= 0) {
            cerr << "Error retrieving CPU core count. Defaulting to 1 core." << endl;
            numCores = 1;
        }

        // Calculate total CPU usage
        double totalCPUUsage = 0.0;
        for (const auto& process : processes) {
            totalCPUUsage += process.cpuUsage;
        }

        // Normalize by core count
        totalCPUUsage /= numCores;

        cout << "Total CPU Usage (normalized): " << totalCPUUsage << "%" << endl;

        adjustPowerProfile(totalCPUUsage, processes);
        askToKillProcess();  // Ask user if they want to kill a process (with timeout)

        cout << "Sleeping for 2 minutes before next update...\n" << endl;
        this_thread::sleep_for(chrono::minutes(2)); // Wait 2 minutes 
    }

    return 0;
}