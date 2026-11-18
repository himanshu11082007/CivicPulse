#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <limits>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <atomic>
#include <sstream>
#include <mutex>
#include <thread>
#include <cmath>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "Complaint.h"
#include "AiService.h"
#include "ds/MaxHeap.h"
#include "ds/BST.h"
#include "ds/CivicGraph.h"
#include "ds/DoublyLinkedList.h"

#include "third_party/httplib/httplib.h"
#include "third_party/nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

const string COMPLAINTS_FILE = "complaints.txt";
const string USERS_FILE = "users.txt";

// Helper to detect actual local LAN IPv4 address on Windows
std::string getLocalIpAddress() {
    std::string detectedIp = "192.168.0.101"; // Default fallback
#ifdef _WIN32
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock != INVALID_SOCKET) {
        sockaddr_in loopback;
        memset(&loopback, 0, sizeof(loopback));
        loopback.sin_family = AF_INET;
        loopback.sin_addr.s_addr = inet_addr("8.8.8.8");
        loopback.sin_port = htons(53);
        if (connect(sock, (sockaddr*)&loopback, sizeof(loopback)) == 0) {
            sockaddr_in name;
            int nameLen = sizeof(name);
            if (getsockname(sock, (sockaddr*)&name, &nameLen) == 0) {
                char buf[64] = {0};
                const char* p = inet_ntop(AF_INET, &name.sin_addr, buf, sizeof(buf));
                if (p && std::string(p) != "127.0.0.1" && std::string(p) != "0.0.0.0") {
                    detectedIp = p;
                }
            }
        }
        closesocket(sock);
    }
#endif
    return detectedIp;
}

// ---------- USER MODEL ----------

struct UserAccount {
    string phone;
    string name;
    string password;
    string role = "citizen"; // "citizen" | "admin"
    long long registeredAt = 0;

    string serialize() const {
        return phone + '|' + name + '|' + password + '|' + role + '|' + to_string(registeredAt);
    }

    static UserAccount deserialize(const string& line) {
        UserAccount u;
        stringstream ss(line);
        string token;
        vector<string> parts;
        while (getline(ss, token, '|')) {
            parts.push_back(token);
        }
        if (parts.size() >= 4) {
            u.phone = parts[0];
            u.name = parts[1];
            u.password = parts[2];
            u.role = parts[3];
            if (parts.size() >= 5) {
                u.registeredAt = stoll(parts[4]);
            }
        }
        return u;
    }
};

// Concurrency and Server Management
std::mutex dataMutex;
httplib::Server apiServer;
std::atomic<bool> serverRunning{false};

// Core Data Structures (Fundamentals of Data Structures)
std::vector<Complaint> allComplaints;
std::vector<UserAccount> allUsers;
MaxHeap priorityHeap;             // Binary Max-Heap for priority queue
BST idIndex;                      // Binary Search Tree for O(log n) lookup
CivicGraph cityGraph;             // Graph with Adjacency List for Dijkstra route dispatch
ResolutionAuditLog auditTrail;    // Doubly Linked List for resolution history
int nextId = 1;

// ---------- USERS FILE PERSISTENCE ----------

void loadUsersFromFile() {
    allUsers.clear();
    ifstream file(USERS_FILE);
    if (!file) {
        // Create initial default admin
        UserAccount admin;
        admin.phone = "admin";
        admin.name = "Municipal Administrator";
        admin.password = "admin123";
        admin.role = "admin";
        admin.registeredAt = static_cast<long long>(time(nullptr));
        allUsers.push_back(admin);
        return;
    }

    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;
        try {
            UserAccount u = UserAccount::deserialize(line);
            if (!u.phone.empty()) {
                allUsers.push_back(u);
            }
        } catch (...) {}
    }
}

void saveUsersToFile() {
    ofstream file(USERS_FILE);
    if (!file) return;
    for (const auto& u : allUsers) {
        file << u.serialize() << '\n';
    }
}

