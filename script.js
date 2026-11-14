/**
 * CivicPulse - Real-Life Multi-Tenant Civic Resolution App
 * Fully Interactive Authentication, Photo Geotagging, and C++ FDS Engine
 */

let allIssues = [];
let citizenIssues = [];
let registeredCitizens = [];
let currentSession = null; // { phone, name, role }

let dashMapInstance = null;
let fullMapInstance = null;
let routePolyline = null;
let cameraStream = null;
let deferredPrompt = null;

const DEPOT_COORDS = [18.5204, 73.8567];

// ---------- INITIALIZATION ----------

document.addEventListener("DOMContentLoaded", () => {
  setupDragAndDrop();
  registerServiceWorker();
  initPwaInstall();
  checkExistingSession();
});

// Check if user already logged in
function checkExistingSession() {
  const saved = localStorage.getItem("civicpulse_session");
  if (saved) {
    try {
      currentSession = JSON.parse(saved);
      enterAppScreen();
      return;
    } catch (e) {
      console.warn("Session error", e);
    }
  }
  showAuthScreen();
}

function showAuthScreen() {
  document.getElementById("authScreen")?.classList.remove("hidden");
  document.getElementById("appScreen")?.classList.add("hidden");
}

function enterAppScreen() {
  document.getElementById("authScreen")?.classList.add("hidden");
  document.getElementById("appScreen")?.classList.remove("hidden");

  updateUserInterface();
  if (currentSession.role === "citizen") {
    showView("report");
  } else {
    showView("dashboard");
  }
  refreshData();
}

// Update UI based on active role
function updateUserInterface() {
  const isCitizen = currentSession.role === "citizen";

  document.getElementById("citizenNav")?.classList.toggle("hidden", !isCitizen);
  document.getElementById("adminNav")?.classList.toggle("hidden", isCitizen);
  document.getElementById("mobileBottomNav")?.classList.toggle("hidden", !isCitizen);

  const roleBadge = document.getElementById("roleBadge");
  const userNameDisplay = document.getElementById("userNameDisplay");
  const userPhoneDisplay = document.getElementById("userPhoneDisplay");
  const headerRolePill = document.getElementById("headerRolePill");
  const headerUserName = document.getElementById("headerUserName");
  const sidebarTagline = document.getElementById("sidebarTagline");

  if (isCitizen) {
    if (roleBadge) roleBadge.textContent = "👤 Citizen";
    if (userNameDisplay) userNameDisplay.textContent = currentSession.name;
    if (userPhoneDisplay) userPhoneDisplay.textContent = "+91 " + currentSession.phone;
    if (headerRolePill) {
      headerRolePill.textContent = "Citizen";
      headerRolePill.className = "role-pill citizen-pill";
    }
    if (headerUserName) headerUserName.textContent = currentSession.name;
    if (sidebarTagline) sidebarTagline.textContent = "CITIZEN PORTAL";
  } else {
    if (roleBadge) roleBadge.textContent = "🛡 Municipal Admin";
    if (userNameDisplay) userNameDisplay.textContent = currentSession.name;
    if (userPhoneDisplay) userPhoneDisplay.textContent = "Municipal Corporation";
    if (headerRolePill) {
      headerRolePill.textContent = "Admin";
      headerRolePill.className = "role-pill admin-pill";
    }
    if (headerUserName) headerUserName.textContent = "Admin";
    if (sidebarTagline) sidebarTagline.textContent = "ADMIN WORKSPACE";
  }
}

// ---------- AUTHENTICATION LOGIC ----------

function switchAuthTab(tab) {
  document.querySelectorAll(".auth-tab-btn").forEach(b => b.classList.remove("active"));
  document.getElementById("citizenLoginForm")?.classList.add("hidden");
  document.getElementById("citizenSignupForm")?.classList.add("hidden");
  document.getElementById("adminLoginForm")?.classList.add("hidden");

  if (tab === "citizenLogin") {
    document.getElementById("tabCitizenLogin")?.classList.add("active");
    document.getElementById("citizenLoginForm")?.classList.remove("hidden");
  } else if (tab === "citizenSignup") {
    document.getElementById("tabCitizenSignup")?.classList.add("active");
    document.getElementById("citizenSignupForm")?.classList.remove("hidden");
  } else if (tab === "adminLogin") {
    document.getElementById("tabAdminLogin")?.classList.add("active");
    document.getElementById("adminLoginForm")?.classList.remove("hidden");
  }
}

async function handleCitizenLogin(e) {
  e.preventDefault();
  const phone = document.getElementById("loginPhone").value.trim();
  const password = document.getElementById("loginPassword").value.trim();

  try {
    const res = await fetch("/api/auth/login", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ phone, password, role: "citizen" })
    });

    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.error || "Login failed");
    }

    currentSession = data.user;
    localStorage.setItem("civicpulse_session", JSON.stringify(currentSession));
    showToast(`Welcome back, ${currentSession.name}!`);
    enterAppScreen();
  } catch (err) {
    showToast(err.message || "Login error. Please check your credentials.");
  }
}

