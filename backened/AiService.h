#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <windows.h>
#include <winhttp.h>

#include "third_party/nlohmann/json.hpp"

using json = nlohmann::json;

struct AiAnalysisResult {
    int safety = 1;              // 1 to 5 (Weight: 35%)
    int severity = 1;            // 1 to 5 (Weight: 25%)
    int peopleAffected = 1;      // 1 to 5 (Weight: 15%)
    int locationSensitivity = 1; // 1 to 5 (Weight: 15%)
    bool emergencyOverride = false;
    std::string reason = "";
    std::string source = "";
    bool isAiGenerated = false;
};

class AiService {
private:
    static std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    static bool containsAny(const std::string& text, const std::vector<std::string>& keywords) {
        for (const auto& kw : keywords) {
            if (text.find(kw) != std::string::npos) return true;
        }
        return false;
    }

    static std::string getEnv(const std::string& varName) {
        char buf[2048] = {0};
        DWORD len = GetEnvironmentVariableA(varName.c_str(), buf, sizeof(buf));
        if (len > 0 && len < sizeof(buf)) {
            return std::string(buf, len);
        }
        return "";
    }

    static std::wstring toWideString(const std::string& s) {
        if (s.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
        std::wstring ws(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
        if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
        return ws;
    }

    // Native in-process HTTPS POST via WinHttp
    static std::string httpsPost(const std::wstring& host, const std::wstring& path, const std::string& jsonBody, const std::wstring& extraHeaders = L"") {
        std::string response;
        HINTERNET hSession = WinHttpOpen(L"CivicPulse-AI-Engine/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return "";

        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return "";
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        // 5-second timeouts so AI analysis never blocks server
        WinHttpSetTimeouts(hRequest, 3000, 3000, 5000, 5000);

        std::wstring headers = L"Content-Type: application/json\r\n" + extraHeaders;
        BOOL bResults = WinHttpSendRequest(
            hRequest,
            headers.c_str(),
            static_cast<DWORD>(headers.length()),
            (LPVOID)jsonBody.c_str(),
            static_cast<DWORD>(jsonBody.length()),
            static_cast<DWORD>(jsonBody.length()),
            0
        );

        if (bResults) {
            bResults = WinHttpReceiveResponse(hRequest, NULL);
        }

        if (bResults) {
            DWORD dwSize = 0;
            do {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                if (dwSize == 0) break;
                std::string buffer(dwSize, '\0');
                DWORD dwDownloaded = 0;
                if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) {
                    response.append(buffer.c_str(), dwDownloaded);
                }
            } while (dwSize > 0);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

public:
    // Deterministic Rule-Based Fallback: Executed if AI service is offline, key missing, or response invalid.
    static AiAnalysisResult evaluateRuleBasedFallback(const std::string& type, const std::string& location, const std::string& description) {
        AiAnalysisResult res;
        res.isAiGenerated = false;
        res.source = "Rule-Based Fallback";

        std::string fullText = toLower(type + " " + location + " " + description);

        // 1. Safety Risk (1 to 5)
        if (containsAny(fullText, {"electric wire", "fallen wire", "live wire", "high voltage", "electrocution",
                                  "gas leak", "lpg leak", "cylinder leak", "building collapse", "wall collapse",
                                  "deep sinkhole", "open manhole", "fire", "sparking", "toxic fumes", "cave in"})) {
            res.safety = 5;
        } else if (containsAny(fullText, {"pothole", "deep pothole", "pipeline burst", "burst pipe",
                                         "water main", "flooded road", "waterlogging", "exposed wire",
                                         "dangling wire", "manhole cover missing", "missing cover", "fallen tree"})) {
            res.safety = 4;
        } else if (containsAny(fullText, {"broken streetlight", "dark street", "damaged footpath",
                                         "slippery", "water leakage", "continuous leak", "garbage accumulation", "dumpster overflow"})) {
            res.safety = 3;
        } else if (containsAny(fullText, {"footpath", "garbage", "trash", "stench", "smell", "pavement", "curb"})) {
            res.safety = 2;
        } else {
            res.safety = 1;
        }

        // 2. Issue Severity (1 to 5)
        if (containsAny(fullText, {"electric wire", "fallen wire", "live wire", "burst", "collapsed", "collapse", "massive", "disaster", "exploded", "submerged", "critical"})) {
            res.severity = (res.safety == 5) ? 4 : 5;
        } else if (containsAny(fullText, {"severe", "deep", "heavy", "blocking", "overflowing", "flooding", "sparking", "broken pipe"})) {
            res.severity = 4;
        } else if (containsAny(fullText, {"broken", "damaged", "leaking", "leakage", "cracked", "uncollected", "faulty"})) {
            res.severity = 3;
        } else if (containsAny(fullText, {"slow", "minor", "small", "faded", "uneven", "dirty"})) {
            res.severity = 2;
        } else {
            res.severity = 1;
        }

        // 3. Location Sensitivity (1 to 5)
        if (containsAny(fullText, {"school", "college", "kindergarten", "nursery", "daycare", "hospital",
                                  "clinic", "dispensary", "nursing home", "children", "kids", "students", "patients"})) {
            res.locationSensitivity = 5; // Highest sensitive vulnerability
        } else if (containsAny(fullText, {"market", "station", "bus stand", "bus stop", "railway",
                                         "metro", "bazaar", "shopping", "chowk", "junction"})) {
            res.locationSensitivity = 4; // High public transit / commerce
        } else if (containsAny(fullText, {"main road", "highway", "avenue", "busy road", "boulevard", "flyover"})) {
            res.locationSensitivity = 3; // Arterial traffic
        } else if (containsAny(fullText, {"empty plot", "vacant plot", "plot", "open plot", "open land", "isolated", "remote"})) {
            res.locationSensitivity = 1; // Isolated / low density
        } else {
            res.locationSensitivity = 2; // Standard residential colony
        }

        // 4. People Affected (1 to 5)
        if (containsAny(fullText, {"metro", "railway", "station", "bus stand", "bazaar", "market", "highway", "crowded", "thousands"})) {
            res.peopleAffected = 5;
        } else if (res.locationSensitivity >= 4 || containsAny(fullText, {"school", "college", "hospital", "children", "students", "main road", "commercial", "pedestrians"})) {
            res.peopleAffected = 4;
        } else if (containsAny(fullText, {"apartment", "society", "colony", "residential", "lane", "street", "park"})) {
            res.peopleAffected = 3;
        } else if (containsAny(fullText, {"empty plot", "vacant plot", "plot", "isolated", "remote"})) {
            res.peopleAffected = 1;
        } else {
            res.peopleAffected = 2;
        }

        // 5. Emergency Override
        if (res.safety == 5 && (res.locationSensitivity >= 4 || containsAny(fullText, {"school", "children", "hospital", "kids", "walking", "crowd", "active"}))) {
            res.emergencyOverride = true;
        }

        // 6. Reason Formulation
        std::string reasonText = "Rule-Based Analysis: ";
        if (res.safety == 5 && res.locationSensitivity == 5) {
            reasonText += "Severe danger to human life directly adjacent to educational/child-dense facility.";
        } else if (res.safety >= 4 && res.locationSensitivity == 1) {
            reasonText += "Hazard identified in isolated/empty plot; low pedestrian footfall reduces immediate exposure.";
        } else if (res.safety >= 4) {
            reasonText += "High physical or infrastructure hazard identified requiring prioritized crew dispatch.";
        } else if (res.locationSensitivity >= 4) {
            reasonText += "Issue situated in high-footfall sensitive public area.";
        } else {
            reasonText += "Standard municipal civic report evaluated under default sector safety parameters.";
        }
        res.reason = reasonText;

        return res;
    }

    // Primary AI Analysis Entrypoint
    static AiAnalysisResult analyzeComplaint(const std::string& type, const std::string& location, const std::string& description) {
        std::string geminiKey = getEnv("GEMINI_API_KEY");
        std::string openaiKey = getEnv("OPENAI_API_KEY");
        std::string groqKey = getEnv("GROQ_API_KEY");

        // 1. Try Gemini AI if key is configured
        if (!geminiKey.empty()) {
            try {
                std::string prompt = 
                    "You are CivicPulse AI, an expert municipal emergency and civic hazard evaluation model.\n"
                    "Analyze this civic complaint and output ONLY a single JSON object (no markdown, no backticks, raw JSON only) with integer fields (all scale 1 to 5):\n"
                    "- safety: integer 1-5 (1=negligible risk, 3=moderate hazard, 5=immediate threat to human life like live wire, gas leak, collapse)\n"
                    "- severity: integer 1-5 (1=minor cosmetic, 3=moderate repair, 5=catastrophic infrastructure failure)\n"
                    "- peopleAffected: integer 1-5 (1=1-5 people/empty plot, 2=5-25, 3=25-100, 4=100-500, 5=500+/dense public area)\n"
                    "- locationSensitivity: integer 1-5 (1=isolated/empty plot, 2=residential, 3=busy road, 4=transit/market, 5=school, hospital, kindergarten)\n"
                    "- emergencyOverride: boolean (true ONLY if verified immediate threat to life exists, otherwise false)\n"
                    "- reason: string (1-2 sentence concise explanation of why this priority was assigned)\n\n"
                    "Complaint Type: " + type + "\n"
                    "Location: " + location + "\n"
                    "Description: " + description;

                json reqPayload = {
                    {"contents", json::array({
                        {
                            {"parts", json::array({
                                {{"text", prompt}}
                            })}
                        }
                    })},
                    {"generationConfig", {
                        {"response_mime_type", "application/json"},
                        {"temperature", 0.1}
                    }}
                };

                std::wstring path = L"/v1beta/models/gemini-1.5-flash:generateContent?key=" + toWideString(geminiKey);
                std::string resp = httpsPost(L"generativelanguage.googleapis.com", path, reqPayload.dump());

                if (!resp.empty()) {
                    json parsedResp = json::parse(resp);
                    if (parsedResp.contains("candidates") && !parsedResp["candidates"].empty()) {
                        std::string rawJson = parsedResp["candidates"][0]["content"]["parts"][0]["text"];
                        // Clean markdown fences if any
                        if (rawJson.find("```json") != std::string::npos) {
                            auto s = rawJson.find("```json") + 7;
                            auto e = rawJson.find("```", s);
                            if (e != std::string::npos) rawJson = rawJson.substr(s, e - s);
                        } else if (rawJson.find("```") != std::string::npos) {
                            auto s = rawJson.find("```") + 3;
                            auto e = rawJson.find("```", s);
                            if (e != std::string::npos) rawJson = rawJson.substr(s, e - s);
                        }

                        json aiData = json::parse(rawJson);
                        AiAnalysisResult result;
                        result.safety = std::clamp(aiData.value("safety", 1), 1, 5);
                        result.severity = std::clamp(aiData.value("severity", 1), 1, 5);
                        result.peopleAffected = std::clamp(aiData.value("peopleAffected", 1), 1, 5);
                        result.locationSensitivity = std::clamp(aiData.value("locationSensitivity", 1), 1, 5);
                        result.emergencyOverride = aiData.value("emergencyOverride", false);
                        result.reason = aiData.value("reason", "AI-evaluated civic hazard analysis.");
                        result.source = "Gemini AI (gemini-1.5-flash)";
                        result.isAiGenerated = true;

                        std::cout << "[CivicPulse AI] Successfully analyzed via Gemini AI. Safety: "
                                  << result.safety << ", LocSens: " << result.locationSensitivity << "\n";
                        return result;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[CivicPulse AI Warning] Gemini API call failed: " << e.what() << ". Falling back to rule engine.\n";
            }
        }

        // 2. Try OpenAI API if key is configured
        if (!openaiKey.empty()) {
            try {
                json reqPayload = {
                    {"model", "gpt-4o-mini"},
                    {"messages", json::array({
                        {
                            {"role", "system"},
                            {"content", "You are CivicPulse AI municipal hazard evaluator. Return ONLY a JSON object with integer fields safety (1-5), severity (1-5), peopleAffected (1-5), locationSensitivity (1-5), emergencyOverride (bool), and reason (string)."}
                        },
                        {
                            {"role", "user"},
                            {"content", "Type: " + type + "\nLocation: " + location + "\nDescription: " + description}
                        }
                    })},
                    {"response_format", {{"type", "json_object"}}},
                    {"temperature", 0.1}
                };

                std::wstring authHdr = L"Authorization: Bearer " + toWideString(openaiKey) + L"\r\n";
                std::string resp = httpsPost(L"api.openai.com", L"/v1/chat/completions", reqPayload.dump(), authHdr);

                if (!resp.empty()) {
                    json parsedResp = json::parse(resp);
                    if (parsedResp.contains("choices") && !parsedResp["choices"].empty()) {
                        std::string rawJson = parsedResp["choices"][0]["message"]["content"];
                        json aiData = json::parse(rawJson);
                        AiAnalysisResult result;
                        result.safety = std::clamp(aiData.value("safety", 1), 1, 5);
                        result.severity = std::clamp(aiData.value("severity", 1), 1, 5);
                        result.peopleAffected = std::clamp(aiData.value("peopleAffected", 1), 1, 5);
                        result.locationSensitivity = std::clamp(aiData.value("locationSensitivity", 1), 1, 5);
                        result.emergencyOverride = aiData.value("emergencyOverride", false);
                        result.reason = aiData.value("reason", "OpenAI-evaluated civic hazard analysis.");
                        result.source = "OpenAI (gpt-4o-mini)";
                        result.isAiGenerated = true;

                        std::cout << "[CivicPulse AI] Successfully analyzed via OpenAI.\n";
                        return result;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[CivicPulse AI Warning] OpenAI call failed: " << e.what() << ". Falling back to rule engine.\n";
            }
        }

        // 3. Deterministic Rule-Based Fallback
        std::cout << "[CivicPulse AI] Running Deterministic Rule-Based Fallback (AI API Key Not Set or Offline).\n";
        return evaluateRuleBasedFallback(type, location, description);
    }
};

