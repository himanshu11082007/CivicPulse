#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <cmath>

struct Complaint {
    int id = 0;
    std::string category;
    std::string type;
    std::string location;
    std::string description;
    std::string status = "Submitted"; // "Submitted", "In Progress", "Resolved", "Closed"
    
    // Risk & Priority Attributes
    int safety = 1;     // 1 to 5
    int severity = 1;   // 1 to 5
    int people = 1;     // Estimated affected population
    long long reportedAt = 0;
    bool urgent = false;

    // Advanced Geotag & Media Attributes
    double latitude = 0.0;
    double longitude = 0.0;
    std::string photoUrl = "";
    long long resolvedAt = 0;
    std::string resolutionNotes = "";
    std::string assignedTo = "";

    // Citizen Ownership & Multi-Tenant Identification
    std::string citizenPhone = "";
    std::string citizenName = "";

    // Calculate affected population score (1-5)
    int affectedScore() const {
        if (people <= 5) return 1;
        if (people <= 20) return 2;
        if (people <= 50) return 3;
        if (people <= 200) return 4;
        return 5;
    }

    // Calculate waiting days based on elapsed time
    int waitingDays() const {
        if (reportedAt <= 0) return 0;
        long long now = static_cast<long long>(time(nullptr));
        long long elapsed = std::max(0LL, now - reportedAt);
        return static_cast<int>(elapsed / 86400);
    }

    // Waiting score from elapsed days
    int waitingScore() const {
        int days = waitingDays();
        if (days < 1) return 1;
        if (days <= 3) return 2;
        if (days <= 7) return 3;
        if (days <= 14) return 4;
        return 5;
    }

    // CivicPulse Priority Formula:
    // Score = 8*Safety + 6*Severity + 4*Affected + 2*Waiting (Max = 100)
    int priorityScore() const {
        return 8 * safety + 6 * severity + 4 * affectedScore() + 2 * waitingScore();
    }

    // Priority label
    std::string priorityLabel() const {
        if (urgent) return "URGENT";
        int score = priorityScore();
        if (score >= 80) return "Critical";
        if (score >= 60) return "High";
        if (score >= 40) return "Medium";
        return "Low";
    }

    // Comparator for priority: urgent first, then score descending, then oldest first
    bool hasHigherPriorityThan(const Complaint& other) const {
        if (urgent != other.urgent) {
            return urgent > other.urgent;
        }
        int s1 = priorityScore();
        int s2 = other.priorityScore();
        if (s1 != s2) {
            return s1 > s2;
        }
        return reportedAt < other.reportedAt; // Earlier reported has higher priority
    }

    // Serialize to pipe-delimited string
    std::string serialize() const {
        std::ostringstream ss;
        ss << id << '|'
           << category << '|'
           << type << '|'
           << location << '|'
           << description << '|'
           << status << '|'
           << safety << '|'
           << severity << '|'
           << people << '|'
           << reportedAt << '|'
           << (urgent ? "1" : "0") << '|'
           << latitude << '|'
           << longitude << '|'
           << photoUrl << '|'
           << resolvedAt << '|'
           << resolutionNotes << '|'
           << assignedTo << '|'
           << citizenPhone << '|'
           << citizenName;
        return ss.str();
    }

    // Deserialize from line with backward compatibility
    static Complaint deserialize(const std::string& line) {
        Complaint c;
        std::vector<std::string> f;
        std::string token;
        std::stringstream ss(line);

        while (std::getline(ss, token, '|')) {
            f.push_back(token);
        }
        if (!line.empty() && line.back() == '|') {
            f.push_back("");
        }

        if (f.size() >= 11) {
            c.id = std::stoi(f[0]);
            c.category = f[1];
            c.type = f[2];
            c.location = f[3];
            c.description = f[4];
            c.status = f[5];
            c.safety = std::stoi(f[6]);
            c.severity = std::stoi(f[7]);
            c.people = std::stoi(f[8]);
            c.reportedAt = std::stoll(f[9]);
            c.urgent = (f[10] == "1");

            if (f.size() > 11 && !f[11].empty()) c.latitude = std::stod(f[11]);
            if (f.size() > 12 && !f[12].empty()) c.longitude = std::stod(f[12]);
            if (f.size() > 13) c.photoUrl = f[13];
            if (f.size() > 14 && !f[14].empty()) c.resolvedAt = std::stoll(f[14]);
            if (f.size() > 15) c.resolutionNotes = f[15];
            if (f.size() > 16) c.assignedTo = f[16];
            if (f.size() > 17) c.citizenPhone = f[17];
            if (f.size() > 18) c.citizenName = f[18];
        } else if (f.size() == 9) {
            c.id = std::stoi(f[0]);
            c.category = "Roads and transport";
            c.type = f[1];
            c.location = f[2];
            c.description = f[3];
            c.status = (f[4] == "Open") ? "Submitted" : f[4];
            c.safety = std::stoi(f[5]);
            c.severity = std::stoi(f[6]);
            c.people = std::max(0, std::stoi(f[7]));
            int days = std::max(0, std::stoi(f[8]));
            c.reportedAt = static_cast<long long>(time(nullptr)) - static_cast<long long>(days) * 86400;
        }

        return c;
    }
};
