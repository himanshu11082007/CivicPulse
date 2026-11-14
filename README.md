# CivicPulse: Smart Geotagged Civic Issue & FDS Resolution Platform

An advanced course project engineered for **Fundamentals of Data Structures (FDS in C++)**. CivicPulse transforms civic complaints from manual reporting to an automated, geotagged photo capture and prioritization pipeline backed by custom C++ data structures and municipal fleet dispatch routing.

---

## 🌟 Key Features

1. **📷 Smart Geotag Photo Pipeline**:
   - **Binary EXIF GPS Extraction**: Reads embedded latitude and longitude directly from uploaded JPEG photos.
   - **HTML5 Camera & Geolocation**: Live camera snapshot stream with automatic device GPS coordinate capture.
   - **1-Click Test Presets**: Instant real-world geotag presets (College Gate Pothole, Hospital Water Leak, Market Garbage, School Streetlight, Highway Electrical Hazard) for immediate testing.
   - **Civic AI & Parameter Inference**: Automatically infers category, issue type, location landmark, safety hazard level (1–5), severity (1–5), affected population, and priority score.
   - **Full User Control**: All auto-detected parameters can be reviewed, edited, or overridden by the user prior to final submission.

2. **⚙️ Fundamentals of Data Structures (C++ Backend)**:
   - **Binary Max-Heap (`backened/ds/MaxHeap.h`)**:
     - Complete binary tree represented as a dynamic array.
     - $O(\log N)$ insertion (`heapifyUp`) and $O(\log N)$ extraction (`heapifyDown`).
     - $O(1)$ peek at highest-priority civic issue.
     - Dynamic HeapSort returning ranked priority queues.
   - **Binary Search Tree (`backened/ds/BST.h`)**:
     - Indexes complaints uniquely by complaint ID.
     - $O(\log N)$ average search time with real-time comparison step counting.
     - In-order traversal generating sorted ID listings.
   - **Civic Graph & Dijkstra Algorithm (`backened/ds/CivicGraph.h`)**:
     - Weighted undirected graph with Adjacency List representation.
     - Dijkstra's single-source shortest path algorithm ($O((V + E) \log V)$).
     - Multi-stop greedy TSP dispatch routing for municipal repair crews departing from Central Depot.
   - **Doubly Linked List (`backened/ds/DoublyLinkedList.h`)**:
     - Bidirectional resolution history and status transition audit log.
     - $O(1)$ insertion at head/tail.

3. **🗺️ Interactive Map & Fleet Dispatch**:
   - Leaflet + OpenStreetMap integration (zero external API keys needed).
   - Real-time priority pins (Red: Critical, Orange: High, Blue: Medium, Green: Low, Purple: Depot).
   - Overlay polyline showing Dijkstra optimal service routes.

4. **🛡️ Role-Based Access Control**:
   - **Citizen Mode**: Photo capture, geotag extraction, complaint reporting, and tracking.
   - **Municipal Officer Mode**: Priority triage, technician assignment, resolution notes logging, and FDS inspection.

5. **⚗️ FDS Live Inspector View**:
   - Real-time academic visualizer in the web UI demonstrating internal heap arrays, tree heights, Dijkstra vertex metrics, and interactive BST search benchmarks.

---

## 📁 Project Architecture

```
CivicPulse/
├── run_server.bat              # One-click server launcher & browser opener
├── index.html                  # Responsive modern web UI (Leaflet map, dropzone, FDS inspector)
├── script.js                   # EXIF GPS parser, camera snapshot, Leaflet routing, FDS client
├── style.css                   # CivicPulse green-and-white theme, animations, and modal styles
└── backened/
    ├── build.bat               # Windows batch compiler using GCC 14.2
    ├── Complaint.h             # Core complaint data model & priority formula
    ├── main_api.cpp            # Multi-threaded C++ HTTP server + FDS engine + CLI
    ├── main.cpp                # Standalone interactive C++ console CLI
    ├── complaints.txt          # Persistent backward-compatible database
    └── ds/
        ├── MaxHeap.h           # Custom Binary Max-Heap (Priority Queue)
        ├── BST.h               # Custom Binary Search Tree (ID Indexing)
        ├── CivicGraph.h        # Custom Adjacency List Graph + Dijkstra Shortest Path
        └── DoublyLinkedList.h  # Custom Doubly Linked List (Resolution Audit Trail)
```

---

## 🚀 How to Run the Project

### Option A: Complete Web Platform + C++ Backend (Recommended)

1. Double-click **`run_server.bat`** (or open PowerShell in the project root and run `.\run_server.bat`).
2. The batch script will compile `main_api.exe` with GCC 14 (if not already built) and automatically open:
   ```
   http://127.0.0.1:8080
   ```
3. You can interact with the web interface while the C++ console menu remains available in the terminal!

### Option B: Standalone C++ Console CLI (For Lab / Terminal Viva)

1. Open PowerShell in `backened/`:
   ```powershell
   cd backened
   .\main.exe
   ```
2. Interact with the menu:
   - `1`: Register complaint
   - `2`: View all complaints (BST in-order traversal)
   - `3`: Search complaint by ID (BST $O(\log N)$ search with comparison count)
   - `4`: View top priority issue (Max-Heap $O(1)$ root)
   - `5`: Rank priority queue (HeapSort)
   - `6`: Run Dijkstra dispatch route for municipal service crew
   - `7`: Display FDS operational metrics
   - `8`: Resolve complaint with notes

---

## 🧪 Testing the Geotag Photo Features

1. In the Web App, navigate to **📷 Report with Geotag**.
2. Click any of the **1-Click Test Presets** (e.g. *🕳 Road Pothole (College Gate)* or *💧 Water Pipeline Leak (Hospital)*):
   - Notice the real GPS coordinates (`18.5280, 73.8570`) and photo preview appear instantly.
   - The Civic AI Detection Card immediately infers the hazard level, severity, crowd exposure, and priority score.
3. Review or modify any value in **Step 2: Review & Confirm Details** (user-controlled override).
4. Click **Confirm & Submit Civic Complaint**.
5. Switch to **⚑ Priority queue** to see the complaint ranked by the C++ Max-Heap.
6. Click **🚐 Calculate Dijkstra Municipal Service Route** to see the shortest repair path calculated from the Central Depot!
7. Switch to **🗺 Interactive Map** to view the live pin and polyline route.
8. Switch role to **Municipal Officer**, click **Update / Resolve**, input technician resolution notes, and submit.
9. Check **✓ Resolved issues** and **⚗ FDS Live Inspector** to see the Doubly Linked List audit trail!