async function handleCitizenSignup(e) {
  e.preventDefault();
  const name = document.getElementById("signupName").value.trim();
  const phone = document.getElementById("signupPhone").value.trim();
  const password = document.getElementById("signupPassword").value.trim();

  try {
    const res = await fetch("/api/auth/register", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name, phone, password })
    });

    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.error || "Sign-up failed");
    }

    currentSession = data.user;
    localStorage.setItem("civicpulse_session", JSON.stringify(currentSession));
    showToast(`Account created successfully! Welcome, ${currentSession.name}.`);
    enterAppScreen();
  } catch (err) {
    showToast(err.message || "Sign-up error. Phone may already be registered.");
  }
}

async function handleAdminLogin(e) {
  e.preventDefault();
  const username = document.getElementById("adminUsername").value.trim();
  const password = document.getElementById("adminPassword").value.trim();

  try {
    const res = await fetch("/api/auth/login", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ phone: username, password, role: "admin" })
    });

    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.error || "Invalid admin credentials");
    }

    currentSession = data.user;
    localStorage.setItem("civicpulse_session", JSON.stringify(currentSession));
    showToast("🛡 Authorized: Entered Municipal Officer Workspace");
    enterAppScreen();
  } catch (err) {
    showToast(err.message || "Invalid Admin login");
  }
}

function logout() {
  currentSession = null;
  localStorage.removeItem("civicpulse_session");
  showToast("Logged out successfully.");
  showAuthScreen();
}

// ---------- FETCH & REFRESH DATA ----------

async function refreshData() {
  if (!currentSession) return;

  if (currentSession.role === "citizen") {
    await loadCitizenIssues();
    renderCitizenComplaints();
  } else {
    await loadAdminAllIssues();
    await loadRegisteredCitizens();
    renderAdminViews();
    setTimeout(() => initLeafletMaps(), 200);
  }
}

async function loadCitizenIssues() {
  try {
    const res = await fetch(`/api/complaints?citizenPhone=${encodeURIComponent(currentSession.phone)}`);
    if (!res.ok) throw new Error("HTTP error " + res.status);
    const data = await res.json();
    citizenIssues = data.map(formatComplaintObject);
  } catch (err) {
    console.debug("Fetch citizen error:", err);
    citizenIssues = [];
  }
}

async function loadAdminAllIssues() {
  try {
    const res = await fetch("/api/complaints");
    if (!res.ok) throw new Error("HTTP error " + res.status);
    const data = await res.json();
    allIssues = data.map(formatComplaintObject);
  } catch (err) {
    console.debug("Fetch all issues error:", err);
    allIssues = [];
  }
}

async function loadRegisteredCitizens() {
  try {
    const res = await fetch("/api/users");
    if (!res.ok) throw new Error("Users fetch error");
    registeredCitizens = await res.json();
    renderCitizensDirectory();
  } catch (err) {
    console.debug("Load users error:", err);
    registeredCitizens = [];
  }
}

function formatComplaintObject(c) {
  return {
    id: c.id,
    category: c.category,
    type: c.type,
    location: c.location,
    description: c.description,
    status: c.status,
    safety: c.safety,
    condition: c.severity,
    people: c.people,
    date: new Date(c.reportedAt * 1000).toISOString(),
    urgent: c.urgent,
    latitude: c.latitude || 18.5204,
    longitude: c.longitude || 73.8567,
    photoUrl: c.photoUrl,
    resolvedAt: c.resolvedAt,
    resolutionNotes: c.resolutionNotes || "",
    assignedTo: c.assignedTo || "",
    citizenPhone: c.citizenPhone || "",
    citizenName: c.citizenName || "Citizen",
    backendPriority: c.priority,
    backendPriorityScore: c.priorityScore
  };
}

// ---------- PHOTO CAPTURE & AUTOMATIC GEOTAG (COMPULSORY STEP 1) ----------

function setupDragAndDrop() {
  const dropzone = document.getElementById("dropzone");
  if (!dropzone) return;

  ["dragenter", "dragover"].forEach(event => {
    dropzone.addEventListener(event, e => {
      e.preventDefault();
      dropzone.classList.add("dragover");
    });
  });

  ["dragleave", "drop"].forEach(event => {
    dropzone.addEventListener(event, e => {
      e.preventDefault();
      dropzone.classList.remove("dragover");
    });
  });

  dropzone.addEventListener("drop", e => {
    if (e.dataTransfer.files.length) {
      processSelectedFile(e.dataTransfer.files[0]);
    }
  });
}

function handleFileSelect(event) {
  if (event.target.files.length) {
    processSelectedFile(event.target.files[0]);
  }
}

