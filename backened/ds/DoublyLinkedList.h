#pragma once

#include <string>
#include <vector>
#include <ctime>

/**
 * Fundamentals of Data Structures: Doubly Linked List (DLL)
 * 
 * Maintains a persistent, bidirectional audit trail of civic complaint
 * lifecycle transitions, dispatch updates, and resolution records.
 */
class ResolutionAuditLog {
public:
    struct AuditNode {
        int complaintId = 0;
        std::string action;       // "Submitted", "Assigned", "Status Changed", "Resolved", "Closed"
        std::string previousStatus;
        std::string newStatus;
        std::string notes;
        std::string officer;
        long long timestamp = 0;

        AuditNode* prev = nullptr;
        AuditNode* next = nullptr;

        AuditNode(int cId, const std::string& act, const std::string& prevSt,
                  const std::string& newSt, const std::string& nts,
                  const std::string& off, long long ts)
            : complaintId(cId), action(act), previousStatus(prevSt),
              newStatus(newSt), notes(nts), officer(off), timestamp(ts),
              prev(nullptr), next(nullptr) {}
    };

    struct AuditRecord {
        int complaintId;
        std::string action;
        std::string previousStatus;
        std::string newStatus;
        std::string notes;
        std::string officer;
        long long timestamp;
    };

private:
    AuditNode* head = nullptr;
    AuditNode* tail = nullptr;
    size_t count = 0;

public:
    ResolutionAuditLog() : head(nullptr), tail(nullptr), count(0) {}

    ~ResolutionAuditLog() {
        clear();
    }

    ResolutionAuditLog(const ResolutionAuditLog&) = delete;
    ResolutionAuditLog& operator=(const ResolutionAuditLog&) = delete;

    void append(int complaintId, const std::string& action,
                const std::string& prevStatus, const std::string& newStatus,
                const std::string& notes, const std::string& officer) {
        long long now = static_cast<long long>(time(nullptr));
        AuditNode* newNode = new AuditNode(complaintId, action, prevStatus, newStatus, notes, officer, now);

        if (!tail) {
            head = tail = newNode;
        } else {
            tail->next = newNode;
            newNode->prev = tail;
            tail = newNode;
        }
        count++;
    }

    // Retrieve full log in reverse chronological order (newest first)
    std::vector<AuditRecord> getReverseChronologicalLog() const {
        std::vector<AuditRecord> records;
        records.reserve(count);
        AuditNode* curr = tail;

        while (curr) {
            records.push_back({
                curr->complaintId,
                curr->action,
                curr->previousStatus,
                curr->newStatus,
                curr->notes,
                curr->officer,
                curr->timestamp
            });
            curr = curr->prev;
        }
        return records;
    }

    // Retrieve history for a specific complaint ID
    std::vector<AuditRecord> getHistoryForComplaint(int complaintId) const {
        std::vector<AuditRecord> records;
        AuditNode* curr = head;

        while (curr) {
            if (curr->complaintId == complaintId) {
                records.push_back({
                    curr->complaintId,
                    curr->action,
                    curr->previousStatus,
                    curr->newStatus,
                    curr->notes,
                    curr->officer,
                    curr->timestamp
                });
            }
            curr = curr->next;
        }
        return records;
    }

    size_t size() const {
        return count;
    }

    bool empty() const {
        return count == 0;
    }

    void clear() {
        AuditNode* curr = head;
        while (curr) {
            AuditNode* nextNode = curr->next;
            delete curr;
            curr = nextNode;
        }
        head = tail = nullptr;
        count = 0;
    }
};
