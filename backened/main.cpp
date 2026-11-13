
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <limits>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
using namespace std;

const string FILE_NAME = "complaints.txt";

struct Complaint {
    int id = 0;
    string category, type, location, description, status;
    int safety = 1;
    int severity = 1;
    int people = 1;
    long long reportedAt = 0;
    bool urgent = false;
};

vector<Complaint> complaints;
int nextId = 1;

// ---------- INPUT HELPERS ----------

string readText(const string& prompt) {
    string value;
    cout << prompt;
    getline(cin, value);

    // Keep the | delimiter safe for our storage format.
    replace(value.begin(), value.end(), '|', '/');
    return value;
}

int readInt(const string& prompt, int low, int high) {
    int value;

    while (true) {
        cout << prompt;

        if (cin >> value && value >= low && value <= high) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        }

        cout << "Enter a valid number from "
             << low << " to " << high << ".\n";

        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

bool readYesNo(const string& prompt) {
    while (true) {
        string answer = readText(prompt);

        if (answer == "y" || answer == "Y" ||
            answer == "yes" || answer == "YES")
            return true;

        if (answer == "n" || answer == "N" ||
            answer == "no" || answer == "NO")
            return false;

        cout << "Please enter y or n.\n";
    }
}

// ---------- PRIORITY ENGINE ----------

int affectedScore(int people) {
    if (people <= 5) return 1;
    if (people <= 20) return 2;
    if (people <= 50) return 3;
    if (people <= 200) return 4;
    return 5;
}

int waitingDays(const Complaint& c) {
    if (c.reportedAt <= 0) return 0;

    long long now = static_cast<long long>(time(nullptr));
    long long elapsed = max(0LL, now - c.reportedAt);

    return static_cast<int>(elapsed / 86400);
}

int waitingScore(int days) {
    if (days < 1) return 1;
    if (days <= 3) return 2;
    if (days <= 7) return 3;
    if (days <= 14) return 4;
    return 5;
}

int priorityScore(const Complaint& c) {
    return 8 * c.safety
         + 6 * c.severity
         + 4 * affectedScore(c.people)
         + 2 * waitingScore(waitingDays(c));
}

string priorityLabel(const Complaint& c) {
    if (c.urgent) return "URGENT";
    
    int score = priorityScore(c);

    if (score >= 80) return "Critical";
    if (score >= 60) return "High";
    if (score >= 40) return "Medium";
    return "Low";
}

void explainPriority(const Complaint& c) {
    cout << "\n--- Priority explanation ---\n";
    cout << "Safety risk: " << c.safety << "/5"
         << " -> " << 8 * c.safety << " points\n";

    cout << "Issue severity estimate: " << c.severity << "/5"
         << " -> " << 6 * c.severity << " points\n";

    cout << "Affected population estimate: "
         << c.people << " people; score "
         << affectedScore(c.people) << "/5"
         << " -> " << 4 * affectedScore(c.people)
         << " points\n";

    cout << "Waiting time: " << waitingDays(c) << " days"
         << "; score " << waitingScore(waitingDays(c))
         << "/5 -> " << 2 * waitingScore(waitingDays(c))
         << " points\n";

    cout << "Numeric priority score: "
         << priorityScore(c) << "/100\n";

    if (c.urgent) {
        cout << "Urgent flag: YES. This complaint is flagged "
                "for immediate attention regardless of its score.\n";
    } else {
        cout << "Priority level: " << priorityLabel(c) << '\n';
    }
}

// ---------- FILE STORAGE ----------

vector<string> splitLine(const string& line) {
    vector<string> fields;
    string field;
    stringstream ss(line);

    while (getline(ss, field, '|'))
        fields.push_back(field);

    // Preserve a final empty field if the line ends with |.
    if (!line.empty() && line.back() == '|')
        fields.push_back("");

    return fields;
}

void saveToFile() {
    ofstream file(FILE_NAME);

    if (!file) {
        cout << "Error: could not save complaints.\n";
        return;
    }

    for (const Complaint& c : complaints) {
        file << c.id << '|'
             << c.category << '|'
             << c.type << '|'
             << c.location << '|'
             << c.description << '|'
             << c.status << '|'
             << c.safety << '|'
             << c.severity << '|'
             << c.people << '|'
             << c.reportedAt << '|'
             << c.urgent << '\n';
    }

    if (!file)
        cout << "Error occurred while writing complaint data.\n";
}

void loadFromFile() {
    ifstream file(FILE_NAME);
    string line;

    while (getline(file, line)) {
        if (line.empty()) continue;

        vector<string> f = splitLine(line);

        try {
            Complaint c;

            // New format: 11 fields.
            if (f.size() == 11) {
                c.id = stoi(f[0]);
                c.category = f[1];
                c.type = f[2];
                c.location = f[3];
                c.description = f[4];
                c.status = f[5];
                c.safety = stoi(f[6]);
                c.severity = stoi(f[7]);
                c.people = stoi(f[8]);
                c.reportedAt = stoll(f[9]);
                c.urgent = (f[10] == "1");
            }
            // Legacy format from the original main.cpp.
            else if (f.size() == 9) {
                c.id = stoi(f[0]);
                c.category = "Other";
                c.type = f[1];
                c.location = f[2];
                c.description = f[3];

                c.status = f[4];
                if (c.status == "Open")
                    c.status = "Submitted";

                c.safety = stoi(f[5]);
                c.severity = stoi(f[6]);
                c.people = max(0, stoi(f[7]));

                int oldWaitingDays = max(0, stoi(f[8]));
                c.reportedAt =
                    static_cast<long long>(time(nullptr))
                    - static_cast<long long>(oldWaitingDays) * 86400;
            } else {
                cout << "Skipped a record with an invalid format.\n";
                continue;
            }

            if (c.id <= 0 || c.safety < 1 || c.safety > 5 ||
                c.severity < 1 || c.severity > 5 ||
                c.people < 0 || c.status.empty()) {
                cout << "Skipped an invalid complaint record.\n";
                continue;
            }

            auto duplicate = find_if(
                complaints.begin(), complaints.end(),
                [&](const Complaint& existing) {
                    return existing.id == c.id;
                });

            if (duplicate != complaints.end())
                continue;

            complaints.push_back(c);
            nextId = max(nextId, c.id + 1);
        } catch (...) {
            cout << "Skipped a complaint record with invalid data.\n";
        }
    }
}

// ---------- COMPLAINT REGISTRATION ----------

void addComplaint() {
    Complaint c;

    cout << "\n--- Register civic complaint ---\n";

    cout << "Categories:\n"
         << "1. Roads and transport\n"
         << "2. Waste management\n"
         << "3. Electricity and lighting\n"
         << "4. Water and drainage\n"
         << "5. Public spaces\n"
         << "6. Environment and hazards\n"
         << "7. Other\n";

    int choice = readInt("Choose category (1-7): ", 1, 7);

    const vector<string> categories = {
        "Roads and transport",
        "Waste management",
        "Electricity and lighting",
        "Water and drainage",
        "Public spaces",
        "Environment and hazards",
        "Other"
    };

    c.category = categories[choice - 1];
    c.type = readText("Issue type (e.g. pothole, overflowing bin): ");
    c.location = readText("Location / area: ");
    c.description = readText("Describe the problem: ");

    cout << "\nLocation/activity context:\n"
         << "1. Quiet or low-traffic area\n"
         << "2. Residential street\n"
         << "3. Busy public area\n"
         << "4. School, hospital, or busy road\n"
         << "5. Major public area affecting many people\n";

    int context = readInt("Choose context (1-5): ", 1, 5);

    // These are estimates from context, not verified headcounts.
    const int estimatedPeople[] = {5, 20, 50, 200, 201};
    c.people = estimatedPeople[context - 1];

    bool danger = readYesNo(
        "Is there an immediate safety danger to people? (y/n): ");

    c.safety = danger ? 5 : 2;

    // Transparent default severity estimate by category.
    const int severityEstimates[] = {4, 3, 3, 4, 2, 4, 2};
    c.severity = severityEstimates[choice - 1];

    c.urgent = readYesNo(
        "Flag this complaint for immediate attention? (y/n): ");

    c.id = nextId++;
    c.status = "Submitted";
    c.reportedAt = static_cast<long long>(time(nullptr));

    complaints.push_back(c);
    saveToFile();

    cout << "\nComplaint registered successfully!\n";
    cout << "Complaint ID: " << c.id << '\n';
    cout << "Category: " << c.category << '\n';
    explainPriority(c);
}

// ---------- DISPLAY AND SEARCH ----------

void displayOne(const Complaint& c) {
    cout << "\n----------------------------------\n";
    cout << "ID: " << c.id
         << "\nCategory: " << c.category
         << "\nIssue: " << c.type
         << "\nLocation: " << c.location
         << "\nDescription: " << c.description
         << "\nStatus: " << c.status
         << "\nPriority: " << priorityLabel(c)
         << "\nScore: " << priorityScore(c) << "/100\n";

    explainPriority(c);
}

void displayComplaints() {
    if (complaints.empty()) {
        cout << "No complaints found.\n";
        return;
    }

    for (const Complaint& c : complaints)
        displayOne(c);
}

void searchComplaint() {
    int id = readInt("Enter complaint ID: ", 1,
                     numeric_limits<int>::max());

    // Linear search: O(n).
    for (const Complaint& c : complaints) {
        if (c.id == id) {
            displayOne(c);
            return;
        }
    }

    cout << "Complaint not found.\n";
}

void rankComplaints() {
    if (complaints.empty()) {
        cout << "No complaints to rank.\n";
        return;
    }

    vector<Complaint> ranked = complaints;

    // Urgent complaints first; then numeric score descending.
    stable_sort(ranked.begin(), ranked.end(),
        [](const Complaint& a, const Complaint& b) {
            if (a.urgent != b.urgent)
                return a.urgent > b.urgent;

            return priorityScore(a) > priorityScore(b);
        });

    cout << "\n--- Complaints ranked by priority ---\n";

    for (const Complaint& c : ranked) {
        cout << "\nID: " << c.id
             << " | " << c.category
             << " | " << c.type
             << " | Score: " << priorityScore(c) << "/100"
             << " | Priority: " << priorityLabel(c)
             << " | Status: " << c.status << '\n';
    }
}

// ---------- STATUS MANAGEMENT ----------

void updateStatus() {
    int id = readInt("Enter complaint ID: ", 1,
                     numeric_limits<int>::max());

    for (Complaint& c : complaints) {
        if (c.id != id) continue;

        cout << "Current status: " << c.status << '\n'
             << "1. Submitted\n"
             << "2. In Progress\n"
             << "3. Resolved\n"
             << "4. Closed\n";

        int choice = readInt("Choose new status (1-4): ", 1, 4);

        const vector<string> statuses = {
            "Submitted", "In Progress", "Resolved", "Closed"
        };

        c.status = statuses[choice - 1];
        saveToFile();

        cout << "Status updated to " << c.status << ".\n";
        return;
    }

    cout << "Complaint not found.\n";
}

// ---------- DASHBOARD ----------

void showDashboard() {
    int total = static_cast<int>(complaints.size());
    int open = 0, high = 0, resolved = 0;

    for (const Complaint& c : complaints) {
        if (c.status == "Submitted" || c.status == "In Progress")
            open++;

        if (c.status == "Resolved" || c.status == "Closed")
            resolved++;

        if (c.urgent || priorityScore(c) >= 60)
            high++;
    }

    cout << "\n========== CIVICPULSE DASHBOARD ==========\n";
    cout << "Total complaints: " << total << '\n';
    cout << "Open complaints: " << open << '\n';
    cout << "High-priority / urgent complaints: " << high << '\n';
    cout << "Resolved or closed complaints: " << resolved << '\n';
    cout << "==========================================\n";
}

// ---------- MAIN MENU ----------

int main() {
    loadFromFile();

    int choice;

    do {
        cout << "\n========== CIVICPULSE ==========\n"
             << "1. Register complaint\n"
             << "2. View all complaints\n"
             << "3. Search complaint by ID\n"
             << "4. Rank complaints by priority\n"
             << "5. Update complaint status\n"
             << "6. Dashboard\n"
             << "7. Save data\n"
             << "8. Exit\n";

        choice = readInt("Enter choice (1-8): ", 1, 8);

        switch (choice) {
            case 1: addComplaint(); break;
            case 2: displayComplaints(); break;
            case 3: searchComplaint(); break;
            case 4: rankComplaints(); break;
            case 5: updateStatus(); break;
            case 6: showDashboard(); break;
            case 7:
                saveToFile();
                cout << "Data saved.\n";
                break;
            case 8:
                saveToFile();
                cout << "Data saved. Goodbye!\n";
                break;
        }
    } while (choice != 8);

    return 0;
}