async function processSelectedFile(file) {
  if (!file || !file.type.startsWith("image/")) {
    showToast("Please choose a valid image file.");
    return;
  }

  showToast("Reading photo metadata & GPS...");

  const reader = new FileReader();
  reader.onload = async e => {
    const dataUrl = e.target.result;
    document.getElementById("previewImg").src = dataUrl;
    document.getElementById("photoUrl").value = dataUrl;

    const coords = await extractExifGps(file);
    let lat = coords ? coords.latitude : null;
    let lng = coords ? coords.longitude : null;

    if (!lat || !lng) {
      // Query device GPS via HTML5
      if ("geolocation" in navigator) {
        navigator.geolocation.getCurrentPosition(
          pos => applyExtractedGeotag(pos.coords.latitude, pos.coords.longitude, dataUrl),
          () => applyExtractedGeotag(18.5280, 73.8570, dataUrl),
          { timeout: 3500 }
        );
      } else {
        applyExtractedGeotag(18.5280, 73.8570, dataUrl);
      }
    } else {
      applyExtractedGeotag(lat, lng, dataUrl);
    }
  };
  reader.readAsDataURL(file);
}

// Binary EXIF GPS Parser
async function extractExifGps(file) {
  try {
    const arrayBuffer = await file.arrayBuffer();
    const dataView = new DataView(arrayBuffer);
    if (dataView.getUint16(0) !== 0xFFD8) return null;

    let offset = 2;
    const length = dataView.byteLength;

    while (offset < length) {
      const marker = dataView.getUint16(offset);
      offset += 2;
      if (marker === 0xFFE1) {
        offset += 2;
        const exifHeader = String.fromCharCode(
          dataView.getUint8(offset), dataView.getUint8(offset + 1),
          dataView.getUint8(offset + 2), dataView.getUint8(offset + 3)
        );
        if (exifHeader !== "Exif") return null;

        const tiffOffset = offset + 6;
        const littleEndian = dataView.getUint16(tiffOffset) === 0x4949;
        const ifd0Offset = tiffOffset + dataView.getUint32(tiffOffset + 4, littleEndian);
        const entries = dataView.getUint16(ifd0Offset, littleEndian);

        let gpsOffset = 0;
        for (let i = 0; i < entries; i++) {
          const entryOffset = ifd0Offset + 2 + i * 12;
          const tag = dataView.getUint16(entryOffset, littleEndian);
          if (tag === 0x8825) {
            gpsOffset = tiffOffset + dataView.getUint32(entryOffset + 8, littleEndian);
            break;
          }
        }

        if (gpsOffset > 0) {
          const gpsEntries = dataView.getUint16(gpsOffset, littleEndian);
          let latRef = "N", lonRef = "E";
          let latVals = null, lonVals = null;

          for (let j = 0; j < gpsEntries; j++) {
            const entryOffset = gpsOffset + 2 + j * 12;
            const tag = dataView.getUint16(entryOffset, littleEndian);
            const valOffset = tiffOffset + dataView.getUint32(entryOffset + 8, littleEndian);

            if (tag === 1) latRef = String.fromCharCode(dataView.getUint8(entryOffset + 8));
            else if (tag === 2) {
              latVals = [
                dataView.getUint32(valOffset, littleEndian) / dataView.getUint32(valOffset + 4, littleEndian),
                dataView.getUint32(valOffset + 8, littleEndian) / dataView.getUint32(valOffset + 12, littleEndian),
                dataView.getUint32(valOffset + 16, littleEndian) / dataView.getUint32(valOffset + 20, littleEndian)
              ];
            } else if (tag === 3) lonRef = String.fromCharCode(dataView.getUint8(entryOffset + 8));
            else if (tag === 4) {
              lonVals = [
                dataView.getUint32(valOffset, littleEndian) / dataView.getUint32(valOffset + 4, littleEndian),
                dataView.getUint32(valOffset + 8, littleEndian) / dataView.getUint32(valOffset + 12, littleEndian),
                dataView.getUint32(valOffset + 16, littleEndian) / dataView.getUint32(valOffset + 20, littleEndian)
              ];
            }
          }

          if (latVals && lonVals) {
            let lat = latVals[0] + latVals[1] / 60 + latVals[2] / 3600;
            let lon = lonVals[0] + lonVals[1] / 60 + lonVals[2] / 3600;
            if (latRef === "S") lat = -lat;
            if (lonRef === "W") lon = -lon;
            return { latitude: lat, longitude: lon };
          }
        }
        return null;
      } else if ((marker & 0xFF00) === 0xFF00) {
        offset += dataView.getUint16(offset);
      } else {
        break;
      }
    }
  } catch (e) {
    console.debug("EXIF parse note:", e);
  }
  return null;
}

// Camera controls
async function toggleCamera() {
  const video = document.getElementById("cameraVideo");
  const snapBtn = document.getElementById("snapBtn");

  if (cameraStream) {
    cameraStream.getTracks().forEach(t => t.stop());
    cameraStream = null;
    video.style.display = "none";
    snapBtn.style.display = "none";
    return;
  }

  try {
    cameraStream = await navigator.mediaDevices.getUserMedia({
      video: { facingMode: "environment" },
      audio: false
    });
    video.srcObject = cameraStream;
    video.style.display = "block";
    snapBtn.style.display = "block";
    showToast("Camera active. Capture photo of the civic issue.");
  } catch (err) {
    console.error("Camera access error:", err);
    showToast("Camera access unavailable. Please use file upload.");
  }
}

