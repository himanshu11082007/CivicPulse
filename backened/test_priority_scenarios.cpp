#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <ctime>
#include "Complaint.h"
#include "AiService.h"
#include "ds/MaxHeap.h"

using namespace std;

int main() {
    cout << "=========================================================\n";
    cout << "  CivicPulse AI Priority System: Comprehensive Test Suite \n";
    cout << "=========================================================\n\n";

    // -----------------------------------------------------------------
    // Scenario 1: Same category, different danger levels
    // "electric wire fallen near school" vs "electric wire in empty plot"
    // -----------------------------------------------------------------
    cout << "[Scenario 1] Category identical, different danger levels...\n";
    AiAnalysisResult ai1 = AiService::analyzeComplaint(
        "Electrical hazard", "Near St. Mary School", "An electric wire has fallen near a school where children are walking."
    );
    AiAnalysisResult ai2 = AiService::analyzeComplaint(
        "Electrical hazard", "Sector 4 Empty Plot", "An electric wire has fallen in an empty plot."
    );

    cout << "  School Wire: Safety=" << ai1.safety << ", LocSens=" << ai1.locationSensitivity
         << ", Override=" << ai1.emergencyOverride << ", Reason: " << ai1.reason << "\n";
    cout << "  Plot Wire:   Safety=" << ai2.safety << ", LocSens=" << ai2.locationSensitivity
         << ", Override=" << ai2.emergencyOverride << ", Reason: " << ai2.reason << "\n";

    Complaint c_school;
    c_school.id = 101;
    c_school.type = "Electrical hazard";
    c_school.description = "An electric wire has fallen near a school where children are walking.";
    c_school.safety = ai1.safety;
    c_school.severity = ai1.severity;
    c_school.locationSensitivity = ai1.locationSensitivity;
    c_school.emergencyOverride = ai1.emergencyOverride;
    c_school.people = 200;
    c_school.reportedAt = static_cast<long long>(time(nullptr));
    c_school.aiReason = ai1.reason;
    c_school.aiSource = ai1.source;

    Complaint c_plot;
    c_plot.id = 102;
    c_plot.type = "Electrical hazard";
    c_plot.description = "An electric wire has fallen in an empty plot.";
    c_plot.safety = ai2.safety;
    c_plot.severity = ai2.severity;
    c_plot.locationSensitivity = ai2.locationSensitivity;
    c_plot.emergencyOverride = ai2.emergencyOverride;
    c_plot.people = 5;
    c_plot.reportedAt = static_cast<long long>(time(nullptr));
    c_plot.aiReason = ai2.reason;
    c_plot.aiSource = ai2.source;

    cout << "  Score Near School: " << c_school.priorityScore() << "/100 (" << c_school.priorityLabel() << ")\n";
    cout << "  Score Empty Plot:  " << c_plot.priorityScore() << "/100 (" << c_plot.priorityLabel() << ")\n";
    assert(c_school.priorityScore() > c_plot.priorityScore());
    assert(c_school.hasHigherPriorityThan(c_plot));
    cout << "  -> PASS: School wire correctly outranks empty plot wire!\n\n";

    // -----------------------------------------------------------------
    // Scenario 2: Two complaints both tagged Urgent, but different AI scores
    // -----------------------------------------------------------------
    cout << "[Scenario 2] Both tagged Urgent, different AI scores...\n";
    Complaint c_urg_high;
    c_urg_high.id = 201;
    c_urg_high.urgent = true;
    c_urg_high.safety = 5;
    c_urg_high.severity = 5;
    c_urg_high.locationSensitivity = 5;
    c_urg_high.people = 500;
    c_urg_high.reportedAt = static_cast<long long>(time(nullptr));

    Complaint c_urg_low;
    c_urg_low.id = 202;
    c_urg_low.urgent = true; // Also tagged urgent by user
    c_urg_low.safety = 2;
    c_urg_low.severity = 2;
    c_urg_low.locationSensitivity = 1;
    c_urg_low.people = 10;
    c_urg_low.reportedAt = static_cast<long long>(time(nullptr));

    cout << "  Urgent High Score: " << c_urg_high.priorityScore() << "/100\n";
    cout << "  Urgent Low Score:  " << c_urg_low.priorityScore() << "/100\n";
    assert(c_urg_high.priorityScore() > c_urg_low.priorityScore());
    assert(c_urg_high.hasHigherPriorityThan(c_urg_low));
    cout << "  -> PASS: Urgent tag does not bypass score ordering!\n\n";

    // -----------------------------------------------------------------
    // Scenario 3: Complaint affecting many people
    // -----------------------------------------------------------------
    cout << "[Scenario 3] Complaint affecting many people...\n";
    Complaint c_crowd;
    c_crowd.id = 301;
    c_crowd.safety = 3;
    c_crowd.severity = 3;
    c_crowd.locationSensitivity = 3;
    c_crowd.people = 1000; // 1000 people
    c_crowd.reportedAt = static_cast<long long>(time(nullptr));

    Complaint c_few;
    c_few.id = 302;
    c_few.safety = 3;
    c_few.severity = 3;
    c_few.locationSensitivity = 3;
    c_few.people = 2; // only 2 people
    c_few.reportedAt = static_cast<long long>(time(nullptr));

    cout << "  1000 People score: " << c_crowd.priorityScore() << "/100 (PeopleScore=" << c_crowd.peopleScore() << ")\n";
    cout << "  2 People score:    " << c_few.priorityScore() << "/100 (PeopleScore=" << c_few.peopleScore() << ")\n";
    assert(c_crowd.priorityScore() > c_few.priorityScore());
    cout << "  -> PASS: Crowd impact raises priority score transparently!\n\n";

    // -----------------------------------------------------------------
    // Scenario 4: Sensitive locations (Hospital / School)
    // -----------------------------------------------------------------
    cout << "[Scenario 4] Complaint near hospital / school...\n";
    AiAnalysisResult ai_hosp = AiService::analyzeComplaint(
        "Water leakage", "Main Road outside General Hospital", "Severe pipeline burst flooding ambulance entrance outside Civil Hospital."
    );
    AiAnalysisResult ai_lane = AiService::analyzeComplaint(
        "Water leakage", "Backyard alley", "Small pipe dripping in private backyard lane."
    );
    cout << "  Hospital burst: LocSens=" << ai_hosp.locationSensitivity << ", Score=";
    Complaint c_hosp;
    c_hosp.id = 401;
    c_hosp.safety = ai_hosp.safety;
    c_hosp.severity = ai_hosp.severity;
    c_hosp.locationSensitivity = ai_hosp.locationSensitivity;
    c_hosp.people = 200;
    c_hosp.reportedAt = static_cast<long long>(time(nullptr));

    Complaint c_lane;
    c_lane.id = 402;
    c_lane.safety = ai_lane.safety;
    c_lane.severity = ai_lane.severity;
    c_lane.locationSensitivity = ai_lane.locationSensitivity;
    c_lane.people = 5;
    c_lane.reportedAt = static_cast<long long>(time(nullptr));

    cout << c_hosp.priorityScore() << "/100\n";
    cout << "  Backyard drip:  LocSens=" << ai_lane.locationSensitivity << ", Score=" << c_lane.priorityScore() << "/100\n";
    assert(c_hosp.priorityScore() > c_lane.priorityScore());
    cout << "  -> PASS: Hospital proximity correctly elevates location sensitivity!\n\n";

    // -----------------------------------------------------------------
    // Scenario 5: Two complaints with equal scores (FIFO / Older wins)
    // -----------------------------------------------------------------
    cout << "[Scenario 5] Equal scores, older complaint tie-breaker...\n";
    Complaint c_older;
    c_older.id = 501;
    c_older.safety = 4;
    c_older.severity = 3;
    c_older.locationSensitivity = 3;
    c_older.people = 50;
    c_older.reportedAt = 1000000; // Older timestamp

    Complaint c_newer;
    c_newer.id = 502;
    c_newer.safety = 4;
    c_newer.severity = 3;
    c_newer.locationSensitivity = 3;
    c_newer.people = 50;
    c_newer.reportedAt = 2000000; // Newer timestamp

    assert(c_older.priorityScore() == c_newer.priorityScore());
    assert(c_older.hasHigherPriorityThan(c_newer));
    cout << "  Older ID #" << c_older.id << " vs Newer ID #" << c_newer.id << " (Equal score=" << c_older.priorityScore() << ")\n";
    cout << "  -> PASS: Older complaint gets priority on tie-breaker!\n\n";

    // -----------------------------------------------------------------
    // Scenario 6: Waiting-time score increasing for pending complaints
    // -----------------------------------------------------------------
    cout << "[Scenario 6] Waiting time score increasing for pending complaints...\n";
    long long now = static_cast<long long>(time(nullptr));
    Complaint c_just_in;
    c_just_in.id = 601;
    c_just_in.safety = 3;
    c_just_in.severity = 3;
    c_just_in.locationSensitivity = 3;
    c_just_in.people = 50;
    c_just_in.reportedAt = now; // 0 days

    Complaint c_pending_3days;
    c_pending_3days.id = 602;
    c_pending_3days.safety = 3;
    c_pending_3days.severity = 3;
    c_pending_3days.locationSensitivity = 3;
    c_pending_3days.people = 50;
    c_pending_3days.reportedAt = now - (3 * 86400); // 3 days pending

    Complaint c_pending_10days;
    c_pending_10days.id = 603;
    c_pending_10days.safety = 3;
    c_pending_10days.severity = 3;
    c_pending_10days.locationSensitivity = 3;
    c_pending_10days.people = 50;
    c_pending_10days.reportedAt = now - (10 * 86400); // 10 days pending

    cout << "  Just in (0 days):  WaitingScore=" << c_just_in.waitingScore() << ", TotalScore=" << c_just_in.priorityScore() << "\n";
    cout << "  Pending 3 days:    WaitingScore=" << c_pending_3days.waitingScore() << ", TotalScore=" << c_pending_3days.priorityScore() << "\n";
    cout << "  Pending 10 days:   WaitingScore=" << c_pending_10days.waitingScore() << ", TotalScore=" << c_pending_10days.priorityScore() << "\n";
    assert(c_pending_10days.waitingScore() > c_pending_3days.waitingScore());
    assert(c_pending_3days.waitingScore() > c_just_in.waitingScore());
    assert(c_pending_10days.priorityScore() > c_just_in.priorityScore());
    cout << "  -> PASS: Waiting time score increases as complaint remains pending!\n\n";

    // -----------------------------------------------------------------
    // Scenario 7: Max-Heap insertion, HeapSort, and Resolved disappearance
    // -----------------------------------------------------------------
    cout << "[Scenario 7] Max-Heap ordering and resolved removal...\n";
    MaxHeap heap;
    heap.insert(c_plot);
    heap.insert(c_school);
    heap.insert(c_crowd);
    heap.insert(c_few);

    vector<Complaint> ranked = heap.getPriorityRankedList();
    cout << "  Heap Ranking Order:\n";
    for (size_t i = 0; i < ranked.size(); ++i) {
        cout << "    #" << (i+1) << " ID " << ranked[i].id << " (" << ranked[i].type << ") Score=" << ranked[i].priorityScore() << "\n";
        if (i > 0) {
            assert(ranked[i-1].hasHigherPriorityThan(ranked[i]));
        }
    }
    assert(ranked.front().id == c_school.id); // Highest must be top
    cout << "  Testing resolved removal...\n";
    bool removed = heap.removeById(c_school.id); // Marked resolved
    assert(removed);
    ranked = heap.getPriorityRankedList();
    for (const auto& r : ranked) {
        assert(r.id != c_school.id);
    }
    cout << "  -> PASS: Heap correctly maintains priority order and excludes resolved complaints!\n\n";

    // -----------------------------------------------------------------
    // Scenario 8: Fallback engine when AI API is unavailable
    // -----------------------------------------------------------------
    cout << "[Scenario 8] Fallback scoring when AI service is unavailable...\n";
    AiAnalysisResult fb = AiService::evaluateRuleBasedFallback(
        "Pothole", "Station Road", "Deep crater on busy road causing two-wheeler accidents."
    );
    cout << "  Fallback Source: " << fb.source << "\n";
    cout << "  Safety=" << fb.safety << ", Severity=" << fb.severity
         << ", LocSens=" << fb.locationSensitivity << ", Override=" << fb.emergencyOverride << "\n";
    cout << "  Reason: " << fb.reason << "\n";
    assert(fb.source == "Rule-Based Fallback");
    assert(fb.safety >= 4);
    assert(!fb.reason.empty());
    cout << "  -> PASS: Fallback engine operates deterministically without pretending to be AI!\n\n";

    // -----------------------------------------------------------------
    // Scenario 9: Persistence and server restart fidelity
    // -----------------------------------------------------------------
    cout << "[Scenario 9] Persistence across serialization (restart simulation)...\n";
    string serialized = c_school.serialize();
    Complaint reloaded = Complaint::deserialize(serialized);

    cout << "  Original: ID=" << c_school.id << ", Safety=" << c_school.safety << ", Score=" << c_school.priorityScore()
         << ", Reason=" << c_school.aiReason << ", Source=" << c_school.aiSource << "\n";
    cout << "  Reloaded: ID=" << reloaded.id << ", Safety=" << reloaded.safety << ", Score=" << reloaded.priorityScore()
         << ", Reason=" << reloaded.aiReason << ", Source=" << reloaded.aiSource << "\n";

    assert(reloaded.id == c_school.id);
    assert(reloaded.safety == c_school.safety);
    assert(reloaded.locationSensitivity == c_school.locationSensitivity);
    assert(reloaded.emergencyOverride == c_school.emergencyOverride);
    assert(reloaded.aiReason == c_school.aiReason);
    assert(reloaded.aiSource == c_school.aiSource);
    assert(reloaded.priorityScore() == c_school.priorityScore());
    cout << "  -> PASS: All AI fields and priority score persist 100% identically across reload!\n\n";

    cout << "=========================================================\n";
    cout << "  ALL 9 SCENARIO TESTS PASSED SUCCESSFULLY! (100% OK)     \n";
    cout << "=========================================================\n";
    return 0;
}
