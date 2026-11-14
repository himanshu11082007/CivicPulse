#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <limits>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cmath>

#include "Complaint.h"
#include "ds/MaxHeap.h"
#include "ds/BST.h"
#include "ds/CivicGraph.h"
#include "ds/DoublyLinkedList.h"

using namespace std;

const string FILE_NAME = "complaints.txt";

// Core Fundamentals of Data Structures (FDS)
vector<Complaint> complaints;
MaxHeap priorityHeap;
BST idTree;
CivicGraph cityGraph;
ResolutionAuditLog auditLog;
int nextId = 1;

void syncDataStructures() {
    priorityHeap.clear();
    idTree.clear();
    for (const auto& c : complaints) {
        idTree.insert(c);
        if (c.status != "Resolved" && c.status != "Closed") {
            priorityHeap.insert(c);
        }
    }
}

void saveToFile() {
    ofstream file(FILE_NAME);
    if (!file) {
        cout << "Error: could not save complaints.\n";
        return;
    }
    for (const Complaint& c : complaints) {
        file << c.serialize() << '\n';
    }
}

void loadFromFile() {
    complaints.clear();
    ifstream file(FILE_NAME);
    if (!file) return;

    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;
        try {
            Complaint c = Complaint::deserialize(line);
            if (c.id > 0) {
                complaints.push_back(c);
                nextId = max(nextId, c.id + 1);
            }
        } catch (...) {
            // Ignore corrupted lines
        }
    }
    syncDataStructures();
}

string readText(const string& prompt) {
    string value;
    cout << prompt;
    getline(cin, value);
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
        cout << "Enter a valid number from " << low << " to " << high << ".\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

void addComplaint() {
    Complaint c;
    cout << "\n--- Register Civic Complaint ---\n";
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
        "Roads and transport", "Waste management", "Electricity and lighting",
        "Water and drainage", "Public spaces", "Environment and hazards", "Other"
    };

    c.category = categories[choice - 1];
    c.type = readText("Issue type (e.g. pothole, overflowing bin): ");
    c.location = readText("Location / area: ");
    c.description = readText("Describe the problem: ");

    c.safety = readInt("Immediate safety danger level (1-5): ", 1, 5);
    c.severity = readInt("Issue severity (1-5): ", 1, 5);
    c.people = readInt("Estimated population affected: ", 1, 100000);

    string urgentAns = readText("Flag for urgent attention? (y/n): ");
    c.urgent = (urgentAns == "y" || urgentAns == "Y" || urgentAns == "yes");

    c.id = nextId++;
    c.status = "Submitted";
    c.reportedAt = static_cast<long long>(time(nullptr));

    complaints.push_back(c);
    idTree.insert(c);
    priorityHeap.insert(c);
    auditLog.append(c.id, "Registered", "", "Submitted", "New complaint", "Citizen");
    saveToFile();

    cout << "\nComplaint #" << c.id << " registered successfully!\n";
    cout << "Assigned Priority: " << c.priorityLabel() << " (Score: " << c.priorityScore() << "/100)\n";
}

void displayComplaints() {
    cout << "\n--- All Complaints (In-Order BST Traversal) ---\n";
    auto list = idTree.inOrder();
    if (list.empty()) {
        cout << "No complaints found.\n";
        return;
    }
    for (const Complaint& c : list) {
        cout << "ID: #" << c.id
             << " | " << c.type
             << " | " << c.location
             << " | Status: " << c.status
             << " | Priority: " << c.priorityLabel()
             << " (Score: " << c.priorityScore() << "/100)\n";
    }
}

void searchComplaint() {
    int id = readInt("Enter complaint ID to search: ", 1, numeric_limits<int>::max());
    int comparisons = 0;
    const Complaint* found = idTree.search(id, comparisons);
    if (found) {
        cout << "\n[BST Search Result: Found in " << comparisons << " node comparisons]\n";
        cout << "ID: " << found->id
             << "\nCategory: " << found->category
             << "\nIssue: " << found->type
             << "\nLocation: " << found->location
             << "\nDescription: " << found->description
             << "\nStatus: " << found->status
             << "\nPriority: " << found->priorityLabel()
             << "\nScore: " << found->priorityScore() << "/100\n";
    } else {
        cout << "Complaint not found in BST (checked " << comparisons << " nodes).\n";
    }
}

void showPriorityQueue() {
    cout << "\n--- Complaints Ordered by Binary Max-Heap (HeapSort) ---\n";
    auto ranked = priorityHeap.getPriorityRankedList();
    if (ranked.empty()) {
        cout << "No open complaints in priority queue.\n";
        return;
    }
    int rank = 1;
    for (const Complaint& c : ranked) {
        cout << rank++ << ". #" << c.id
             << " | " << c.type
             << " | " << c.location
             << " | Score: " << c.priorityScore() << "/100"
             << " | Priority: " << c.priorityLabel()
             << " | Status: " << c.status << '\n';
    }
}

