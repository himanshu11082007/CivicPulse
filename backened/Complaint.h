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
    int safety = 1;     // 1 to 5 (Weight: 35%)
    int severity = 1;   // 1 to 5 (Weight: 25%)
    int people = 1;     // Estimated affected population (Score: 1 to 5, Weight: 15%)
    long long reportedAt = 0;
    bool urgent = false; // Informational urgent flag from user

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

    // AI-Based Smart Priority Parameters
    int locationSensitivity = 1; // 1 to 5 (Weight: 15%)
    bool emergencyOverride = false; // Verified life-threatening emergency override
    std::string aiReason = "Standard civic evaluation.";
    std::string aiSource = "Rule-Based Fallback";

    // Calculate affected population score (1-5)
    int affectedScore() const {
        if (people <= 5) return 1;
        if (people <= 20) return 2;
        if (people <= 50) return 3;
        if (people <= 200) return 4;
        return 5;
    }

    int peopleScore() const {
        return affectedScore();
    }

    // Calculate waiting days based on elapsed time
    int waitingDays() const {
        if (reportedAt <= 0) return 0;
        long long now = static_cast<long long>(time(nullptr));
        long long elapsed = std::max(0LL, now - reportedAt);
        return static_cast<int>(elapsed / 86400);
    }

    // Waiting score from elapsed days (1 to 5, Weight: 10%)
    // Increases gradually as an unresolved complaint remains pending
    int waitingScore() const {
        if (status == "Resolved" || status == "Closed") {
            if (resolvedAt > reportedAt && reportedAt > 0) {
                int days = static_cast<int>((resolvedAt - reportedAt) / 86400);
                if (days < 1) return 1;
                if (days <= 2) return 2;
                if (days <= 5) return 3;
                if (days <= 9) return 4;
                return 5;
            }
            return 1;
        }
        int days = waitingDays();
        if (days < 1) return 1;  // Day 0: 2.0 pts
        if (days <= 2) return 2; // Day 1-2: 4.0 pts
        if (days <= 5) return 3; // Day 3-5: 6.0 pts
        if (days <= 9) return 4; // Day 6-9: 8.0 pts
        return 5;                // Day 10+: 10.0 pts
    }

    // Transparent AI-Based Priority Scoring Formula (0 - 100):
    // Safety Risk (Weight 35) + Issue Severity (Weight 25) + People Affected (Weight 15)
    // + Location Sensitivity (Weight 15) + Waiting Time (Weight 10)
    int priorityScore() const {
        double safeVal = std::clamp(safety, 1, 5) / 5.0;
        double sevVal = std::clamp(severity, 1, 5) / 5.0;
        double affVal = std::clamp(affectedScore(), 1, 5) / 5.0;
        double locVal = std::clamp(locationSensitivity, 1, 5) / 5.0;
        double waitVal = std::clamp(waitingScore(), 1, 5) / 5.0;

        double rawScore = (safeVal * 35.0) + (sevVal * 25.0) + (affVal * 15.0) + (locVal * 15.0) + (waitVal * 10.0);
        int finalScore = static_cast<int>(std::round(rawScore));
        return std::clamp(finalScore, 0, 100);
    }

    // Priority label
    std::string priorityLabel() const {
        if (emergencyOverride) return "Critical";
        int score = priorityScore();
        if (score >= 80) return "Critical";
        if (score >= 60) return "High";
        if (score >= 40) return "Medium";
        return "Low";
    }

    // Strict Binary Max-Heap Comparator:
    // 1. Explicitly verified life-threatening emergency override
    // 2. Highest AI-calculated priority score descending (urgent tag does not override higher score)
    // 3. Older complaint gets priority on equal score
    // 4. Deterministic tie-breaker by ID
    bool hasHigherPriorityThan(const Complaint& other) const {
        if (emergencyOverride != other.emergencyOverride) {
            return emergencyOverride > other.emergencyOverride;
        }

        int s1 = priorityScore();
        int s2 = other.priorityScore();
        if (s1 != s2) {
            return s1 > s2;
        }

        if (reportedAt != other.reportedAt) {
            return reportedAt < other.reportedAt; // Earlier reported has higher priority
        }

        return id < other.id;
    }

    // Helper to sanitize text fields to prevent line-breaking and delimiter collision
    static std::string sanitizeField(std::string s) {
        for (char& ch : s) {
            if (ch == '|' || ch == '\n' || ch == '\r') {
                ch = ' ';
            }
        }
        return s;
    }

    // Serialize to pipe-delimited string
    std::string serialize() const {
        std::ostringstream ss;
        ss << id << '|'
           << sanitizeField(category) << '|'
           << sanitizeField(type) << '|'
           << sanitizeField(location) << '|'
           << sanitizeField(description) << '|'
           << sanitizeField(status) << '|'
           << safety << '|'
           << severity << '|'
           << people << '|'
           << reportedAt << '|'
           << (urgent ? "1" : "0") << '|'
           << latitude << '|'
           << longitude << '|'
           << photoUrl << '|'
           << resolvedAt << '|'
           << sanitizeField(resolutionNotes) << '|'
           << sanitizeField(assignedTo) << '|'
           << sanitizeField(citizenPhone) << '|'
           << sanitizeField(citizenName) << '|'
           << locationSensitivity << '|'
           << (emergencyOverride ? "1" : "0") << '|'
           << sanitizeField(aiReason) << '|'
           << sanitizeField(aiSource);
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
            try { c.id = std::stoi(f[0]); } catch (...) { c.id = 0; }
            c.category = f[1];
            c.type = f[2];
            c.location = f[3];
            c.description = f[4];
            c.status = f[5];
            try { c.safety = std::stoi(f[6]); } catch (...) { c.safety = 1; }
            try { c.severity = std::stoi(f[7]); } catch (...) { c.severity = 1; }
            try { c.people = std::stoi(f[8]); } catch (...) { c.people = 1; }
            try { c.reportedAt = std::stoll(f[9]); } catch (...) { c.reportedAt = 0; }
            c.urgent = (f[10] == "1");

            if (f.size() > 11 && !f[11].empty()) {
                try { c.latitude = std::stod(f[11]); } catch (...) { c.latitude = 0.0; }
            }
            if (f.size() > 12 && !f[12].empty()) {
                try { c.longitude = std::stod(f[12]); } catch (...) { c.longitude = 0.0; }
            }
            if (f.size() > 13) c.photoUrl = f[13];
            if (f.size() > 14 && !f[14].empty()) {
                try { c.resolvedAt = std::stoll(f[14]); } catch (...) { c.resolvedAt = 0; }
            }
            if (f.size() > 15) c.resolutionNotes = f[15];
            if (f.size() > 16) c.assignedTo = f[16];
            if (f.size() > 17) c.citizenPhone = f[17];
            if (f.size() > 18) c.citizenName = f[18];

            // Backward-compatible load for AI fields (indices 19-22)
            if (f.size() > 19 && !f[19].empty()) {
                try { c.locationSensitivity = std::stoi(f[19]); } catch (...) { c.locationSensitivity = 2; }
            } else {
                c.locationSensitivity = 2;
            }

            if (f.size() > 20 && !f[20].empty()) {
                c.emergencyOverride = (f[20] == "1");
            } else {
                c.emergencyOverride = false;
            }

            if (f.size() > 21 && !f[21].empty()) {
                c.aiReason = f[21];
            } else {
                c.aiReason = "Evaluated via municipal priority rules.";
            }

            if (f.size() > 22 && !f[22].empty()) {
                c.aiSource = f[22];
            } else {
                c.aiSource = "Rule-Based Fallback";
            }
        } else if (f.size() == 9) {
            try { c.id = std::stoi(f[0]); } catch (...) { c.id = 0; }
            c.category = "Roads and transport";
            c.type = f[1];
            c.location = f[2];
            c.description = f[3];
            c.status = (f[4] == "Open") ? "Submitted" : f[4];
            try { c.safety = std::stoi(f[5]); } catch (...) { c.safety = 1; }
            try { c.severity = std::stoi(f[6]); } catch (...) { c.severity = 1; }
            try { c.people = std::max(0, std::stoi(f[7])); } catch (...) { c.people = 1; }
            int days = 0;
            try { days = std::max(0, std::stoi(f[8])); } catch (...) { days = 0; }
            c.reportedAt = static_cast<long long>(time(nullptr)) - static_cast<long long>(days) * 86400;
            c.locationSensitivity = 2;
            c.emergencyOverride = false;
            c.aiReason = "Evaluated via municipal priority rules.";
            c.aiSource = "Rule-Based Fallback";
        }

        return c;
    }
};