function takePhotoSnapshot() {
  const video = document.getElementById("cameraVideo");
  const canvas = document.getElementById("cameraCanvas");
  if (!video || !canvas) return;

  canvas.width = video.videoWidth || 640;
  canvas.height = video.videoHeight || 480;
  const ctx = canvas.getContext("2d");
  ctx.drawImage(video, 0, 0, canvas.width, canvas.height);

  const dataUrl = canvas.toDataURL("image/jpeg", 0.85);
  document.getElementById("previewImg").src = dataUrl;
  document.getElementById("photoUrl").value = dataUrl;

  if (cameraStream) {
    cameraStream.getTracks().forEach(t => t.stop());
    cameraStream = null;
    video.style.display = "none";
    document.getElementById("snapBtn").style.display = "none";
  }

  // Get current device GPS
  if ("geolocation" in navigator) {
    navigator.geolocation.getCurrentPosition(
      pos => applyExtractedGeotag(pos.coords.latitude, pos.coords.longitude, dataUrl),
      () => applyExtractedGeotag(18.5280, 73.8570, dataUrl)
    );
  } else {
    applyExtractedGeotag(18.5280, 73.8570, dataUrl);
  }
}

function applyExtractedGeotag(lat, lng, dataUrl) {
  document.getElementById("dispLat").textContent = lat.toFixed(4);
  document.getElementById("dispLng").textContent = lng.toFixed(4);
  document.getElementById("latitude").value = lat.toFixed(5);
  document.getElementById("longitude").value = lng.toFixed(5);

  const estimatedArea = `GPS Sector [${lat.toFixed(3)}N, ${lng.toFixed(3)}E]`;
  document.getElementById("dispAddress").textContent = estimatedArea;
  document.getElementById("location").value = estimatedArea;

  document.getElementById("photoSuccessCard").classList.remove("hidden");
  showToast("✓ Photo GPS coordinates locked!");

  // Smooth scroll to Step 2
  document.getElementById("reportForm")?.scrollIntoView({ behavior: "smooth" });
}

// ---------- CITIZEN COMPLAINT SUBMISSION ----------

async function submitCitizenComplaint(e) {
  e.preventDefault();

  const photoUrl = document.getElementById("photoUrl").value;
  if (!photoUrl) {
    showToast("⚠️ Step 1 is compulsory: Please take or upload a photo of the issue.");
    document.querySelector(".geotag-capture-card")?.scrollIntoView({ behavior: "smooth" });
    return;
  }

  const type = document.getElementById("type").value;
  const description = document.getElementById("description").value.trim() || `Civic issue reported via mobile camera geotag.`;
  const urgent = document.getElementById("urgent").checked;
  const lat = parseFloat(document.getElementById("latitude").value) || 18.5280;
  const lng = parseFloat(document.getElementById("longitude").value) || 73.8570;
  const location = document.getElementById("location").value || `GPS [${lat.toFixed(3)}, ${lng.toFixed(3)}]`;

  const payload = {
    type,
    location,
    description,
    latitude: lat,
    longitude: lng,
    photoUrl,
    urgent,
    citizenPhone: currentSession.phone,
    citizenName: currentSession.name
  };

  try {
    const res = await fetch("/api/complaints", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });

    if (!res.ok) {
      const err = await res.json().catch(() => ({}));
      throw new Error(err.error || "Failed to submit");
    }

    const created = await res.json();
    showToast(`✓ Complaint #CP-${created.id} submitted to Municipal Corporation!`);

    // Reset Form
    document.getElementById("reportForm").reset();
    document.getElementById("photoSuccessCard").classList.add("hidden");
    document.getElementById("photoUrl").value = "";

    await refreshData();
    showView("myComplaints");
  } catch (err) {
    console.error("Submission error:", err);
    showToast(err.message || "Submission failed. Ensure C++ server is running.");
  }
}

// ---------- CITIZEN MY COMPLAINTS VIEW ----------