// ---------- COMPLAINT SERIALIZERS ----------

json complaintToJson(const Complaint& c) {
    return {
        {"id", c.id},
        {"category", c.category},
        {"type", c.type},
        {"location", c.location},
        {"description", c.description},
        {"status", c.status},
        {"safety", c.safety},
        {"severity", c.severity},
        {"people", c.people},
        {"reportedAt", c.reportedAt},
        {"urgent", c.urgent},
        {"locationSensitivity", c.locationSensitivity},
        {"emergencyOverride", c.emergencyOverride},
        {"priorityScore", c.priorityScore()},
        {"priority", c.priorityLabel()},
        {"aiReason", c.aiReason},
        {"aiSource", c.aiSource},
        {"latitude", c.latitude},
        {"longitude", c.longitude},
        {"photoUrl", c.photoUrl},
        {"resolvedAt", c.resolvedAt},
        {"resolutionNotes", c.resolutionNotes},
        {"assignedTo", c.assignedTo},
        {"citizenPhone", c.citizenPhone},
        {"citizenName", c.citizenName}
    };
}

json getAllComplaintsJson(const string& filterPhone = "") {
    json result = json::array();
    for (const auto& c : allComplaints) {
        if (filterPhone.empty() || c.citizenPhone == filterPhone) {
            result.push_back(complaintToJson(c));
        }
    }
    return result;
}

json getDashboardJson() {
    int total = static_cast<int>(allComplaints.size());
    int attention = 0;
    int inProgress = 0;
    int resolved = 0;

    for (const auto& c : allComplaints) {
        if (c.status == "Submitted" || c.status == "In Progress") {
            if (c.urgent || c.priorityScore() >= 60) {
                attention++;
            }
        }
        if (c.status == "In Progress") {
            inProgress++;
        }
        if (c.status == "Resolved" || c.status == "Closed") {
            resolved++;
        }
    }

    return {
        {"total", total},
        {"attention", attention},
        {"progress", inProgress},
        {"resolved", resolved},
        {"totalCitizens", allUsers.size()}
    };
}

// ---------- PERSISTENCE & FDS SYNC ----------

void syncDataStructures() {
    priorityHeap.clear();
    idIndex.clear();

    for (const auto& c : allComplaints) {
        idIndex.insert(c);
        if (c.status != "Resolved" && c.status != "Closed") {
            priorityHeap.insert(c);
        }
    }
}

void saveComplaintsToFile() {
    ofstream file(COMPLAINTS_FILE);
    if (!file) {
        cerr << "[CivicPulse Error] Unable to open " << COMPLAINTS_FILE << " for writing.\n";
        return;
    }

    for (const auto& c : allComplaints) {
        file << c.serialize() << '\n';
    }
}

void loadComplaintsFromFile() {
    allComplaints.clear();
    auditTrail.clear();

    ifstream file(COMPLAINTS_FILE);
    if (!file) {
        cout << "[CivicPulse Info] complaints.txt initialized cleanly with 0 records.\n";
        return;
    }

    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;
        try {
            Complaint c = Complaint::deserialize(line);
            if (c.id > 0) {
                allComplaints.push_back(c);
                nextId = max(nextId, c.id + 1);
            }
        } catch (const exception& e) {
            cerr << "[CivicPulse Warning] Skipped invalid line: " << e.what() << '\n';
        }
    }

    syncDataStructures();
    cout << "[CivicPulse] Loaded " << allComplaints.size() << " real complaints into FDS engine.\n";
}

// ---------- API ROUTES SETUP ----------