void runDijkstraDispatch() {
    cout << "\n--- Municipal Service Crew Shortest Route (Dijkstra) ---\n";
    vector<int> targets;
    auto topList = priorityHeap.getPriorityRankedList();
    for (const auto& c : topList) {
        int node = cityGraph.findNodeByLocation(c.location);
        if (node >= 0 && find(targets.begin(), targets.end(), node) == targets.end()) {
            targets.push_back(node);
        }
        if (targets.size() >= 4) break;
    }

    auto route = cityGraph.calculateDispatchRoute(0, targets);
    cout << "Depot: Municipal Central Depot\nWaypoints:\n";
    for (size_t i = 0; i < route.pathNodeNames.size(); ++i) {
        cout << "  [" << (i + 1) << "] " << route.pathNodeNames[i] << '\n';
    }
    cout << "Total Route Distance: " << route.totalDistanceKm << " km\n";
    cout << "Estimated Travel & Work Duration: " << route.estimatedMinutes << " minutes\n";
}

void updateStatus() {
    int id = readInt("Enter complaint ID: ", 1, numeric_limits<int>::max());
    for (Complaint& c : complaints) {
        if (c.id == id) {
            cout << "Current status: " << c.status << '\n'
                 << "1. Submitted\n2. In Progress\n3. Resolved\n4. Closed\n";
            int choice = readInt("Choose new status (1-4): ", 1, 4);
            const vector<string> statuses = {"Submitted", "In Progress", "Resolved", "Closed"};
            string prev = c.status;
            c.status = statuses[choice - 1];
            c.resolutionNotes = readText("Resolution notes: ");
            if (c.status == "Resolved" || c.status == "Closed") {
                c.resolvedAt = static_cast<long long>(time(nullptr));
            }
            auditLog.append(c.id, "Status Transition", prev, c.status, c.resolutionNotes, "Admin");
            syncDataStructures();
            saveToFile();
            cout << "Status updated to " << c.status << ".\n";
            return;
        }
    }
    cout << "Complaint not found.\n";
}

void showFdsMetrics() {
    cout << "\n========== FUNDAMENTALS OF DATA STRUCTURES (FDS) ==========\n";
    cout << "1. Binary Max-Heap:\n";
    cout << "   - Elements: " << priorityHeap.size() << '\n';
    cout << "   - Tree Height: " << (priorityHeap.size() > 0 ? static_cast<int>(floor(log2(priorityHeap.size()))) + 1 : 0) << '\n';
    cout << "   - Insertion Time Complexity: O(log N)\n";
    cout << "   - Extract Max Complexity: O(log N)\n";
    cout << "2. Binary Search Tree (BST):\n";
    cout << "   - Total Nodes: " << idTree.size() << '\n';
    cout << "   - Tree Height: " << idTree.height() << '\n';
    cout << "   - Lookup Complexity: O(log N) average\n";
    cout << "3. Civic Road Network Graph:\n";
    cout << "   - Vertices (Hubs & Intersections): " << cityGraph.getVertexCount() << '\n';
    cout << "   - Undirected Road Edges: " << cityGraph.getEdgeCount() << '\n';
    cout << "   - Algorithm: Dijkstra's Single-Source Shortest Path O((V + E) log V)\n";
    cout << "4. Doubly Linked List:\n";
    cout << "   - Audit Trail Entries: " << auditLog.size() << '\n';
    cout << "===========================================================\n";
}

int main() {
    loadFromFile();

    int choice;
    do {
        cout << "\n========== CIVICPULSE (FDS ENGINE) ==========\n"
             << "1. Register complaint\n"
             << "2. View all complaints (BST In-Order)\n"
             << "3. Search complaint by ID (BST Search)\n"
             << "4. Rank complaints by priority (Max-Heap)\n"
             << "5. Municipal repair crew route (Dijkstra)\n"
             << "6. Update complaint status / resolve\n"
             << "7. Fundamentals of Data Structures metrics\n"
             << "8. Save data\n"
             << "9. Exit\n";

        choice = readInt("Enter choice (1-9): ", 1, 9);
        switch (choice) {
            case 1: addComplaint(); break;
            case 2: displayComplaints(); break;
            case 3: searchComplaint(); break;
            case 4: showPriorityQueue(); break;
            case 5: runDijkstraDispatch(); break;
            case 6: updateStatus(); break;
            case 7: showFdsMetrics(); break;
            case 8: saveToFile(); cout << "Data saved successfully.\n"; break;
            case 9: saveToFile(); cout << "Data saved. Goodbye!\n"; break;
        }
    } while (choice != 9);

    return 0;
}