function renderCitizenComplaints() {
  const container = document.getElementById("citizenComplaintsList");
  if (!container) return;

  if (citizenIssues.length === 0) {
    container.innerHTML = `
      <div class="empty-state-card">
        <span class="empty-icon">📂</span>
        <h3>No complaints reported yet</h3>
        <p class="muted">You have not submitted any complaints under mobile number <b>+91 ${escapeText(currentSession.phone)}</b>.</p>
        <button class="primary" style="margin-top:14px;" onclick="showView('report')">＋ Report an Issue with Photo</button>
      </div>
    `;
    return;
  }

  const sorted = [...citizenIssues].sort((a, b) => new Date(b.date) - new Date(a.date));

  container.innerHTML = sorted.map(c => {
    const isResolved = c.status === "Resolved" || c.status === "Closed";
    const isInProgress = c.status === "In Progress";
    
    const statusClass = isResolved ? "status-resolved" : isInProgress ? "status-in-progress" : "status-submitted";
    const statusText = isResolved ? "✓ Resolved" : isInProgress ? "⚙ In Progress" : "⏳ Submitted";
    
    const reportedDate = new Date(c.date).toLocaleDateString("en-IN", {
      day: "numeric", month: "short", year: "numeric", hour: "2-digit", minute: "2-digit"
    });

    return `
      <article class="citizen-complaint-card">
        <div class="card-media">
          <img src="${c.photoUrl}" alt="Photo" onclick="openPhotoViewer('${c.photoUrl}')">
          <span class="status-badge ${statusClass}">${statusText}</span>
        </div>
        <div class="card-content">
          <div class="card-header-row">
            <h4>${escapeText(c.type)}</h4>
            <span class="complaint-id">#CP-${c.id}</span>
          </div>
          <p class="card-meta">📍 <b>Location:</b> ${escapeText(c.location)}</p>
          <p class="card-desc">${escapeText(c.description)}</p>
          <small class="muted">Reported on: ${reportedDate}</small>
          
          ${isResolved ? `
            <div class="resolution-feedback-box">
              <strong>✓ Work Completed by Municipal Crew:</strong>
              <p class="small">"${escapeText(c.resolutionNotes || "Issue inspected and resolved.")}"</p>
              ${c.assignedTo ? `<small class="muted">Officer in charge: ${escapeText(c.assignedTo)}</small>` : ""}
            </div>
          ` : isInProgress ? `
            <div class="progress-feedback-box">
              <strong>⚙ Active Field Repair:</strong>
              <p class="small">Assigned Technician: ${escapeText(c.assignedTo || "Ward Maintenance Crew")}</p>
            </div>
          ` : `
            <div class="submitted-feedback-box">
              <small class="muted">Status: Registered. Municipal department is reviewing priority ranking.</small>
            </div>
          `}
        </div>
      </article>
    `;
  }).join("");
}

// ---------- ADMIN VIEWS RENDERING ----------

function renderAdminViews() {
  const isResolved = c => ["Resolved", "Closed"].includes(c.status);
  const isInProgress = c => c.status === "In Progress";
  const openIssues = allIssues.filter(c => !isResolved(c));
  const resolvedIssues = allIssues.filter(isResolved);

  // Stats
  const totalEl = document.getElementById("total");
  const attEl = document.getElementById("attention");
  const progEl = document.getElementById("progress");
  const resEl = document.getElementById("resolved");

  if (totalEl) totalEl.textContent = allIssues.length;
  if (attEl) attEl.textContent = openIssues.filter(c => ["Critical", "High", "URGENT"].includes(c.backendPriority)).length;
  if (progEl) progEl.textContent = allIssues.filter(isInProgress).length;
  if (resEl) resEl.textContent = resolvedIssues.length;

  // Recent rows
  const recentTbody = document.getElementById("recentRows");
  if (recentTbody) {
    const recent = [...allIssues].sort((a, b) => new Date(b.date) - new Date(a.date)).slice(0, 5);
    recentTbody.innerHTML = recent.length
      ? recent.map(c => renderAdminRow(c, "recent")).join("")
      : `<tr><td colspan="7" class="text-center muted">No complaints filed by citizens yet.</td></tr>`;
  }

  renderAdminIssues();

  // Priority queue (HeapSort from backend)
  const queueTbody = document.getElementById("priorityRows");
  if (queueTbody) {
    const queue = [...openIssues].sort((a, b) => {
      if (a.urgent !== b.urgent) return a.urgent ? -1 : 1;
      return (b.backendPriorityScore || 0) - (a.backendPriorityScore || 0);
    });

    queueTbody.innerHTML = queue.length
      ? queue.map((c, i) => renderAdminRow(c, "queue", i)).join("")
      : `<tr><td colspan="8" class="text-center muted">All complaints resolved! Priority queue is clear.</td></tr>`;
  }

  // Resolved rows
  const resTbody = document.getElementById("resolvedRows");
  if (resTbody) {
    resTbody.innerHTML = resolvedIssues.length
      ? resolvedIssues.map(c => renderAdminRow(c, "resolved")).join("")
      : `<tr><td colspan="6" class="text-center muted">No resolved issues yet.</td></tr>`;
  }

  updateMapMarkers();
}

function renderCitizensDirectory() {
  const tbody = document.getElementById("citizenDirectoryRows");
  if (!tbody) return;

  if (registeredCitizens.length === 0) {
    tbody.innerHTML = `<tr><td colspan="4" class="text-center muted">No citizen accounts registered yet.</td></tr>`;
    return;
  }

  tbody.innerHTML = registeredCitizens.map(u => {
    const regDate = u.registeredAt ? new Date(u.registeredAt * 1000).toLocaleDateString() : "Active";
    return `
      <tr>
        <td><b>👤 ${escapeText(u.name)}</b></td>
        <td>+91 ${escapeText(u.phone)}</td>
        <td>${regDate}</td>
        <td><span class="pill-badge pill-green">${u.complaintsCount} complaints</span></td>
      </tr>
    `;
  }).join("");
}