void setupApiRoutes() {
    apiServer.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization"}
    });

    apiServer.Options(R"(/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // Health check
    apiServer.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        std::string ip = getLocalIpAddress();
        json info = {
            {"status", "ok"},
            {"project", "CivicPulse"},
            {"version", "3.0-Production"},
            {"networkIp", ip},
            {"networkUrl", "http://" + ip + ":8080"},
            {"structures", {"Binary Max-Heap", "Binary Search Tree", "Graph (Dijkstra)", "Doubly Linked List"}}
        };
        res.set_content(info.dump(2), "application/json");
    });

    // Dynamic Network IP and Phone Access Info
    apiServer.Get("/api/network-info", [](const httplib::Request&, httplib::Response& res) {
        std::string ip = getLocalIpAddress();
        json info = {
            {"ip", ip},
            {"port", 8080},
            {"url", "http://" + ip + ":8080"}
        };
        res.set_content(info.dump(2), "application/json");
    });

    // ---------- AUTHENTICATION ROUTES ----------

    // Citizen Sign-Up
    apiServer.Post("/api/auth/register", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON payload."})", "application/json");
            return;
        }

        string phone = body.value("phone", "");
        string name = body.value("name", "");
        string password = body.value("password", "");

        if (name.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Please enter your full name."})", "application/json");
            return;
        }
        if (phone.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Please enter your 10-digit mobile number."})", "application/json");
            return;
        }

        bool validDigits = (phone.length() == 10);
        for (char ch : phone) {
            if (!isdigit(static_cast<unsigned char>(ch))) {
                validDigits = false;
                break;
            }
        }
        if (!validDigits) {
            res.status = 400;
            res.set_content(R"({"error":"Please enter a valid 10-digit mobile number (digits only)."})", "application/json");
            return;
        }

        if (password.length() < 4) {
            res.status = 400;
            res.set_content(R"({"error":"Password must be at least 4 characters long."})", "application/json");
            return;
        }

        std::lock_guard<std::mutex> lock(dataMutex);
        for (const auto& u : allUsers) {
            if (u.phone == phone) {
                res.status = 409;
                res.set_content(R"({"error":"This mobile number is already registered. Please log in."})", "application/json");
                return;
            }
        }

        UserAccount newUser;
        newUser.phone = phone;
        newUser.name = name;
        newUser.password = password;
        newUser.role = "citizen";
        newUser.registeredAt = static_cast<long long>(time(nullptr));

        allUsers.push_back(newUser);
        saveUsersToFile();

        json resp = {
            {"success", true},
            {"user", {
                {"phone", newUser.phone},
                {"name", newUser.name},
                {"role", newUser.role},
                {"registeredAt", newUser.registeredAt}
            }}
        };
        res.status = 201;
        res.set_content(resp.dump(2), "application/json");
    });

    // Sign-In (Citizen or Admin)
    apiServer.Post("/api/auth/login", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON payload."})", "application/json");
            return;
        }

        string phone = body.value("phone", "");
        string password = body.value("password", "");
        string role = body.value("role", "citizen");

        std::lock_guard<std::mutex> lock(dataMutex);

        if (role == "admin") {
            if (phone.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"Please enter administrator username."})", "application/json");
                return;
            }
            if (password.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"Please enter administrator password."})", "application/json");
                return;
            }

            if (phone != "admin" && phone != "admin@civicpulse.org") {
                res.status = 404;
                res.set_content(R"({"error":"Administrator account not found."})", "application/json");
                return;
            }

            if (password != "admin123" && password != "1234") {
                res.status = 401;
                res.set_content(R"({"error":"Wrong password. Please enter the correct administrator password or PIN."})", "application/json");
                return;
            }

            json resp = {
                {"success", true},
                {"user", {
                    {"phone", "admin"},
                    {"name", "Municipal Administrator"},
                    {"role", "admin"}
                }}
            };
            res.set_content(resp.dump(2), "application/json");
            return;
        }

        // Citizen Login Validations
        if (phone.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Please enter your 10-digit mobile number."})", "application/json");
            return;
        }

        bool validDigits = (phone.length() == 10);
        for (char ch : phone) {
            if (!isdigit(static_cast<unsigned char>(ch))) {
                validDigits = false;
                break;
            }
        }
        if (!validDigits) {
            res.status = 400;
            res.set_content(R"({"error":"Please enter a valid 10-digit mobile number."})", "application/json");
            return;
        }

        if (password.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Please enter your password."})", "application/json");
            return;
        }

        for (const auto& u : allUsers) {
            if (u.phone == phone && u.role == "citizen") {
                if (u.password != password) {
                    res.status = 401;
                    res.set_content(R"({"error":"Wrong password. Please check your password and try again."})", "application/json");
                    return;
                }
                json resp = {
                    {"success", true},
                    {"user", {
                        {"phone", u.phone},
                        {"name", u.name},
                        {"role", u.role},
                        {"registeredAt", u.registeredAt}
                    }}
                };
                res.set_content(resp.dump(2), "application/json");
                return;
            }
        }

        res.status = 404;
        res.set_content(R"({"error":"Mobile number not registered. Please create a Citizen Account first."})", "application/json");
    });

    // Registered Citizens Directory (Admin Only)
    apiServer.Get("/api/users", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(dataMutex);
        json userList = json::array();

        for (const auto& u : allUsers) {
            if (u.role == "citizen") {
                int count = 0;
                for (const auto& c : allComplaints) {
                    if (c.citizenPhone == u.phone) count++;
                }
                userList.push_back({
                    {"phone", u.phone},
                    {"name", u.name},
                    {"registeredAt", u.registeredAt},
                    {"complaintsCount", count}
                });
            }
        }
        res.set_content(userList.dump(2), "application/json");
    });

    // Get complaints - filtered by citizenPhone if provided, or full list for Admin
    apiServer.Get("/api/complaints", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(dataMutex);
        string phoneFilter = req.has_param("citizenPhone") ? req.get_param_value("citizenPhone") : "";
        res.set_content(getAllComplaintsJson(phoneFilter).dump(2), "application/json");
    });

    // Get single complaint by ID using BST search O(log n)
    apiServer.Get(R"(/api/complaints/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int id = 0;
        try {
            id = stoi(req.matches[1].str());
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid ID format"})", "application/json");
            return;
        }

        std::lock_guard<std::mutex> lock(dataMutex);
        int steps = 0;
        const Complaint* found = idIndex.search(id, steps);

        if (!found) {
            res.status = 404;
            res.set_content(R"({"error":"Complaint not found in BST"})", "application/json");
            return;
        }

        json data = complaintToJson(*found);
        data["fdsLookup"] = {
            {"method", "Binary Search Tree (BST)"},
            {"comparisons", steps},
            {"complexity", "O(log N)"}
        };
        res.set_content(data.dump(2), "application/json");
    });

    // Priority Queue: Ranked strictly by Binary Max-Heap (HeapSort O(N log N)) - Admin Only
    apiServer.Get("/api/priority-queue", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(dataMutex);
        syncDataStructures();
        vector<Complaint> ranked = priorityHeap.getPriorityRankedList();
        json arr = json::array();
        for (const auto& c : ranked) {
            arr.push_back(complaintToJson(c));
        }
        res.set_content(arr.dump(2), "application/json");
    });

    // Dashboard metrics
    apiServer.Get("/api/dashboard", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(dataMutex);
        res.set_content(getDashboardJson().dump(2), "application/json");
    });

    // Register new complaint (Minimal Step 2, automatic location & priority)
    apiServer.Post("/api/complaints", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON payload"})", "application/json");
            return;
        }

        Complaint c;
        c.type = body.value("type", string("General Issue"));
        c.category = body.value("category", string(""));
        if (c.category.empty()) {
            if (c.type == "Pothole" || c.type == "Damaged footpath") c.category = "Roads and transport";
            else if (c.type == "Garbage accumulation") c.category = "Waste management";
            else if (c.type == "Broken streetlight" || c.type == "Electrical hazard") c.category = "Electricity and lighting";
            else if (c.type == "Water leakage") c.category = "Water and drainage";
            else c.category = "Other";
        }

        c.location = body.value("location", string(""));
        c.latitude = body.value("latitude", 0.0);
        c.longitude = body.value("longitude", 0.0);
        if (c.location.empty()) {
            ostringstream locSs;
            locSs << fixed << setprecision(4) << "GPS: " << c.latitude << ", " << c.longitude;
            c.location = locSs.str();
        }

        c.description = body.value("description", string("Reported via photo geotag"));
        c.status = "Submitted";
        c.urgent = body.value("urgent", false);
        c.reportedAt = static_cast<long long>(time(nullptr));
        c.photoUrl = body.value("photoUrl", string(""));
        c.citizenPhone = body.value("citizenPhone", string(""));
        c.citizenName = body.value("citizenName", string("Citizen"));

        // AI-based complaint analysis (Gemini/OpenAI/Groq or deterministic rule-based fallback)
        AiAnalysisResult ai = AiService::analyzeComplaint(c.type, c.location, c.description);
        c.safety = ai.safety;
        c.severity = ai.severity;
        c.locationSensitivity = ai.locationSensitivity;
        c.emergencyOverride = ai.emergencyOverride;
        c.aiReason = ai.reason;
        c.aiSource = ai.source;

        // Map AI peopleAffected (1-5) to people count for peopleScore()
        if (ai.peopleAffected >= 5) c.people = 500;
        else if (ai.peopleAffected == 4) c.people = 200;
        else if (ai.peopleAffected == 3) c.people = 50;
        else if (ai.peopleAffected == 2) c.people = 20;
        else c.people = 5;

        // Allow manual overrides if explicitly supplied in JSON payload (e.g. testing)
        if (body.contains("safety")) c.safety = body["safety"].get<int>();
        if (body.contains("severity")) c.severity = body["severity"].get<int>();
        if (body.contains("locationSensitivity")) c.locationSensitivity = body["locationSensitivity"].get<int>();
        if (body.contains("emergencyOverride")) c.emergencyOverride = body["emergencyOverride"].get<bool>();
        if (body.contains("people")) c.people = body["people"].get<int>();
        if (body.contains("aiReason")) c.aiReason = body["aiReason"].get<string>();

        if (c.emergencyOverride) {
            c.urgent = true;
        }

        {
            std::lock_guard<std::mutex> lock(dataMutex);
            c.id = nextId++;
            allComplaints.push_back(c);
            idIndex.insert(c);
            priorityHeap.insert(c);
            auditTrail.append(c.id, "Registered", "", "Submitted", "Reported by " + c.citizenName + " [AI Priority: " + to_string(c.priorityScore()) + "/100]", c.citizenPhone);
            saveComplaintsToFile();
        }

        res.status = 201;
        res.set_content(complaintToJson(c).dump(2), "application/json");
    });

    // Update status (Submitted -> In Progress -> Resolved -> Closed)
    auto handleStatusUpdate = [](const httplib::Request& req, httplib::Response& res) {
        int id = 0;
        try {
            id = stoi(req.matches[1].str());
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid complaint ID"})", "application/json");
            return;
        }

        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON payload"})", "application/json");
            return;
        }

        string newStatus = body.value("status", string(""));
        string notes = body.contains("notes") ? body.value("notes", string("")) : body.value("resolutionNotes", string(""));
        string officer = body.contains("officer") ? body.value("officer", string("Municipal Official")) : body.value("assignedTo", string("Municipal Official"));

        if (newStatus != "Submitted" && newStatus != "In Progress" &&
            newStatus != "Resolved" && newStatus != "Closed") {
            res.status = 400;
            res.set_content(R"({"error":"Invalid status. Allowed values: Submitted, In Progress, Resolved, Closed"})", "application/json");
            return;
        }

        json resultJson;
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            bool found = false;

            for (auto& c : allComplaints) {
                if (c.id == id) {
                    string prevStatus = c.status;
                    c.status = newStatus;

                    if (!notes.empty()) c.resolutionNotes = notes;
                    if (!officer.empty()) c.assignedTo = officer;
                    if (newStatus == "Resolved" || newStatus == "Closed") {
                        c.resolvedAt = static_cast<long long>(time(nullptr));
                    }

                    auditTrail.append(c.id, "Status Transition", prevStatus, newStatus, notes, officer);
                    syncDataStructures();
                    saveComplaintsToFile();

                    resultJson = complaintToJson(c);
                    found = true;
                    break;
                }
            }

            if (!found) {
                res.status = 404;
                res.set_content(R"({"error":"Complaint not found"})", "application/json");
                return;
            }
        }

        res.set_content(resultJson.dump(2), "application/json");
    };

    apiServer.Patch(R"(/api/complaints/(\d+)/status)", handleStatusUpdate);
    apiServer.Post(R"(/api/complaints/(\d+)/status)", handleStatusUpdate);

    // Delete Complaint
    auto handleDeleteComplaint = [](const httplib::Request& req, httplib::Response& res) {
        int id = 0;
        try {
            id = stoi(req.matches[1].str());
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid complaint ID"})", "application/json");
            return;
        }

        std::lock_guard<std::mutex> lock(dataMutex);
        auto it = std::find_if(allComplaints.begin(), allComplaints.end(), [id](const Complaint& c) {
            return c.id == id;
        });

        if (it == allComplaints.end()) {
            res.status = 404;
            res.set_content(R"({"error":"Complaint not found"})", "application/json");
            return;
        }

        auditTrail.append(id, "Deleted", it->status, "Deleted", "Complaint removed by user", "Municipal Officer");
        allComplaints.erase(it);
        syncDataStructures();
        saveComplaintsToFile();

        json resp = {
            {"success", true},
            {"message", "Complaint deleted successfully"},
            {"deletedId", id}
        };
        res.set_content(resp.dump(2), "application/json");
    };

    apiServer.Delete(R"(/api/complaints/(\d+))", handleDeleteComplaint);
    apiServer.Post(R"(/api/complaints/(\d+)/delete)", handleDeleteComplaint);

    // Dijkstra dispatch route
    apiServer.Get("/api/dispatch-route", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(dataMutex);

        vector<int> targetNodes;
        vector<Complaint> topComplaints = priorityHeap.getPriorityRankedList();

        for (const auto& c : topComplaints) {
            if (c.status != "Resolved" && c.status != "Closed") {
                int node = -1;
                if (c.latitude != 0.0 && c.longitude != 0.0) {
                    node = cityGraph.findNearestNode(c.latitude, c.longitude);
                } else {
                    node = cityGraph.findNodeByLocation(c.location);
                }
                if (node >= 0 && std::find(targetNodes.begin(), targetNodes.end(), node) == targetNodes.end()) {
                    targetNodes.push_back(node);
                }
                if (targetNodes.size() >= 5) break;
            }
        }

        CivicGraph::RouteResult route = cityGraph.calculateDispatchRoute(0, targetNodes);

        json response = {
            {"algorithm", "Dijkstra's Shortest Path & Multi-Stop Dispatch"},
            {"hub", "Municipal Central Depot"},
            {"waypoints", route.pathNodeNames},
            {"totalDistanceKm", std::round(route.totalDistanceKm * 100.0) / 100.0},
            {"estimatedMinutes", route.estimatedMinutes},
            {"stopsCount", targetNodes.size()}
        };

        res.set_content(response.dump(2), "application/json");
    });

    // FDS Live Metrics
    apiServer.Get("/api/fds-metrics", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(dataMutex);

        size_t heapCount = priorityHeap.size();
        int heapDepth = heapCount > 0 ? static_cast<int>(std::floor(std::log2(heapCount))) + 1 : 0;
        int bstHeight = idIndex.height();
        size_t bstCount = idIndex.size();

        json metrics = {
            {"project", "CivicPulse - Fundamentals of Data Structures Implementation"},
            {"maxHeap", {
                {"name", "Binary Max-Heap Priority Queue"},
                {"elements", heapCount},
                {"treeDepth", heapDepth},
                {"insertionComplexity", "O(log N)"},
                {"peekMaxComplexity", "O(1)"},
                {"extractMaxComplexity", "O(log N)"}
            }},
            {"bst", {
                {"name", "Binary Search Tree (ID Index)"},
                {"elements", bstCount},
                {"treeHeight", bstHeight},
                {"searchComplexity", "O(log N) average"},
                {"traversal", "In-Order"}
            }},
            {"graph", {
                {"name", "Adjacency-List Weighted Civic Graph"},
                {"vertexCount", cityGraph.getVertexCount()},
                {"edgeCount", cityGraph.getEdgeCount()},
                {"routingAlgorithm", "Dijkstra's Shortest Path"},
                {"complexity", "O((V + E) log V)"}
            }},
            {"doublyLinkedList", {
                {"name", "Doubly Linked List (Audit Log)"},
                {"recordsCount", auditTrail.size()},
                {"timeComplexityAppend", "O(1)"},
                {"bidirectionalTraversal", true}
            }}
        };

        res.set_content(metrics.dump(2), "application/json");
    });

    // Audit log
    apiServer.Get("/api/audit-log", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(dataMutex);
        auto logs = auditTrail.getReverseChronologicalLog();
        json result = json::array();
        for (const auto& item : logs) {
            result.push_back({
                {"complaintId", item.complaintId},
                {"action", item.action},
                {"previousStatus", item.previousStatus},
                {"newStatus", item.newStatus},
                {"notes", item.notes},
                {"officer", item.officer},
                {"timestamp", item.timestamp}
            });
        }
        res.set_content(result.dump(2), "application/json");
    });

    // Static file serving: HTML, JS, CSS, PWA Manifest, Service Worker
    auto serveFile = [](const string& path, const string& mime, httplib::Response& res) {
        ifstream file(path, ios::binary);
        if (!file) {
            res.status = 404;
            res.set_content("File not found: " + path, "text/plain");
            return;
        }
        string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
        res.set_content(content, mime);
    };

    apiServer.Get("/", [serveFile](const httplib::Request&, httplib::Response& res) {
        serveFile("../index.html", "text/html", res);
    });

    apiServer.Get("/script.js", [serveFile](const httplib::Request&, httplib::Response& res) {
        serveFile("../script.js", "application/javascript", res);
    });

    apiServer.Get("/style.css", [serveFile](const httplib::Request&, httplib::Response& res) {
        serveFile("../style.css", "text/css", res);
    });

    apiServer.Get("/manifest.json", [serveFile](const httplib::Request&, httplib::Response& res) {
        serveFile("../manifest.json", "application/manifest+json", res);
    });

    apiServer.Get("/sw.js", [serveFile](const httplib::Request&, httplib::Response& res) {
        serveFile("../sw.js", "application/javascript", res);
    });
}

// ---------- MAIN ----------

int main() {
    loadUsersFromFile();
    loadComplaintsFromFile();
    setupApiRoutes();

    if (!apiServer.bind_to_port("0.0.0.0", 8080)) {
        cerr << "[CivicPulse Error] Could not bind to 0.0.0.0:8080.\n";
        return 1;
    }

    serverRunning = true;
    std::string localIp = getLocalIpAddress();
    thread serverThread([localIp]() {
        cout << "\n=======================================================\n"
             << "  * CivicPulse Real-Life Multi-User Engine ACTIVE\n"
             << "  * Local Access:   http://127.0.0.1:8080\n"
             << "  * Phone Access:   http://" << localIp << ":8080\n"
             << "=======================================================\n";
        apiServer.listen_after_bind();
    });

    cout << "\n[CivicPulse Server Running] Press Enter to exit server.\n";
    string line;
    getline(cin, line);

    apiServer.stop();
    if (serverThread.joinable()) {
        serverThread.join();
    }

    return 0;
}