function renderAdminIssues() {
  const query = (document.getElementById("search")?.value || "").toLowerCase();
  const statusFilter = document.getElementById("statusFilter")?.value || "all";

  const filtered = allIssues.filter(c => {
    const match = `${c.id} ${c.type} ${c.category} ${c.location} ${c.citizenName} ${c.citizenPhone}`.toLowerCase().includes(query);
    const matchSt = statusFilter === "all" || c.status === statusFilter;
    return match && matchSt;
  });

  const tbody = document.getElementById("issueRows");
  if (tbody) {
    tbody.innerHTML = filtered.length
      ? filtered.map(c => renderAdminRow(c)).join("")
      : `<tr><td colspan="7" class="text-center muted">No matching complaints found.</td></tr>`;
  }
}

function renderAdminRow(c, mode = "all", rank = 0) {
  const priority = c.backendPriority || "Medium";
  const pill = `<span class="pill ${priority.toLowerCase()}">${escapeText(priority)}</span>`;
  const photo = `<img src="${c.photoUrl}" alt="Photo" class="thumb-img" onclick="openPhotoViewer('${c.photoUrl}')">`;
  const citizen = `<b>${escapeText(c.citizenName)}</b><br><small class="muted">+91 ${escapeText(c.citizenPhone)}</small>`;
  const issueTitle = `<b>${escapeText(c.type)}</b><br><small class="muted">#CP-${c.id} · ${escapeText(c.category)}</small>`;
  const loc = `${escapeText(c.location)}<br><small class="muted">📍 ${Number(c.latitude).toFixed(3)}, ${Number(c.longitude).toFixed(3)}</small>`;
  const statusPill = `<span class="status-pill status-${c.status.toLowerCase().replace(/\s+/g, '-')}">${escapeText(c.status)}</span>`;
  const actionBtn = `<button class="action-btn" onclick="openResolutionModal(${c.id})">Update / Resolve</button>`;

  if (mode === "recent") {
    return `<tr><td>${photo}</td><td>${issueTitle}</td><td>${citizen}</td><td>${loc}</td><td>${c.backendPriorityScore} ${pill}</td><td>${statusPill}</td><td>${actionBtn}</td></tr>`;
  }
  if (mode === "queue") {
    return `<tr><td><b>#${rank + 1}</b></td><td>${photo}</td><td>${issueTitle}</td><td>${citizen}</td><td>${loc}</td><td><b>${c.backendPriorityScore} pts</b> ${pill}</td><td>${statusPill}</td><td>${actionBtn}</td></tr>`;
  }
  if (mode === "resolved") {
    const notes = c.resolutionNotes ? `<p class="res-notes">"${escapeText(c.resolutionNotes)}"</p>` : `<span class="muted">No notes</span>`;
    const officer = c.assignedTo ? `<small class="muted">By: ${escapeText(c.assignedTo)}</small>` : ``;
    return `<tr><td>${photo}</td><td>${issueTitle}</td><td>${citizen}</td><td>${loc}</td><td>${notes}${officer}</td><td>${statusPill}</td></tr>`;
  }

  return `<tr><td>${photo}</td><td>${issueTitle}</td><td>${citizen}</td><td>${loc}</td><td>${c.backendPriorityScore} ${pill}</td><td>${statusPill}</td><td>${actionBtn}</td></tr>`;
}

// ---------- ADMIN RESOLUTION WORKFLOW ----------

function openResolutionModal(id) {
  const c = allIssues.find(x => x.id === id);
  if (!c) return;

  document.getElementById("modalComplaintId").value = c.id;
  document.getElementById("modalTitle").textContent = `Update & Resolve Complaint #CP-${c.id}`;
  document.getElementById("modalIssueDetails").innerHTML = `
    <b>Issue:</b> ${escapeText(c.type)} (${escapeText(c.category)})<br>
    <b>Citizen:</b> ${escapeText(c.citizenName)} (+91 ${escapeText(c.citizenPhone)})<br>
    <b>Location:</b> ${escapeText(c.location)}<br>
    <b>Current Status:</b> <span class="pill">${escapeText(c.status)}</span>
  `;

  document.getElementById("modalStatusSelect").value = c.status;
  document.getElementById("modalOfficer").value = c.assignedTo || "Ward Officer Patil";
  document.getElementById("modalNotes").value = c.resolutionNotes || "";

  document.getElementById("resolutionModal").classList.remove("hidden");
}

function closeModal() {
  document.getElementById("resolutionModal").classList.add("hidden");
}

async function submitResolution(event) {
  event.preventDefault();

  const id = Number(document.getElementById("modalComplaintId").value);
  const status = document.getElementById("modalStatusSelect").value;
  const officer = document.getElementById("modalOfficer").value.trim();
  const notes = document.getElementById("modalNotes").value.trim();

  try {
    const res = await fetch(`/api/complaints/${id}/status`, {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ status, officer, notes })
    });

    if (!res.ok) throw new Error("Status update failed");
    showToast(`✓ Complaint #CP-${id} updated to ${status}. Recorded in FDS audit log.`);
    closeModal();
    await refreshData();
  } catch (err) {
    console.error("Resolution error:", err);
    showToast("Error updating status. Check server connection.");
  }
}

// ---------- LEAFLET MAPS & DIJKSTRA ROUTE ----------

function initLeafletMaps() {
  if (typeof L === "undefined") return;

  const fullContainer = document.getElementById("fullMap");
  if (fullContainer && !fullMapInstance) {
    fullMapInstance = L.map("fullMap").setView(DEPOT_COORDS, 13);
    L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", { maxZoom: 19 }).addTo(fullMapInstance);
  }

  const dashContainer = document.getElementById("dashMap");
  if (dashContainer && !dashMapInstance) {
    dashMapInstance = L.map("dashMap").setView(DEPOT_COORDS, 13);
    L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", { maxZoom: 19 }).addTo(dashMapInstance);
  }

  updateMapMarkers();
}

function updateMapMarkers() {
  const maps = [dashMapInstance, fullMapInstance].filter(Boolean);

  maps.forEach(map => {
    map.eachLayer(layer => {
      if (layer instanceof L.Marker || layer instanceof L.Polyline) {
        map.removeLayer(layer);
      }
    });

    const depotIcon = L.divIcon({
      className: "custom-depot-pin",
      html: `<div style="background:#582c8b; color:white; border-radius:50%; width:26px; height:26px; display:flex; align-items:center; justify-content:center; font-weight:bold;">★</div>`,
      iconSize: [26, 26]
    });
    L.marker(DEPOT_COORDS, { icon: depotIcon }).addTo(map).bindPopup("<b>Municipal Central Depot</b>");

    allIssues.forEach(c => {
      const priorityLabel = c.backendPriority || "Medium";
      const color =
        priorityLabel === "URGENT" || priorityLabel === "Critical" ? "#cb514b" :
        priorityLabel === "High" ? "#b97a21" :
        priorityLabel === "Medium" ? "#4e7fb6" : "#42845a";

      const pinIcon = L.divIcon({
        className: "custom-issue-pin",
        html: `<div style="background:${color}; color:white; border-radius:50%; width:22px; height:22px; display:flex; align-items:center; justify-content:center; font-size:10px; font-weight:bold; border:2px solid white;">${c.id}</div>`,
        iconSize: [22, 22]
      });

      L.marker([c.latitude, c.longitude], { icon: pinIcon }).addTo(map).bindPopup(`
        <div style="max-width:180px;">
          <img src="${c.photoUrl}" style="width:100%; height:75px; object-fit:cover; border-radius:4px;">
          <b>#CP-${c.id}: ${escapeText(c.type)}</b><br>
          <small>By: ${escapeText(c.citizenName)}</small><br>
          <span class="pill ${priorityLabel.toLowerCase()}">${priorityLabel} (${c.backendPriorityScore})</span>
        </div>
      `);
    });
  });
}

async function requestDispatchRoute() {
  showToast("Computing Dijkstra shortest path via C++ Graph...");
  try {
    const res = await fetch("/api/dispatch-route");
    if (!res.ok) throw new Error("API error");
    const data = await res.json();

    document.getElementById("dispatchCard").classList.remove("hidden");
    document.getElementById("routeHub").textContent = data.hub || "Municipal Central Depot";
    document.getElementById("routeWaypoints").textContent = (data.waypoints || []).join(" ➔ ");
    document.getElementById("routeDistance").textContent = (data.totalDistanceKm || 0) + " km";
    document.getElementById("routeTime").textContent = (data.estimatedMinutes || 0) + " mins";

    if (fullMapInstance) {
      if (routePolyline) fullMapInstance.removeLayer(routePolyline);
      const openIssues = allIssues.filter(x => x.status !== "Resolved" && x.status !== "Closed");
      const routePoints = [DEPOT_COORDS, ...openIssues.slice(0, 4).map(c => [c.latitude, c.longitude])];

      routePolyline = L.polyline(routePoints, { color: "#17613a", weight: 4, dashArray: "8, 8" }).addTo(fullMapInstance);
      fullMapInstance.fitBounds(routePolyline.getBounds(), { padding: [40, 40] });
    }

    showToast(`✓ Dijkstra optimal route: ${data.totalDistanceKm} km`);
  } catch (err) {
    console.debug("Dijkstra route error:", err);
  }
}

// ---------- FDS METRICS & BST TESTER ----------

async function loadFdsMetrics() {
  try {
    const res = await fetch("/api/fds-metrics");
    if (!res.ok) throw new Error("Metrics error");
    const m = await res.json();

    document.getElementById("fdsHeapCount").textContent = m.maxHeap.elements;
    document.getElementById("fdsHeapHeight").textContent = m.maxHeap.treeDepth;
    document.getElementById("fdsBstCount").textContent = m.bst.elements;
    document.getElementById("fdsBstHeight").textContent = m.bst.treeHeight;
    document.getElementById("fdsVertices").textContent = m.graph.vertexCount;
    document.getElementById("fdsEdges").textContent = m.graph.edgeCount;
    document.getElementById("fdsAuditCount").textContent = m.doublyLinkedList.recordsCount;

    const openList = allIssues.filter(x => x.status !== "Resolved" && x.status !== "Closed");
    const heapVisual = openList.map((c, idx) => `[${idx}] #CP-${c.id} (P:${c.backendPriorityScore})`).join("  ➔  ");
    document.getElementById("heapArrayView").innerHTML = `<code>${heapVisual || "Max-Heap is currently clear"}</code>`;
  } catch (err) {
    console.debug("Metrics error:", err);
  }

  loadAuditLog();
}

async function testBstSearch() {
  const searchId = document.getElementById("bstTestId").value;
  if (!searchId) {
    showToast("Enter a Complaint ID to test BST lookup.");
    return;
  }

  try {
    const res = await fetch(`/api/complaints/${searchId}`);
    const outBox = document.getElementById("bstSearchResult");

    if (res.ok) {
      const data = await res.json();
      outBox.innerHTML = `
        <span class="pill-badge pill-green">✓ BST Hit in ${data.fdsLookup.comparisons} comparisons</span><br>
        <b>#CP-${data.id}:</b> ${data.type} (Reported by: ${data.citizenName})
      `;
    } else {
      outBox.innerHTML = `<span class="pill-badge pill-yellow">✗ Miss</span>: Complaint #${searchId} not found in BST.`;
    }
  } catch (err) {
    console.debug("BST search test error:", err);
  }
}

async function loadAuditLog() {
  try {
    const res = await fetch("/api/audit-log");
    if (!res.ok) throw new Error("Audit log error");
    const list = await res.json();
    const container = document.getElementById("auditLogList");
    if (!container) return;

    if (list.length === 0) {
      container.innerHTML = `<p class="muted small">No audit actions recorded yet.</p>`;
      return;
    }

    container.innerHTML = list.slice(0, 10).map(item => `
      <div class="audit-item">
        <div class="audit-item-header">
          <strong>#CP-${item.complaintId}: ${escapeText(item.action)}</strong>
          <small class="muted">${new Date(item.timestamp * 1000).toLocaleTimeString()}</small>
        </div>
        <p class="small">${escapeText(item.previousStatus || "Start")} ➔ <b>${escapeText(item.newStatus)}</b></p>
        ${item.notes ? `<p class="small muted">"${escapeText(item.notes)}"</p>` : ""}
      </div>
    `).join("");
  } catch (err) {
    console.debug("Audit log error:", err);
  }
}

// ---------- VIEW NAVIGATION ----------

function showView(viewId) {
  document.querySelectorAll(".view").forEach(v => {
    v.classList.toggle("hidden", v.id !== viewId);
  });

  document.querySelectorAll(".nav-links button").forEach(b => {
    b.classList.remove("active");
  });

  const activeBtn = document.getElementById(`nav-${viewId.replace("View", "")}`);
  if (activeBtn) activeBtn.classList.add("active");

  document.querySelectorAll(".mobile-bottom-nav button").forEach(b => b.classList.remove("active"));
  const mobBtn = document.getElementById(`mob-${viewId}`);
  if (mobBtn) mobBtn.classList.add("active");

  const titles = {
    report: "Report an Issue with Geotag",
    myComplaints: "My Complaints",
    dashboard: "Municipal Dashboard",
    citizens: "Registered Citizen Accounts",
    issues: "All City Complaints",
    priority: "Priority Queue (Max-Heap)",
    mapView: "Interactive Map & Dispatch",
    resolved: "Resolved Issues",
    fds: "Fundamentals of Data Structures Inspector"
  };

  document.getElementById("pageTitle").textContent = titles[viewId] || viewId;
  window.scrollTo({ top: 0, behavior: "smooth" });

  if (viewId === "mapView" && fullMapInstance) {
    setTimeout(() => fullMapInstance.invalidateSize(), 200);
  }
  if (viewId === "dashboard" && dashMapInstance) {
    setTimeout(() => dashMapInstance.invalidateSize(), 200);
  }
  if (viewId === "fds") {
    loadFdsMetrics();
  }
}

// ---------- PWA SERVICE WORKER & INSTALL ----------

function registerServiceWorker() {
  if ("serviceWorker" in navigator) {
    navigator.serviceWorker.register("/sw.js")
      .then(() => console.debug("CivicPulse PWA Service Worker Registered"))
      .catch(err => console.debug("SW error:", err));
  }
}

function initPwaInstall() {
  window.addEventListener("beforeinstallprompt", e => {
    e.preventDefault();
    deferredPrompt = e;
    const btn = document.getElementById("pwaInstallBtn");
    if (btn) btn.classList.remove("hidden");
  });
}

function installPwa() {
  if (deferredPrompt) {
    deferredPrompt.prompt();
    deferredPrompt.userChoice.then(() => {
      deferredPrompt = null;
      document.getElementById("pwaInstallBtn")?.classList.add("hidden");
    });
  } else {
    showToast("To install: Tap your phone browser menu (⋮ or Share) and select 'Add to Home screen'.");
  }
}

// ---------- HELPERS ----------

function escapeText(v) {
  return String(v ?? "").replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}

function showToast(msg) {
  const toast = document.getElementById("toast");
  if (!toast) return;
  toast.textContent = msg;
  toast.style.display = "block";
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => {
    toast.style.display = "none";
  }, 3200);
}

function openPhotoViewer(url) {
  window.open(url, "_blank");
}
